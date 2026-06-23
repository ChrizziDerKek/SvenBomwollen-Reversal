using System.Runtime.InteropServices;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace Editor
{
    static class Helpers
    {
        public static void EnsureValue(int value, int min, int max)
        {
            if (value < min || value >= max)
                throw new ArgumentOutOfRangeException(nameof(value), string.Format("Field {0} out of range {1}..{2}", value, min, max - 1));
        }

        public static void Write<T>(byte[] stream, int offset, T value)
        {
            byte[] serialized = (byte[])typeof(BitConverter).GetMethod("GetBytes", [typeof(T)])!.Invoke(null, [value])!;
            Buffer.BlockCopy(serialized, 0, stream, offset, Marshal.SizeOf<T>());
        }

        public static int Mod(int value, int modulo)
        {
            int remainder = value % modulo;
            return remainder < 0 ? remainder + modulo : remainder;
        }

        public static int RawToCx25(int rawX, int rawY) => Mod(rawX + 3 * rawY + 2, Constants.TileWidth);

        public static int Cx25ToRawX(int cx25, int cy) => Mod(cx25 - 3 * cy - 2, Constants.TileWidth);

        public static int VisibleToRawX(int cx, int cy) => Cx25ToRawX(cx, cy);

        public static int HiddenToRawX(int hx, int hy) => Cx25ToRawX(Constants.VisibleTileWidth + hx, hy);

        public static bool CoordsToGridCell(int rawX, int rawY, out int cx, out int cy)
        {
            cx = -1;
            cy = -1;
            int index = rawY * Constants.TileWidth + rawX;
            int linear = index - Constants.CollisionStartIndex;
            if (linear < 0)
                return false;
            int slot = linear % Constants.CollisionStride;
            cy = linear / Constants.CollisionStride;
            if (cy < 0 || cy >= Constants.TileHeight || slot < 0 || slot >= Constants.VisibleTileWidth)
                return false;
            cx = slot;
            return true;
        }

        public static bool GridCellToCoords(int cx, int cy, out int rawX, out int rawY)
        {
            rawX = -1;
            rawY = -1;
            if (cx < 0 || cx >= Constants.VisibleTileWidth || cy < 0 || cy >= Constants.TileHeight)
                return false;
            int index = Constants.CollisionStartIndex + cy * Constants.CollisionStride + cx;
            if (index < 0 || index >= Constants.TileCount)
                return false;
            rawX = index % Constants.TileWidth;
            rawY = index / Constants.TileWidth;
            return true;
        }

        public static bool IsGameplayTile(int tileType) => tileType != 0;

        public static bool IsAirTile(int tileType) => !IsGameplayTile(tileType);

        public static bool IsVisibleCanvasCell(float px, float py, out int cx, out int cy)
        {
            if (px >= Constants.VisibleOriginX && px < Constants.VisibleOriginX + Constants.VisibleTileWidth * Constants.CellWidth && py >= Constants.VisibleOriginY && py < Constants.VisibleOriginY + Constants.TileHeight * Constants.CellHeight)
            {
                cx = (int)Math.Floor((px - Constants.VisibleOriginX) / Constants.CellWidth);
                cy = (int)Math.Floor((py - Constants.VisibleOriginY) / Constants.CellHeight);
                return true;
            }
            cx = -1;
            cy = -1;
            return false;
        }

        public static bool TryMapTileForDisplay(int tileType, int rawX, int rawY, out int displayX, out int displayY, out bool hidden, out string coordLabel, out int cx25)
        {
            cx25 = RawToCx25(rawX, rawY);
            if (IsGameplayTile(tileType))
            {
                if (CoordsToGridCell(rawX, rawY, out int cx, out int cy))
                {
                    displayX = cx;
                    displayY = cy;
                    hidden = false;
                    coordLabel = string.Format("CX{0},CY{1} (T{2})", cx, cy, tileType);
                    return true;
                }
                displayX = -1;
                displayY = -1;
                hidden = true;
                coordLabel = string.Format("CXX,CYY (T{0})", tileType);
                return false;
            }
            displayY = rawY;
            if (cx25 < Constants.VisibleTileWidth)
            {
                displayX = cx25;
                hidden = false;
                coordLabel = string.Format("CX{0},CY{1}", displayX, displayY);
                return true;
            }
            displayX = cx25 - Constants.VisibleTileWidth;
            hidden = true;
            coordLabel = string.Format("HX{0},HY{1}", displayX, displayY);
            return displayX >= 0 && displayX < Constants.HiddenTileWidth;
        }

        public static bool IsHiddenTile(int rawX, int rawY) => RawToCx25(rawX, rawY) >= Constants.VisibleTileWidth;

        public static float VisibleLeft(int cx) => Constants.VisibleOriginX + cx * Constants.CellWidth;

        public static float VisibleTop(int cy) => Constants.VisibleOriginY + cy * Constants.CellHeight;

        public static float HiddenLeft(int cx) => Constants.HiddenOriginX + cx * Constants.CellWidth;

        public static float HiddenTop(int cy) => Constants.HiddenOriginY + cy * Constants.CellHeight;

        public static ImageSource LoadBitmapWithChromaKey(string path)
        {
            BitmapImage bmp = new();
            bmp.BeginInit();
            bmp.CacheOption = BitmapCacheOption.OnLoad;
            bmp.UriSource = new(path);
            bmp.EndInit();
            bmp.Freeze();
            FormatConvertedBitmap converted = new(bmp, PixelFormats.Bgra32, null, 0);
            int stride = converted.PixelWidth * 4;
            byte[] pixels = new byte[stride * converted.PixelHeight];
            converted.CopyPixels(pixels, stride, 0);
            for (int i = 0; i < pixels.Length; i += 4)
            {
                byte b = pixels[i];
                byte g = pixels[i + 1];
                byte r = pixels[i + 2];
                if (r > Constants.ChromaR && g < Constants.ChromaG && b > Constants.ChromaB)
                    pixels[i + 3] = 0;
            }
            WriteableBitmap wb = new(converted.PixelWidth, converted.PixelHeight, converted.DpiX, converted.DpiY, PixelFormats.Bgra32, null);
            wb.WritePixels(new(0, 0, converted.PixelWidth, converted.PixelHeight), pixels, stride, 0);
            wb.Freeze();
            return wb;
        }

        public static TextBlock MakeOverlayText(string text, int size)
        {
            return new()
            {
                Text = text,
                FontFamily = new("Consolas"),
                FontSize = size,
                Foreground = Brushes.White,
                Background = new SolidColorBrush(Color.FromArgb(120, 0, 0, 0)),
                IsHitTestVisible = false,
            };
        }

        public static (int X, int Y) ObjectAnchorTile(ObjectRecord obj)
        {
            (int CX, int CY) = ObjectAnchorCell(obj);
            return GridCellToCoords(CX, CY, out int rawX, out int rawY) ? (rawX, rawY) : (-1, -1);
        }

        public static (int CX, int CY) ObjectAnchorCell(ObjectRecord obj)
        {
            float x = obj.GetFloat(1);
            float y = obj.GetFloat(2);
            int w = obj.GetInt(6);
            int h = obj.GetInt(7);
            float anchorX = x + w / 2.0f;
            float anchorY = y + h;
            int cx = (int)Math.Floor(anchorX / Constants.CellWidth);
            int cy = (int)Math.Floor(anchorY / Constants.CellHeight);
            cx = Math.Clamp(cx, 0, Constants.VisibleTileWidth - 1);
            cy = Math.Clamp(cy, 0, Constants.TileHeight - 1);
            return (cx, cy);
        }

        public static (int X, int Y) PixelToTile(float px, float py)
        {
            if (IsVisibleCanvasCell(px, py, out int cx, out int cy))
            {
                int rawX = VisibleToRawX(cx, cy);
                return (rawX, cy);
            }
            if (px >= Constants.HiddenOriginX && px < Constants.HiddenOriginX + Constants.HiddenTileWidth * Constants.CellWidth && py >= Constants.HiddenOriginY && py < Constants.HiddenOriginY + Constants.TileHeight * Constants.CellHeight)
            {
                int hx = (int)Math.Floor((px - Constants.HiddenOriginX) / Constants.CellWidth);
                int hy = (int)Math.Floor((py - Constants.HiddenOriginY) / Constants.CellHeight);
                int rawX = HiddenToRawX(hx, hy);
                return (rawX, hy);
            }
            return (-1, -1);
        }

        public static void EnsureStatic(ObjectRecord obj, int index)
        {
            if (obj.GetInt(12) != 1)
                throw new InvalidOperationException(string.Format("object[{0}] is not static scenery. field12={1}", index, obj.GetInt(12)));
        }

        public static string CSV(string s) => s.Replace("\"", "\"\"");
    }
}