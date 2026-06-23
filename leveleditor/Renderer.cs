using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Shapes;

namespace Editor
{
    public partial class MainWindow
    {
        private void Render()
        {
            cLevel?.Children.Clear();
            SelectedBorder = null;
            RenderBackground();
            if (LevelData == null)
            {
                RenderEmptyLevel();
                return;
            }
            if (cbShowObjects.IsChecked == true)
                RenderStaticObjects();
            if (cbShowGrid.IsChecked == true)
                RenderGrid();
            if (cbShowTiles.IsChecked == true)
                RenderTiles();
            RenderSelectedBorder();
            UpdateInfo();
            UpdateSelectedInfo();
            UpdateCorrelationList();
        }

        private void RenderBackground()
        {
            Rectangle background = new()
            {
                Width = Constants.Width,
                Height = Constants.Height,
                Fill = new SolidColorBrush(Color.FromRgb(32, 32, 32)),
                Stroke = new SolidColorBrush(Color.FromRgb(80, 80, 80)),
                StrokeThickness = 1,
            };
            Canvas.SetLeft(background, 0);
            Canvas.SetTop(background, 0);
            cLevel?.Children.Add(background);
        }

        private void RenderEmptyLevel()
        {
            TextBlock text = new()
            {
                Text = "Open a .lvl File and optionally an Asset Folder.",
                Foreground = Brushes.White,
                FontSize = 18,
                TextWrapping = TextWrapping.Wrap,
                Width = Constants.Width,
            };
            Canvas.SetLeft(text, 30);
            Canvas.SetTop(text, 30);
            cLevel?.Children.Add(text);
        }

        private void RenderStaticObjects()
        {
            if (LevelData == null)
                return;
            foreach ((ObjectRecord Obj, int Index) item in LevelData.Objects.Select((o, i) => (o, i)))
            {
                if (item.Obj.GetInt(12) != 1) //TODO: Draw Characters, 1 = object, 2 = sheep, 4 = dog, 5 = farmer, 6 = girl, 7/8/9/10/11/12 = idk (maybe item spawner)
                    continue;
                float x = item.Obj.GetFloat(1);
                float y = item.Obj.GetFloat(2);
                int w = Math.Max(1, item.Obj.GetInt(6));
                int h = Math.Max(1, item.Obj.GetInt(7));
                int tex = item.Obj.GetInt(18);
                FrameworkElement element;
                ImageSource? source = GetTextureImage(tex);
                if (source != null)
                {
                    element = new Image()
                    {
                        Source = source,
                        Width = w,
                        Height = h,
                        Stretch = Stretch.Fill,
                        Opacity = 0.92,
                        ToolTip = ObjectTooltip(item.Index),
                    };
                }
                else
                {
                    element = new Border()
                    {
                        Width = w,
                        Height = h,
                        BorderBrush = Brushes.Lime,
                        BorderThickness = new Thickness(1),
                        Background = new SolidColorBrush(Color.FromArgb(35, 60, 255, 60)),
                        ToolTip = ObjectTooltip(item.Index),
                        Child = new TextBlock()
                        {
                            Text = $"{item.Index}\n{TextureCatalog.TextureName(tex)}",
                            Foreground = Brushes.Lime,
                            FontSize = 10
                        },
                    };
                }
                element.Tag = item.Index;
                element.Cursor = Cursors.Hand;
                element.MouseLeftButtonDown += StaticObject_MouseLeftButtonDown;
                element.MouseMove += StaticObject_MouseMove;
                element.MouseLeftButtonUp += StaticObject_MouseLeftButtonUp;
                Canvas.SetLeft(element, x);
                Canvas.SetTop(element, y);
                cLevel?.Children.Add(element);
                Ellipse dot = new()
                {
                    Width = 6,
                    Height = 6,
                    Fill = Brushes.Lime,
                    Opacity = 0.9,
                    IsHitTestVisible = false,
                    ToolTip = $"object[{item.Index}] bottom-center anchor",
                };
                Canvas.SetLeft(dot, x + w / 2.0 - 3);
                Canvas.SetTop(dot, y + h - 3);
                cLevel?.Children.Add(dot);
            }
        }

