using System.Reflection;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace Editor
{
    public partial class MainWindow : Window
    {
        private record ObjectListItem(int Index, string Label)
        {
            public override string ToString() => Label;
        }

        private record Correlation(int ObjectIndex, int TextureIndex, string TextureName, float ObjectX, float ObjectY, int Width, int Height, int AnchorX, int AnchorY, int TileX, int TileY, int OffsetX, int OffsetY, string TileFields);

        private LevelFile? LevelData;
        private string? LevelPath;
        private string? AssetPath;
        private readonly Dictionary<string, string> AssetMap = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<int, ImageSource?> ImageCache = [];
        private int? SelectedObjectIndex;
        private FrameworkElement? DragElement;
        private int? DragObjectIndex;
        private Point DragStartMouse;
        private float DragStartX;
        private float DragStartY;
        private Rectangle? SelectedBorder;

        public MainWindow()
        {
            InitializeComponent();
            Version version = Assembly.GetExecutingAssembly().GetName().Version!;
            Title = string.Format("{0} v{1}.{2}.{3}", AppDomain.CurrentDomain.FriendlyName, version.Major, version.Minor, version.Build);
            MinHeight = Height;
            MinWidth = Width;
            Render();
        }

        private string ObjectTooltip(int index)
        {
            if (LevelData == null)
                return "";
            ObjectRecord obj = LevelData.Object(index);
            int texture = obj.GetInt(18);
            return string.Format("object[{0}]\n{1}\ntex={2}\nxy=({3},{4})\nsize={5}x{6}", index, TextureCatalog.TextureName(texture), texture, obj.GetFloat(1), obj.GetFloat(2), obj.GetInt(6), obj.GetInt(7));
        }

        private ImageSource? GetTextureImage(int index)
        {
            if (ImageCache.TryGetValue(index, out ImageSource? cached))
                return cached;
            string name = TextureCatalog.TextureName(index);
            if (!AssetMap.TryGetValue(name, out string? path))
            {
                ImageCache[index] = null;
                return null;
            }
            try
            {
                ImageSource source = Helpers.LoadBitmapWithChromaKey(path);
                ImageCache[index] = source;
                return source;
            }
            catch { }
            ImageCache[index] = null;
            return null;
        }

        private void RefreshObjectList()
        {
            lbObjects.Items.Clear();
            if (LevelData == null)
                return;
            foreach ((ObjectRecord Obj, int Index) item in LevelData.Objects.Select((o, i) => (Obj: o, Index: i)))
            {
                if (item.Obj.GetInt(12) != 1)
                    continue;
                int tex = item.Obj.GetInt(18);
                string name = TextureCatalog.TextureName(tex);
                lbObjects.Items.Add(new ObjectListItem(item.Index, $"[{item.Index:000}] tex={tex,2} {name}  ({item.Obj.GetFloat(1):0.#},{item.Obj.GetFloat(2):0.#})"));
            }
            if (SelectedObjectIndex is int selected)
            {
                foreach (ObjectListItem obj in lbObjects.Items.OfType<ObjectListItem>())
                {
                    if (obj.Index == selected)
                    {
                        lbObjects.SelectedItem = obj;
                        break;
                    }
                }
            }
        }

        private void SelectObject(int index, bool render = true)
        {
            SelectedObjectIndex = index;
            if (render)
            {
                Render();
                return;
            }
            UpdateSelectedInfo();
            UpdateCorrelationList();
        }

        private void CreateTile(Point position, int type, bool render = true)
        {
            if (LevelData == null)
                return;
            int x;
            int y;
            if ((Helpers.IsGameplayTile(type) || Helpers.IsAirTile(type)) && Helpers.IsVisibleCanvasCell((float)position.X, (float)position.Y, out int cx, out int cy))
            {
                if (!Helpers.GridCellToCoords(cx, cy, out x, out y))
                    return;
            }
            else
            {
                (int tx, int ty) = Helpers.PixelToTile((float)position.X, (float)position.Y);
                x = tx;
                y = ty;
            }
            if (x < 0 || x >= Constants.TileWidth || y < 0 || y >= Constants.TileHeight)
                return;
            TileRecord tile = LevelData.Tile(x, y);
            tile.SetInt(0, type);
            for (int i = 1; i <= 5; i++)
                tile.SetInt(i, 0);
            if (render)
                Render();
        }

        private int GetSelectedTileType()
        {
            if (cbPaintType.SelectedItem is ComboBoxItem item && item.Content is string text)
            {
                string first = text.Split(' ', StringSplitOptions.RemoveEmptyEntries).FirstOrDefault() ?? "1";
                first = first.TrimStart('0');
                if (string.IsNullOrEmpty(first))
                    first = "0";
                if (int.TryParse(first, out int value))
                    return value;
            }
            return 1;
        }

        private void MoveDrawOrder(int direction)
        {
            if (LevelData == null || SelectedObjectIndex is not int index)
                return;
            int other = direction < 0 ? LevelData.FindPreviousStatic(index) : LevelData.FindNextStatic(index);
            if (other < 0)
                return;
            if (LevelData.TileReferencesToObject(index).Any() || LevelData.TileReferencesToObject(other).Any())
            {
                MessageBox.Show(this, "Failed to swap Objects", "Draw Order", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }
            LevelData.SwapObjects(index, other);
            SelectedObjectIndex = other;
            RefreshObjectList();
            Render();
        }

        private IEnumerable<Correlation> GetAllCorrelations()
        {
            if (LevelData == null)
                yield break;
            foreach ((ObjectRecord Obj, int Index) item in LevelData.Objects.Select((o, i) => (o, i)))
            {
                if (item.Obj.GetInt(12) != 1)
                    continue;
                int tex = item.Obj.GetInt(18);
                string name = TextureCatalog.TextureName(tex);
                (int acx, int acy) = Helpers.ObjectAnchorCell(item.Obj);
                for (int cy = Math.Max(0, acy - 4); cy <= Math.Min(Constants.TileHeight - 1, acy + 4); cy++)
                {
                    for (int cx = Math.Max(0, acx - 5); cx <= Math.Min(Constants.VisibleTileWidth - 1, acx + 5); cx++)
                    {
                        if (!Helpers.GridCellToCoords(cx, cy, out int rawX, out int rawY))
                            continue;
                        TileRecord tile = LevelData.Tile(rawX, rawY);
                        if (tile.GetInt(0) != 1)
                            continue;
                        yield return new(item.Index, tex, name, item.Obj.GetFloat(1), item.Obj.GetFloat(2), item.Obj.GetInt(6), item.Obj.GetInt(7), acx, acy, cx, cy, cx - acx, cy - acy, $"raw RX{rawX},RY{rawY}; fields={string.Join(" ", tile.GetInts())}");
                    }
                }
            }
        }

        private void ApplyColliderPreset(int objectIndex, int offsetX, int offsetY, int width, int height)
        {
            if (LevelData == null)
                return;
            ObjectRecord obj = LevelData.Object(objectIndex);
            Helpers.EnsureStatic(obj, objectIndex);
            (int cx, int cy) = Helpers.ObjectAnchorCell(obj);
            int startCx = cx + offsetX;
            int startCy = cy + offsetY;
            for (int y = startCy; y < startCy + height; y++)
            {
                for (int x = startCx; x < startCx + width; x++)
                {
                    if (x < 0 || x >= Constants.VisibleTileWidth || y < 0 || y >= Constants.TileHeight)
                        continue;
                    if (!Helpers.GridCellToCoords(x, y, out int rawX, out int rawY))
                        continue;
                    TileRecord tile = LevelData.Tile(rawX, rawY);
                    tile.SetInt(0, 1);
                    for (int i = 1; i <= 5; i++)
                        tile.SetInt(i, 0);
                }
            }
        }
    }
}