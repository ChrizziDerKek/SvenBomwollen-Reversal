using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using System.Windows.Controls;

namespace Editor
{
    internal static class Helpers
    {
        public static bool GridCellToRaw(int cx, int cy, out int rawX, out int rawY)
        {
            rawX = -1;
            rawY = -1;
            if (cx < 0 || cx >= Constants.VisibleTileWidth || cy < 0 || cy >= Constants.TileHeight)
                return false;
            int rawIndex = Constants.CollisionStartIndex + cy * Constants.CollisionStride + cx;
            if (rawIndex < 0 || rawIndex >= Constants.TileCount)
                return false;
            rawX = rawIndex % Constants.TileWidth;
            rawY = rawIndex / Constants.TileWidth;
            return true;
        }

        public static bool RawToGridCell(int rawX, int rawY, out int cx, out int cy)
        {
            cx = -1;
            cy = -1;
            int rawIndex = rawY * Constants.TileWidth + rawX;
            int linear = rawIndex - Constants.CollisionStartIndex;
            if (linear < 0)
                return false;
            int slot = linear % Constants.CollisionStride;
            int row = linear / Constants.CollisionStride;
            if (slot < 0 || slot >= Constants.VisibleTileWidth || row < 0 || row >= Constants.TileHeight)
                return false;
            cx = slot;
            cy = row;
            return true;
        }

        public static bool PointToCell(Point point, out int cx, out int cy)
        {
            cx = (int)Math.Floor(point.X / Constants.CellWidth);
            cy = (int)Math.Floor(point.Y / Constants.CellHeight);
            return cx >= 0 && cx < Constants.VisibleTileWidth && cy >= 0 && cy < Constants.TileHeight;
        }

        public static bool PointToRawTile(Point point, out int rawX, out int rawY)
        {
            rawX = -1;
            rawY = -1;
            if (!PointToCell(point, out int cx, out int cy))
                return false;
            return GridCellToRaw(cx, cy, out rawX, out rawY);
        }

        public static Rect ObjectRect(ObjectRecord obj)
        {
            double x = obj.GetFloat(1);
            double y = obj.GetFloat(2);
            double w = Math.Max(1, obj.GetInt(6));
            double h = Math.Max(1, obj.GetInt(7));
            return new Rect(x, y, w, h);
        }

        public static string ObjectKindName(int kind)
        {
            return kind switch
            {
                -1 => "Inactive",
                0 => "Sven",
                1 => "Static",
                2 => "Sheep",
                3 => "Alien",
                11 => "Chicken",
                12 => "Goat",
                13 => "Pig",
                4 => "Dog/Wotan",
                5 => "Farmer/Lars",
                6 => "Girl/Bromse",
                7 => "Beer",
                8 => "Teddy",
                9 => "Bone",
                10 => "Artifact",
                _ => $"Unknown {kind}",
            };
        }

        public static bool IsVisibleObjectKind(int kind) => kind != -1;

        public static bool IsActorKind(int kind) => kind is >= 2 and <= 13;

        public static Brush TileBrush(int type)
        {
            return type switch
            {
                1 => new SolidColorBrush(Color.FromArgb(110, 255, 40, 30)),
                2 => new SolidColorBrush(Color.FromArgb(100, 190, 70, 255)),
                3 => new SolidColorBrush(Color.FromArgb(130, 255, 170, 0)),
                4 => new SolidColorBrush(Color.FromArgb(110, 255, 70, 70)),
                5 => new SolidColorBrush(Color.FromArgb(110, 255, 120, 70)),
                6 => new SolidColorBrush(Color.FromArgb(110, 255, 70, 180)),
                10 => new SolidColorBrush(Color.FromArgb(110, 70, 170, 255)),
                11 => new SolidColorBrush(Color.FromArgb(130, 10, 70, 210)),
                _ => new SolidColorBrush(Color.FromArgb(110, 220, 220, 220)),
            };
        }

        public static Brush ObjectMarkerBrush(int kind)
        {
            return kind switch
            {
                0 => Brushes.DeepSkyBlue,
                2 => Brushes.MediumPurple,
                4 => Brushes.IndianRed,
                5 => Brushes.OrangeRed,
                6 => Brushes.HotPink,
                7 or 8 or 9 or 10 => Brushes.Gold,
                _ => Brushes.White,
            };
        }

        public static TextBlock OverlayText(string text, double fontSize = 11)
        {
            return new TextBlock
            {
                Text = text,
                FontFamily = new FontFamily("Consolas"),
                FontSize = fontSize,
                Foreground = Brushes.White,
                Background = new SolidColorBrush(Color.FromArgb(150, 0, 0, 0)),
                IsHitTestVisible = false,
            };
        }

        public static BitmapSource LoadImage(string path)
        {
            BitmapImage bmp = new();
            bmp.BeginInit();
            bmp.CacheOption = BitmapCacheOption.OnLoad;
            bmp.UriSource = new Uri(path, UriKind.Absolute);
            bmp.EndInit();
            bmp.Freeze();
            return bmp;
        }

        public static BitmapSource LoadChromaImage(string path)
        {
            BitmapSource source = LoadImage(path);
            FormatConvertedBitmap converted = new(source, PixelFormats.Bgra32, null, 0);
            int stride = converted.PixelWidth * 4;
            byte[] pixels = new byte[stride * converted.PixelHeight];
            converted.CopyPixels(pixels, stride, 0);
            for (int i = 0; i < pixels.Length; i += 4)
            {
                byte b = pixels[i];
                byte g = pixels[i + 1];
                byte r = pixels[i + 2];
                if (r >= Constants.ChromaR && g <= Constants.ChromaG && b >= Constants.ChromaB)
                    pixels[i + 3] = 0;
            }
            WriteableBitmap output = new(converted.PixelWidth, converted.PixelHeight, 96, 96, PixelFormats.Bgra32, null);
            output.WritePixels(new Int32Rect(0, 0, converted.PixelWidth, converted.PixelHeight), pixels, stride, 0);
            output.Freeze();
            return output;
        }
    }
}
