using System.IO;
using System.Xml.Linq;

namespace Editor;

internal sealed record PackLevelEntry(string LevelSet, string Name, string Tileset, string Music, string Map, string FullPath)
{
    public override string ToString() => $"{LevelSet} / {Name} ({Tileset})";
}

internal sealed class PackProject
{
    public string RootFolder { get; }
    public string? XmlPath { get; private set; }
    public string? ArchivePath { get; private set; }
    public MudgeArchive? Archive { get; private set; }
    public string SignatureStatus { get; private set; } = "No archive signature (extracted folder).";
    public XDocument? Document { get; private set; }
    public List<PackLevelEntry> Levels { get; } = new();
    private PackProject(string folder) => RootFolder = folder;

    public static PackProject LoadFromExtractedFolder(string folder)
    {
        PackProject project = new(Path.GetFullPath(folder));
        foreach (string xml in Directory.EnumerateFiles(folder, "*.xml", SearchOption.AllDirectories).OrderBy(x => x.Length))
        {
            XDocument doc;
            try { doc = XDocument.Load(xml, LoadOptions.PreserveWhitespace); }
            catch (System.Xml.XmlException) { continue; }
            if (doc.Root?.Name != "sven2" || doc.Root.Element("package") == null) continue;
            project.XmlPath = xml;
            project.Document = doc;
            TextureCatalog.LoadTilesetsFromXml(xml);
            project.RefreshLevels();
            break;
        }
        return project;
    }

    public static PackProject OpenArchive(string path)
    {
        MudgeArchive archive = MudgeArchive.Read(path);
        string workspace = Path.Combine(Path.GetTempPath(), "SvenEditor", Guid.NewGuid().ToString("N"));
        archive.Extract(workspace);
        PackProject project = LoadFromExtractedFolder(workspace);
        project.Archive = archive;
        project.ArchivePath = Path.GetFullPath(path);
        project.SignatureStatus = PackSignature.Verify(File.ReadAllBytes(path));
        return project;
    }

    public void RefreshLevels()
    {
        Levels.Clear();
        if (Document == null) return;
        foreach (XElement level in Document.Descendants("level"))
        {
            string map = (string?)level.Attribute("map") ?? "";
            if (string.IsNullOrWhiteSpace(map)) continue;
            string full = ResolveMap(map);
            Levels.Add(new((string?)level.Ancestors("levelset").FirstOrDefault()?.Attribute("name") ?? "",
                (string?)level.Attribute("name") ?? Path.GetFileNameWithoutExtension(map),
                (string?)level.Attribute("tileset") ?? "Download", (string?)level.Attribute("music") ?? "", map, full));
        }
    }

    private string ResolveMap(string map)
    {
        string full = MudgeArchive.SafePath(RootFolder, map);
        string rootName = Path.GetFileName(RootFolder.TrimEnd(Path.DirectorySeparatorChar));
        string normalized = map.Replace('\\', '/');
        if (!File.Exists(full) && normalized.StartsWith(rootName + "/", StringComparison.OrdinalIgnoreCase))
            full = MudgeArchive.SafePath(RootFolder, normalized[(rootName.Length + 1)..]);
        return full;
    }

    public PackLevelEntry AddLevel(LevelFile level, string setName, string name, string tileset, string music)
    {
        if (Document == null || XmlPath == null) throw new InvalidOperationException("Open a pack with a package XML first.");
        if (string.IsNullOrWhiteSpace(setName) || string.IsNullOrWhiteSpace(name)) throw new InvalidOperationException("Level and levelset names are required.");
        XElement package = Document.Root!.Element("package")!;
        XElement? sets = package.Element("levelsets");
        if (sets == null) { sets = new XElement("levelsets"); package.Add(sets); }
        XElement? set = sets.Elements("levelset").FirstOrDefault(e => (string?)e.Attribute("name") == setName);
        if (set?.Elements("level").Any(e => (string?)e.Attribute("name") == name) == true)
            throw new InvalidOperationException("A level with this name already exists in that levelset.");
        string root = (string?)package.Attribute("name") ?? throw new InvalidDataException("Package has no name.");
        string map = root + "/levels/custom_" + Guid.NewGuid().ToString("N") + ".lvl";
        string full = ResolveMap(map);
        Directory.CreateDirectory(Path.GetDirectoryName(full)!);
        File.WriteAllBytes(full, level.Data);
        if (set == null) { set = new XElement("levelset", new XAttribute("name", setName)); sets.Add(set); }
        set.Add(new XElement("level", new XAttribute("name", name), new XAttribute("tileset", tileset), new XAttribute("music", music), new XAttribute("map", map)));
        Document.Save(XmlPath);
        RefreshLevels();
        return Levels.Single(e => e.Map == map);
    }

    public PackLevelEntry UpdateLevelMetadata(PackLevelEntry entry, string tileset, string? music)
    {
        if (Document == null || XmlPath == null) return entry;
        XElement level = Document.Descendants("level").First(e => (string?)e.Attribute("map") == entry.Map
            && (string?)e.Attribute("name") == entry.Name && (string?)e.Ancestors("levelset").FirstOrDefault()?.Attribute("name") == entry.LevelSet);
        string selectedMusic = string.IsNullOrWhiteSpace(music) ? entry.Music : music.Trim();
        if ((string?)level.Attribute("tileset") == tileset && ((string?)level.Attribute("music") ?? "") == selectedMusic) return entry;
        level.SetAttributeValue("tileset", tileset); level.SetAttributeValue("music", selectedMusic);
        Document.Save(XmlPath); RefreshLevels();
        return Levels.First(e => e.Map == entry.Map && e.Name == entry.Name && e.LevelSet == entry.LevelSet);
    }