        private void RenderGrid()
        {
            for (int cx = 0; cx <= Constants.VisibleTileWidth; cx++)
            {
                float px = Constants.VisibleOriginX + cx * Constants.CellWidth;
                Line line = new()
                {
                    X1 = px,
                    Y1 = Constants.VisibleOriginY,
                    X2 = px,
                    Y2 = Constants.VisibleOriginY + Constants.TileHeight * Constants.CellHeight,
                    Stroke = Brushes.Red,
                    StrokeThickness = 1,
                    Opacity = 0.28,
                    IsHitTestVisible = false,
                };
                cLevel?.Children.Add(line);
                if (cx < Constants.VisibleTileWidth)
                {
                    TextBlock label = Helpers.MakeOverlayText("CX" + cx, 10);
                    Canvas.SetLeft(label, px + 3);
                    Canvas.SetTop(label, Constants.VisibleOriginY - 16);
                    cLevel?.Children.Add(label);
                }
            }
            for (int cy = 0; cy <= Constants.TileHeight; cy++)
            {
                float py = Constants.VisibleOriginY + cy * Constants.CellHeight;
                Line line = new()
                {
                    X1 = Constants.VisibleOriginX,
                    Y1 = py,
                    X2 = Constants.VisibleOriginX + Constants.VisibleTileWidth * Constants.CellWidth,
                    Y2 = py,
                    Stroke = Brushes.Red,
                    StrokeThickness = 1,
                    Opacity = 0.28,
                    IsHitTestVisible = false,
                };
                cLevel?.Children.Add(line);
                if (cy < Constants.TileHeight)
                {
                    TextBlock label = Helpers.MakeOverlayText("CY" + cy, 10);
                    Canvas.SetLeft(label, Constants.VisibleOriginX - 34);
                    Canvas.SetTop(label, py + 3);
                    cLevel?.Children.Add(label);
                }
            }
            for (int hx = 0; hx <= Constants.HiddenTileWidth; hx++)
            {
                float px = Constants.HiddenOriginX + hx * Constants.CellWidth;
                Line line = new()
                {
                    X1 = px,
                    Y1 = Constants.HiddenOriginY,
                    X2 = px,
                    Y2 = Constants.HiddenOriginY + Constants.TileHeight * Constants.CellHeight,
                    Stroke = Brushes.DimGray,
                    StrokeThickness = 1,
                    Opacity = 0.45,
                    IsHitTestVisible = false,
                };
                cLevel?.Children.Add(line);
                if (hx < Constants.HiddenTileWidth)
                {
                    TextBlock label = Helpers.MakeOverlayText("H" + hx, 10);
                    Canvas.SetLeft(label, px + 3);
                    Canvas.SetTop(label, Constants.HiddenOriginY - 16);
                    cLevel?.Children.Add(label);
                }
            }
            for (int hy = 0; hy <= Constants.TileHeight; hy++)
            {
                float py = Constants.HiddenOriginY + hy * Constants.CellHeight;
                Line line = new()
                {
                    X1 = Constants.HiddenOriginX,
                    Y1 = py,
                    X2 = Constants.HiddenOriginX + Constants.HiddenTileWidth * Constants.CellWidth,
                    Y2 = py,
                    Stroke = Brushes.DimGray,
                    StrokeThickness = 1,
                    Opacity = 0.45,
                    IsHitTestVisible = false,
                };
                cLevel?.Children.Add(line);
            }
        }

        private void RenderTiles()
        {
            if (LevelData == null)
                return;
            for (int rawY = 0; rawY < Constants.TileHeight; rawY++)
            {
                for (int rawX = 0; rawX < Constants.TileWidth; rawX++)
                {
                    TileRecord tile = LevelData.Tile(rawX, rawY);
                    int t = tile.GetInt(0);
                    if (t == 0)
                        continue;
                    if (!Helpers.TryMapTileForDisplay(t, rawX, rawY, out int displayX, out int displayY, out bool hidden, out string coordLabel, out int cx25))
                        continue;
                    if (hidden && cbShowHidden.IsChecked != true)
                        continue;
                    float left = hidden ? Helpers.HiddenLeft(displayX) : Helpers.VisibleLeft(displayX);
                    float top = hidden ? Helpers.HiddenTop(displayY) : Helpers.VisibleTop(displayY);
                    Brush fill = t switch
                    {
                        1 => Brushes.Red,
                        2 => Brushes.MediumPurple,
                        3 => Brushes.Gold,
                        10 => Brushes.DeepSkyBlue,
                        11 => Brushes.DodgerBlue,
                        _ => Brushes.Orange
                    };
                    Rectangle rect = new()
                    {
                        Width = Constants.CellWidth,
                        Height = Constants.CellHeight,
                        Fill = fill,
                        Stroke = fill,
                        StrokeThickness = 2,
                        Opacity = hidden ? 0.45 : 0.32,
                        IsHitTestVisible = false,
                        ToolTip = string.Format("raw RX{0},{1} index={2}\n{3}\nraw projection CX25={4} {5}\nfields: {6}", rawX, rawY, rawY * Constants.TileWidth + rawX, coordLabel, cx25, cx25 >= Constants.VisibleTileWidth ? "(H" + (cx25 - Constants.VisibleTileWidth) + ")" : "(CX" + cx25 + ")", string.Join(",", tile.GetInts())),
                    };
                    Canvas.SetLeft(rect, left);
                    Canvas.SetTop(rect, top);
                    cLevel?.Children.Add(rect);
                    TextBlock label = Helpers.MakeOverlayText($"T{t}", 10);
                    Canvas.SetLeft(label, left + 3);
                    Canvas.SetTop(label, top + 3);
                    cLevel?.Children.Add(label);
                    TextBlock rawLabel = Helpers.MakeOverlayText($"R{rawX}", 8);
                    Canvas.SetLeft(rawLabel, left + 3);
                    Canvas.SetTop(rawLabel, top + 27);
                    cLevel?.Children.Add(rawLabel);
                    if (tile.GetInt(2) != 0)
                    {
                        TextBlock olabel = Helpers.MakeOverlayText($"o{tile.GetInt(2)}", 9);
                        Canvas.SetLeft(olabel, left + 3);
                        Canvas.SetTop(olabel, top + 17);
                        cLevel?.Children.Add(olabel);
                    }
                }
            }
        }

