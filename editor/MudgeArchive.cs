using System.IO;
using System.Text;

namespace Editor;

// Keeps the original tree, name hashes, flags, header and trailing bytes.
// Editing a payload must not normalize metadata belonging to unrelated entries.
internal sealed class MudgeArchive
{
    internal sealed class Node
    {
        public string Name = "";
        public uint Hash, Flags = 1, Unknown;
        public byte[]? Data;
        public List<Node> Children = new();
        public bool Directory => Data == null;
    }

    public Node Root { get; private set; } = new();
    private byte[] Header = new byte[64], Separator = new byte[17], Trailer = Array.Empty<byte>();

    public static MudgeArchive Read(string path)
    {
        byte[] bytes = File.ReadAllBytes(path);
        if (bytes.Length < 81 || Encoding.ASCII.GetString(bytes, 0, 8) != "MUDGE4.0")
            throw new InvalidDataException("Not a MUDGE4.0 archive.");
        uint end = BitConverter.ToUInt32(bytes, 56);
        if (end < 64 || (ulong)end + 17 >= (ulong)bytes.Length)
            throw new InvalidDataException("Invalid archive directory offset.");
        MudgeArchive archive = new() { Header = bytes[..64], Separator = bytes[(int)end..((int)end + 17)] };
        using MemoryStream stream = new(bytes);
        using BinaryReader reader = new(stream, Encoding.Latin1);
        stream.Position = end + 17;
        int count = 0;
        Node ReadNode(int depth)
        {
            if (depth > 64 || ++count > 100000) throw new InvalidDataException("Archive tree exceeds limits.");
            byte type = reader.ReadByte();
            uint hash = reader.ReadUInt32(), length = reader.ReadUInt32();
            if (length == 0 || length > 4096 || length > stream.Length - stream.Position)
                throw new InvalidDataException("Invalid archive name length.");
            string name = Encoding.Latin1.GetString(reader.ReadBytes((int)length));
            ValidateName(name);
            Node node = new() { Name = name, Hash = hash };
            if (type == 1)
            {
                uint children = reader.ReadUInt32();
                if (children > 100000) throw new InvalidDataException("Invalid archive child count.");
                for (int i = 0; i < children; i++) node.Children.Add(ReadNode(depth + 1));
                if (node.Children.Select(n => n.Name).Distinct(StringComparer.OrdinalIgnoreCase).Count() != node.Children.Count)
                    throw new InvalidDataException("Duplicate archive entry names.");
            }
            else if (type == 2)
            {
                node.Flags = reader.ReadUInt32();
                uint offset = reader.ReadUInt32() ^ 0xFFAA5533, size = reader.ReadUInt32() ^ 0x3355AAFF;
                node.Unknown = reader.ReadUInt32();
                if (offset < 64 || (ulong)offset + size > end) throw new InvalidDataException("Payload outside archive data.");
                node.Data = bytes.AsSpan((int)offset, checked((int)size)).ToArray();
                for (int i = 0; i < node.Data.Length; i++) node.Data[i] ^= 0x88;
            }
            else throw new InvalidDataException($"Unknown archive node type {type}.");
            return node;
        }
        archive.Root = ReadNode(0);
        if (!archive.Root.Directory) throw new InvalidDataException("Archive root is not a directory.");
        archive.Trailer = bytes[(int)stream.Position..];
        return archive;
    }

    public static MudgeArchive Create(string rootName)
    {
        ValidateName(rootName);
        MudgeArchive archive = new() { Root = new Node { Name = rootName } };
        byte[] text = Encoding.ASCII.GetBytes("MUDGE4.0" + DateTime.Now.ToString("MM/dd/yyyy") + "Copyright by mudGE Entertainment");
        text.CopyTo(archive.Header, 0);
        archive.Header[54] = 0x25; archive.Header[55] = 0x6D;
        archive.Separator[4] = 1; archive.Separator[13] = 1;
        return archive;
    }

    public MudgeArchive CopyWithRootName(string name)
    {
        ValidateName(name);
        Node Clone(Node node) => new()
        {
            Name = node.Name, Hash = node.Hash, Flags = node.Flags, Unknown = node.Unknown,
            Data = node.Data == null ? null : (byte[])node.Data.Clone(),
            Children = node.Children.Select(Clone).ToList()
        };
        MudgeArchive copy = new()
        {
            Root = Clone(Root), Header = (byte[])Header.Clone(),
            Separator = (byte[])Separator.Clone(), Trailer = (byte[])Trailer.Clone()
        };
        copy.Root.Name = name; copy.Root.Hash = 0;
        return copy;
    }