    public PackProject SaveArchive(string path, int? exportVersion = null)
    {
        if (Archive == null) throw new InvalidOperationException("Use New Pack from this project's template first.");
        foreach (string file in Directory.EnumerateFiles(RootFolder, "*", SearchOption.AllDirectories))
            Archive.SetFile(Path.GetRelativePath(RootFolder, file).Replace('\\', '/'), File.ReadAllBytes(file));
        MudgeArchive exportArchive = Archive.CopyWithRootName(Archive.Root.Name);
        if (exportVersion != null)
            foreach (var entry in exportArchive.Entries().Where(e => !e.Node.Directory && e.Path.EndsWith(".lvl", StringComparison.OrdinalIgnoreCase)))
                entry.Node.Data = LevelFile.ExportBytes(entry.Node.Data!, exportVersion);
        string name = Path.GetFileNameWithoutExtension(path);
        if (string.Equals(name, Archive.Root.Name, StringComparison.Ordinal))
        {
            exportArchive.Save(path); ArchivePath = Path.GetFullPath(path);
            SignatureStatus = PackSignature.Verify(File.ReadAllBytes(path)); return this;
        }
        if (Document == null || XmlPath == null) throw new InvalidOperationException("Renaming requires package XML.");
        string oldName = Archive.Root.Name;
        MudgeArchive renamed = exportArchive.CopyWithRootName(name);
        string descriptor = Path.GetRelativePath(RootFolder, XmlPath).Replace('\\', '/');
        if (!descriptor.StartsWith(oldName + "/", StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("Package descriptor is outside the archive root.");
        string renamedDescriptor = name + descriptor[oldName.Length..];
        var node = renamed.Entries().Single(e => e.Path.Equals(renamedDescriptor, StringComparison.OrdinalIgnoreCase)).Node;
        // The engine opens <package>/<package>.xml after mounting a package.
        if (node.Name != name + ".xml") { node.Name = name + ".xml"; node.Hash = 0; }
        foreach (var entry in renamed.Entries().Where(e => !e.Node.Directory && e.Path.EndsWith(".xml", StringComparison.OrdinalIgnoreCase)).ToList())
        {
            XDocument doc;
            if (ReferenceEquals(entry.Node, node)) doc = new XDocument(Document);
            else
            {
                try { using MemoryStream input = new(entry.Node.Data!); doc = XDocument.Load(input, LoadOptions.PreserveWhitespace); }
                catch (System.Xml.XmlException) { continue; }
            }
            bool changed = ReferenceEquals(entry.Node, node);
            if (changed) doc.Root!.Element("package")!.SetAttributeValue("name", name);
            foreach (var attr in doc.Descendants().Attributes())
            {
                string normalized = attr.Value.Replace('\\', '/');
                if (!normalized.StartsWith(oldName + "/", StringComparison.OrdinalIgnoreCase)) continue;
                attr.Value = name + normalized[oldName.Length..]; changed = true;
            }
            if (!changed) continue;
            using MemoryStream output = new(); doc.Save(output); entry.Node.Data = output.ToArray();
        }
        renamed.Save(path);
        return OpenArchive(path);
    }

    // Clone a working package including its resources and dependency metadata.
    // The archive filename/root/package name stay aligned, as required by the game.
    public static PackProject CreateFromTemplate(PackProject template, string path)
    {
        if (template.Document == null || template.XmlPath == null) throw new InvalidOperationException("Template requires package XML.");
        string name = Path.GetFileNameWithoutExtension(path);
        MudgeArchive archive = MudgeArchive.Create(name);
        XDocument doc = new(template.Document);
        XElement package = doc.Root!.Element("package")!;
        string oldName = (string?)package.Attribute("name") ?? "";
        package.SetAttributeValue("name", name);
        package.Element("levelsets")?.Remove();
        package.Element("Signature")?.Remove();
        package.AddFirst(new XElement("levelsets"));
        foreach (XAttribute attr in doc.Descendants().Attributes().Where(a => a.Value.StartsWith(oldName + "/", StringComparison.OrdinalIgnoreCase)))
            attr.Value = name + attr.Value[oldName.Length..];
        foreach (string file in Directory.EnumerateFiles(template.RootFolder, "*", SearchOption.AllDirectories))
        {
            if (Path.GetFullPath(file) == Path.GetFullPath(template.XmlPath) || Path.GetExtension(file).Equals(".lvl", StringComparison.OrdinalIgnoreCase)) continue;
            string rel = Path.GetRelativePath(template.RootFolder, file).Replace('\\', '/');
            if (rel.StartsWith(oldName + "/", StringComparison.OrdinalIgnoreCase)) rel = rel[(oldName.Length + 1)..];
            archive.SetFile(name + "/" + rel, File.ReadAllBytes(file));
        }
        using MemoryStream xml = new(); doc.Save(xml);
        archive.SetFile(name + "/" + name + ".xml", xml.ToArray());
        archive.Save(path);
        return OpenArchive(path);
    }
}
