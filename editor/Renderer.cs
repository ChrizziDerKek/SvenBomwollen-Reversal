using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Shapes;
using System.Windows.Media.Imaging;

namespace Editor
{
    public partial class MainWindow
    {
        private void Render()
        {
            if (cLevel == null)
                return;
            cLevel.Children.Clear();

            DrawBackground();
            if (LevelData != null)
            {
                DrawObjects();

                // Tile overlays are editor/debug information, so keep them above objects.
                if (cbLayerTiles.IsChecked == true)
                    DrawTiles();

                DrawSelection();
            }
            if (cbLayerGrid != null && cbLayerGrid.IsChecked == true)
                DrawGrid();
        }

        private void DrawBackground()
        {
            Rectangle fallback = new()
            {
                Width = Constants.LevelWidth,
                Height = Constants.LevelHeight,
                Fill = new SolidColorBrush(Color.FromRgb(42, 48, 42)),
                IsHitTestVisible = false,
            };
            cLevel.Children.Add(fallback);

            if (cbLayerBackground.IsChecked != true)
                return;

            ImageSource? bg = Assets.GetImage(TextureCatalog.CurrentBackgroundFile(), chromaKey: false);
            if (bg == null)
                return;
            Image image = new()
            {
                Source = bg,
                Width = Constants.LevelWidth,
                Height = Constants.LevelHeight,
                Stretch = Stretch.Fill,
                IsHitTestVisible = false,
            };
            Canvas.SetLeft(image, 0);
            Canvas.SetTop(image, 0);
            cLevel.Children.Add(image);
        }

        private void DrawGrid()
        {
            for (int x = 0; x <= Constants.VisibleTileWidth; x++)
            {
                Line line = new()
                {
                    X1 = x * Constants.CellWidth,
                    Y1 = 0,
                    X2 = x * Constants.CellWidth,
                    Y2 = Constants.LevelHeight,
                    Stroke = new SolidColorBrush(Color.FromArgb(55, 255, 255, 255)),
                    StrokeThickness = 1,
                    IsHitTestVisible = false,
                };
                cLevel.Children.Add(line);
            }
            for (int y = 0; y <= Constants.TileHeight; y++)
            {
                Line line = new()
                {
                    X1 = 0,
                    Y1 = y * Constants.CellHeight,
                    X2 = Constants.LevelWidth,
                    Y2 = y * Constants.CellHeight,
                    Stroke = new SolidColorBrush(Color.FromArgb(55, 255, 255, 255)),
                    StrokeThickness = 1,
                    IsHitTestVisible = false,
                };
                cLevel.Children.Add(line);
            }
        }

        private void DrawTiles()
        {
            if (LevelData == null)
                return;

            for (int rawY = 0; rawY < Constants.TileHeight; rawY++)
            {
                for (int rawX = 0; rawX < Constants.TileWidth; rawX++)
                {
                    if (rawY * Constants.TileWidth + rawX >= Constants.TileCount) continue;
                    TileRecord tile = LevelData.Tile(rawX, rawY);
                    int[] fields = tile.GetInts();
                    bool nonEmpty = fields.Any(v => v != 0);
                    if (!nonEmpty)
                        continue;
                    if (!Helpers.RawToGridCell(rawX, rawY, out int cx, out int cy))
                        continue;

                    int type = fields[0];
                    Rectangle rect = new()
                    {
                        Width = Constants.CellWidth,
                        Height = Constants.CellHeight,
                        Fill = type == 0 ? new SolidColorBrush(Color.FromArgb(120, 255, 210, 40)) : Helpers.TileBrush(type),
                        Stroke = Brushes.White,
                        StrokeThickness = 1,
                        IsHitTestVisible = false,
                    };
                    Canvas.SetLeft(rect, cx * Constants.CellWidth);
                    Canvas.SetTop(rect, cy * Constants.CellHeight);
                    cLevel.Children.Add(rect);

                    TextBlock label = Helpers.OverlayText(type == 0 ? (fields[5] != 0 ? fields[5].ToString() : "?") : type.ToString(), 11);
                    Canvas.SetLeft(label, cx * Constants.CellWidth + 2);
                    Canvas.SetTop(label, cy * Constants.CellHeight + 1);
                    cLevel.Children.Add(label);
                }
            }
        }

        private void DrawObjects()
        {
            if (LevelData == null)
                return;

            foreach (int i in DrawOrder.Indices(LevelData))
            {
                ObjectRecord obj = LevelData.Object(i);
                int kind = obj.GetInt(12);
                if (!ShouldShowObjectKind(kind)) continue;
                if (kind == 1) DrawStaticObject(i, obj);
                else DrawMarkerObject(i, obj, kind);
            }
        }