    public IEnumerable<(string Path, Node Node)> Entries()
    {
        IEnumerable<(string, Node)> Walk(Node node, string path)
        {
            yield return (path, node);
            foreach (Node child in node.Children)
                foreach (var entry in Walk(child, path + "/" + child.Name)) yield return entry;
        }
        return Walk(Root, Root.Name);
    }

    public void SetFile(string path, byte[] data)
    {
        string[] parts = path.Replace('\\', '/').Split('/');
        foreach (string part in parts) ValidateName(part);
        if (parts.Length < 2 || !parts[0].Equals(Root.Name, StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("File must be inside the archive root.");
        Node current = Root;
        for (int i = 1; i < parts.Length; i++)
        {
            Node? child = current.Children.FirstOrDefault(n => n.Name.Equals(parts[i], StringComparison.OrdinalIgnoreCase));
            if (child == null) { child = new Node { Name = parts[i] }; current.Children.Add(child); }
            if (i == parts.Length - 1)
            {
                if (child.Directory && child.Children.Count > 0) throw new InvalidDataException("Cannot replace a directory with a file.");
                child.Data = (byte[])data.Clone();
            }
            else if (!child.Directory) throw new InvalidDataException("File used as a directory.");
            current = child;
        }
    }

    public void Extract(string folder)
    {
        foreach (var entry in Entries())
        {
            string path = SafePath(folder, entry.Path);
            if (entry.Node.Directory) System.IO.Directory.CreateDirectory(path);
            else
            {
                System.IO.Directory.CreateDirectory(System.IO.Path.GetDirectoryName(path)!);
                File.WriteAllBytes(path, entry.Node.Data!);
            }
        }
    }

    public void Save(string path)
    {
        using MemoryStream stream = new();
        using BinaryWriter writer = new(stream, Encoding.Latin1, true);
        writer.Write(Header);
        Dictionary<Node, (uint Offset, uint Size)> locations = new();
        foreach (var entry in Entries().Where(e => !e.Node.Directory))
        {
            byte[] bytes = (byte[])entry.Node.Data!.Clone();
            locations[entry.Node] = (checked((uint)stream.Position), (uint)bytes.Length);
            for (int i = 0; i < bytes.Length; i++) bytes[i] ^= 0x88;
            writer.Write(bytes);
        }
        uint end = checked((uint)stream.Position);
        writer.Write(Separator);
        void WriteNode(Node node)
        {
            writer.Write((byte)(node.Directory ? 1 : 2)); writer.Write(node.Hash);
            byte[] name = Encoding.Latin1.GetBytes(node.Name);
            writer.Write((uint)name.Length); writer.Write(name);
            if (node.Directory)
            {
                writer.Write((uint)node.Children.Count);
                foreach (Node child in node.Children) WriteNode(child);
            }
            else
            {
                writer.Write(node.Flags); writer.Write(locations[node].Offset ^ 0xFFAA5533);
                writer.Write(locations[node].Size ^ 0x3355AAFF); writer.Write(node.Unknown);
            }
        }
        WriteNode(Root); writer.Write(Trailer);
        stream.Position = 56; writer.Write(end); writer.Write(end + 4);
        string full = System.IO.Path.GetFullPath(path);
        string temporary = full + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllBytes(temporary, stream.ToArray());
            MudgeArchive check = Read(temporary);
            var expected = Entries().Where(e => !e.Node.Directory).ToDictionary(e => e.Path, e => e.Node.Data!);
            foreach (var entry in check.Entries().Where(e => !e.Node.Directory))
                if (!expected.Remove(entry.Path, out byte[]? data) || !data.SequenceEqual(entry.Node.Data!))
                    throw new InvalidDataException("Archive verification failed.");
            if (expected.Count != 0) throw new InvalidDataException("Archive lost entries.");
            if (File.Exists(full)) File.Replace(temporary, full, full + ".bak", true);
            else File.Move(temporary, full);
        }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }

    public static string SafePath(string folder, string relative)
    {
        string[] parts = relative.Replace('\\', '/').Split('/');
        foreach (string part in parts) ValidateName(part);
        string root = System.IO.Path.GetFullPath(folder).TrimEnd('\\', '/') + System.IO.Path.DirectorySeparatorChar;
        string full = System.IO.Path.GetFullPath(System.IO.Path.Combine(root, System.IO.Path.Combine(parts)));
        if (!full.StartsWith(root, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Path outside workspace.");
        return full;
    }

    private static void ValidateName(string name)
    {
        if (string.IsNullOrWhiteSpace(name) || name is "." or ".." || name.IndexOfAny(System.IO.Path.GetInvalidFileNameChars()) >= 0 || name.Contains('/') || name.Contains('\\') || name.EndsWith('.') || name.EndsWith(' '))
            throw new InvalidDataException($"Unsafe archive name: {name}");
    }
}