        private void RenderSelectedBorder()
        {
            if (LevelData == null || SelectedObjectIndex is not int idx)
                return;
            ObjectRecord obj = LevelData.Object(idx);
            if (obj.GetInt(12) != 1)
                return;
            SelectedBorder = new Rectangle()
            {
                Width = Math.Max(1, obj.GetInt(6)),
                Height = Math.Max(1, obj.GetInt(7)),
                Stroke = Brushes.Yellow,
                StrokeThickness = 3,
                Fill = Brushes.Transparent,
                IsHitTestVisible = false,
            };
            Canvas.SetLeft(SelectedBorder, obj.GetFloat(1));
            Canvas.SetTop(SelectedBorder, obj.GetFloat(2));
            cLevel?.Children.Add(SelectedBorder);
        }

        private void UpdateInfo()
        {
            if (LevelData == null)
            {
                tbLevelInfo.Text = "No Level loaded";
                return;
            }
            int staticCount = LevelData.Objects.Count(o => o.GetInt(12) == 1);
            int type1Count = LevelData.Tiles.Count(t => t.GetInt(0) == 1);
            int type10Count = LevelData.Tiles.Count(t => t.GetInt(0) == 10 || t.GetInt(0) == 11);
            int hiddenNonZero = 0;
            for (int y = 0; y < Constants.TileHeight; y++)
            {
                for (int x = 0; x < Constants.TileWidth; x++)
                {
                    if (LevelData.Tile(x, y).GetInt(0) != 0 && Helpers.IsHiddenTile(x, y))
                        hiddenNonZero++;
                }
            }
            tbLevelInfo.Text =
                $"File: {System.IO.Path.GetFileName(LevelPath ?? "")}\n" +
                $"Name: {LevelData.Name}\n" +
                $"Static objects: {staticCount}\n" +
                $"Type-1 blockers: {type1Count}\n" +
                $"Puddle triggers 10/11: {type10Count}\n" +
                $"Hidden nonzero raw tiles: {hiddenNonZero}\n" +
                $"Assets: {(string.IsNullOrEmpty(AssetPath) ? "not loaded" : AssetMap.Count + " images")}";
        }

        private void UpdateSelectedInfo()
        {
            if (LevelData == null || SelectedObjectIndex is not int idx)
            {
                tbSelectedInfo.Text = "Nothing selected";
                return;
            }
            ObjectRecord obj = LevelData.Object(idx);
            int tex = obj.GetInt(18);
            (int tx, int ty) = Helpers.ObjectAnchorTile(obj);
            (int cx, int cy) = Helpers.ObjectAnchorCell(obj);
            tbSelectedInfo.Text =
                $"object[{idx}]\n" +
                $"type/field12 = {obj.GetInt(12)}\n" +
                $"texture/field18 = {tex} {TextureCatalog.TextureName(tex)}\n" +
                $"x/y = {obj.GetFloat(1):0.##}, {obj.GetFloat(2):0.##}\n" +
                $"size = {obj.GetInt(6)} x {obj.GetInt(7)}\n" +
                $"bottom-center cell = CX{cx},CY{cy} / raw RX{tx},RY{ty}\n" +
                $"field20={obj.GetInt(20)} field21={obj.GetInt(21)} field22={obj.GetInt(22)} field24={obj.GetInt(24)}";
        }

        private void UpdateCorrelationList()
        {
            lbCorrelations.Items.Clear();
            if (LevelData == null || SelectedObjectIndex is not int idx)
                return;
            ObjectRecord obj = LevelData.Object(idx);
            if (obj.GetInt(12) != 1)
                return;
            (int tx, int ty) = Helpers.ObjectAnchorTile(obj);
            (int ccx, int ccy) = Helpers.ObjectAnchorCell(obj);
            lbCorrelations.Items.Add($"anchor cell = CX{ccx},CY{ccy} / raw RX{tx},RY{ty}");
            bool any = false;
            for (int cy = Math.Max(0, ccy - 4); cy <= Math.Min(Constants.TileHeight - 1, ccy + 4); cy++)
            {
                for (int cx = Math.Max(0, ccx - 5); cx <= Math.Min(Constants.VisibleTileWidth - 1, ccx + 5); cx++)
                {
                    if (!Helpers.GridCellToCoords(cx, cy, out int rawX, out int rawY))
                        continue;
                    TileRecord tile = LevelData.Tile(rawX, rawY);
                    if (tile.GetInt(0) != 1)
                        continue;
                    lbCorrelations.Items.Add($"cell CX{cx},CY{cy} raw RX{rawX},RY{rawY} offset=({cx - ccx},{cy - ccy}) fields={string.Join(",", tile.GetInts())}");
                    any = true;
                }
            }
            if (!any)
                lbCorrelations.Items.Add("no nearby type-1 tiles");
        }
    }
}