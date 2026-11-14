using System.IO;
using System.Windows.Media;

namespace Editor
{
    internal sealed class AssetManager
    {
        private readonly Dictionary<string, string> Files = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, ImageSource?> ImageCache = new(StringComparer.OrdinalIgnoreCase);
        public string? RootFolder { get; private set; }

        public void Clear()
        {
            RootFolder = null;
            Files.Clear();
            ImageCache.Clear();
        }

        public void LoadFolder(string folder)
        {
            Clear();
            RootFolder = folder;
            AddFolder(folder);
        }

        public void AddFolder(string folder)
        {
            ImageCache.Clear();
            foreach (string file in Directory.EnumerateFiles(folder, "*.*", SearchOption.AllDirectories))
            {
                string ext = Path.GetExtension(file).ToLowerInvariant();
                if (ext is not (".bmp" or ".png" or ".jpg" or ".jpeg"))
                    continue;
                string rel = Path.GetRelativePath(folder, file).Replace('\\', '/');
                Files[rel] = file;
                Files[Path.GetFileName(file)] = file;
            }
        }

        public bool TryFind(string logicalPath, out string path)
        {
            string normalized = TextureCatalog.Normalize(logicalPath);
            if (Files.TryGetValue(normalized, out path!))
                return true;
            if (Files.TryGetValue(Path.GetFileName(normalized), out path!))
                return true;
            path = "";
            return false;
        }

        public ImageSource? GetImage(string logicalPath, bool chromaKey)
        {
            string key = (chromaKey ? "ck:" : "bg:") + logicalPath;
            if (ImageCache.TryGetValue(key, out ImageSource? image))
                return image;
            if (!TryFind(logicalPath, out string path))
            {
                ImageCache[key] = null;
                return null;
            }
            try
            {
                image = chromaKey ? Helpers.LoadChromaImage(path) : Helpers.LoadImage(path);
                ImageCache[key] = image;
                return image;
            }
            catch
            {
                ImageCache[key] = null;
                return null;
            }
        }

        public bool TryGetImagePixelSize(string logicalPath, out int width, out int height)
        {
            width = 0;
            height = 0;

            if (!TryFind(logicalPath, out string path))
                return false;

            try
            {
                System.Windows.Media.Imaging.BitmapSource image = Helpers.LoadImage(path);
                width = image.PixelWidth;
                height = image.PixelHeight;
                return width > 0 && height > 0;
            }
            catch
            {
                width = 0;
                height = 0;
                return false;
            }
        }
    }
}