        private void DrawStaticObject(int index, ObjectRecord obj)
        {
            int tex = obj.GetInt(18);
            double x = obj.GetFloat(1);
            double y = obj.GetFloat(2);
            double w = Math.Max(1, obj.GetInt(6));
            double h = Math.Max(1, obj.GetInt(7));
            ImageSource? image = Assets.GetImage(TextureCatalog.TextureFile(tex), chromaKey: true);
            if (image != null)
            {
                Image element = new()
                {
                    Source = image,
                    Width = w,
                    Height = h,
                    Stretch = Stretch.Fill,
                    Opacity = 1.0,
                    IsHitTestVisible = false,
                };
                Canvas.SetLeft(element, x);
                Canvas.SetTop(element, y);
                cLevel.Children.Add(element);
            }
            else
            {
                Rectangle rect = new()
                {
                    Width = w,
                    Height = h,
                    Fill = new SolidColorBrush(Color.FromArgb(75, 80, 220, 80)),
                    Stroke = Brushes.LimeGreen,
                    StrokeThickness = 2,
                    IsHitTestVisible = false,
                };
                Canvas.SetLeft(rect, x);
                Canvas.SetTop(rect, y);
                cLevel.Children.Add(rect);
            }

            if (cbLayerLabels.IsChecked != true) return;
            TextBlock label = Helpers.OverlayText($"{index}: {tex}", 10);
            Canvas.SetLeft(label, x);
            Canvas.SetTop(label, y);
            cLevel.Children.Add(label);
        }

        private void DrawMarkerObject(int index, ObjectRecord obj, int kind)
        {
            Rect rect = Helpers.ObjectRect(obj);
            string file = kind switch
            {
                0 => "Sven2/gfx/Sven/sven_stand.bmp", 2 => "Sven2/gfx/Sheep/Sheep_Stand.bmp",
                3 => "Sven2/gfx/Alien/alien_stand.bmp", 4 => "Sven2/gfx/Dog/dog_stand.bmp",
                5 => "Sven2/gfx/Guy/guy_stand.bmp", 6 => "Sven2/gfx/Broemse/broemse_stand.bmp",
                7 => "Sven2/gfx/Beer.bmp", 8 => "Sven2/gfx/Teddybear.bmp",
                9 => "Sven2/gfx/Bone.bmp", 10 => "Sven2/gfx/Artifact.bmp",
                11 => "Sven2/gfx/Animals/Chicken_Stand.bmp", 12 => "Sven2/gfx/Animals/Goat_Stand.bmp",
                13 => "Sven2/gfx/Animals/Pig_Stand.bmp", _ => ""
            };
            if (Assets.GetImage(file, true) is BitmapSource sheet)
            {
                int width = Math.Min(obj.GetInt(6), sheet.PixelWidth), height = Math.Min(obj.GetInt(7), sheet.PixelHeight);
                var image = new Image { Source = new CroppedBitmap(sheet, new Int32Rect(0, AnimalFactory.IsAnimal(kind) && sheet.PixelHeight >= height * 4 ? Math.Clamp(obj.GetInt(20), 0, 3) * height : 0, width, height)), Width = rect.Width, Height = rect.Height, IsHitTestVisible = false };
                Canvas.SetLeft(image, rect.X); Canvas.SetTop(image, rect.Y); cLevel.Children.Add(image);
            }
            Brush brush = Helpers.ObjectMarkerBrush(kind);
            if (cbLayerLabels.IsChecked != true) return;
            Rectangle marker = new()
            {
                Width = rect.Width,
                Height = rect.Height,
                Fill = Brushes.Transparent,
                Stroke = brush,
                StrokeThickness = 2,
                IsHitTestVisible = false,
            };
            Canvas.SetLeft(marker, rect.X);
            Canvas.SetTop(marker, rect.Y);
            cLevel.Children.Add(marker);

            TextBlock label = Helpers.OverlayText($"{index}: {Helpers.ObjectKindName(kind)}", 11);
            Canvas.SetLeft(label, rect.X);
            Canvas.SetTop(label, rect.Y);
            cLevel.Children.Add(label);

            if (Helpers.IsActorKind(kind))
                DrawActorTileLink(index, obj, brush);
        }

        private void DrawActorTileLink(int index, ObjectRecord obj, Brush brush)
        {
            if (LevelData == null)
                return;
            int raw = obj.GetInt(21);
            if (raw < 0 || raw >= Constants.TileCount)
                return;
            int rawX = raw % Constants.TileWidth;
            int rawY = raw / Constants.TileWidth;
            if (!Helpers.RawToGridCell(rawX, rawY, out int cx, out int cy))
                return;
            Rect rect = Helpers.ObjectRect(obj);
            double ox = rect.X + rect.Width / 2;
            double oy = rect.Y + rect.Height / 2;
            double tx = cx * Constants.CellWidth + Constants.CellWidth / 2;
            double ty = cy * Constants.CellHeight + Constants.CellHeight / 2;
            Line line = new()
            {
                X1 = ox,
                Y1 = oy,
                X2 = tx,
                Y2 = ty,
                Stroke = brush,
                StrokeThickness = 2,
                StrokeDashArray = new DoubleCollection { 3, 3 },
                IsHitTestVisible = false,
            };
            cLevel.Children.Add(line);
        }

        private void DrawSelection()
        {
            if (LevelData == null || Tool.SelectedObjectIndex is not int index)
                return;
            ObjectRecord obj = LevelData.Object(index);
            if (!ShouldShowObjectKind(obj.GetInt(12)))
                return;
            Rect rect = Helpers.ObjectRect(obj);
            Rectangle selection = new()
            {
                Width = rect.Width,
                Height = rect.Height,
                Stroke = Brushes.Yellow,
                StrokeThickness = 3,
                Fill = Brushes.Transparent,
                IsHitTestVisible = false,
            };
            Canvas.SetLeft(selection, rect.X);
            Canvas.SetTop(selection, rect.Y);
            cLevel.Children.Add(selection);
        }
    }
}
