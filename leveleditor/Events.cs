using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace Editor
{
    public partial class MainWindow
    {
        private void Button_Click(object sender, RoutedEventArgs e)
        {
            if (sender == bOpenLevel)
            {
                OpenFileDialog dialog = new() { Filter = "Sven level (*.lvl)|*.lvl|All files (*.*)|*.*" };
                if (dialog.ShowDialog(this) != true)
                    return;
                try
                {
                    LevelData = LevelFile.Load(dialog.FileName);
                    LevelPath = dialog.FileName;
                    SelectedObjectIndex = null;
                    RefreshObjectList();
                    Render();
                    UpdateInfo();
                }
                catch (Exception ex)
                {
                    MessageBox.Show(this, ex.Message, "Failed to open level", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
            else if (sender == bOpenAssets)
            {
                OpenFolderDialog dialog = new();
                if (dialog.ShowDialog(this) != true)
                    return;
                AssetPath = dialog.FolderName;
                AssetMap.Clear();
                ImageCache.Clear();
                foreach (string file in Directory.EnumerateFiles(AssetPath, "*.*", SearchOption.AllDirectories))
                {
                    string ext = Path.GetExtension(file).ToLowerInvariant();
                    if (ext is ".bmp" or ".png" or ".jpg" or ".jpeg")
                        AssetMap[Path.GetFileName(file)] = file;
                }
                Render();
                UpdateInfo();
            }
            else if (sender == bSaveLevel)
            {
                if (LevelData == null)
                    return;
                SaveFileDialog dialog = new()
                {
                    Filter = "Sven level (*.lvl)|*.lvl|All files (*.*)|*.*",
                    FileName = string.IsNullOrEmpty(LevelPath) ? "edited.lvl" : Path.GetFileNameWithoutExtension(LevelPath) + "_edited.lvl",
                };
                if (dialog.ShowDialog(this) != true)
                    return;
                try
                {
                    LevelData.Save(dialog.FileName);
                    MessageBox.Show(this, "Saved level", "Save", MessageBoxButton.OK, MessageBoxImage.Information);
                }
                catch (Exception ex)
                {
                    MessageBox.Show(this, ex.Message, "Failed to save level", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
            else if (sender == bDrawEarlier || sender == bDrawLater)
            {
                int modifier = sender == bDrawEarlier ? -1 : 1;
                MoveDrawOrder(modifier);
            }
            else if (sender == bDuplicate)
            {
                if (LevelData == null || SelectedObjectIndex is not int src)
                    return;
                try
                {
                    int dst = LevelData.FindInactiveObject();
                    if (dst < 0)
                    {
                        MessageBox.Show(this, "No inactive Object Slot found", "Duplicate failed", MessageBoxButton.OK, MessageBoxImage.Error);
                        return;
                    }
                    ObjectRecord source = LevelData.Object(src);
                    Helpers.EnsureStatic(source, src);
                    ObjectRecord destination = LevelData.Object(dst);
                    Helpers.EnsureStatic(destination, dst);
                    destination.CopyFrom(source);
                    float x = source.GetFloat(1) + 20;
                    float y = source.GetFloat(2) + 20;
                    destination.SetFloat(1, x);
                    destination.SetFloat(2, y);
                    destination.SetFloat(3, x);
                    destination.SetFloat(4, y);
                    destination.SetInt(12, 1);
                    SelectObject(dst);
                    RefreshObjectList();
                    Render();
                }
                catch (Exception ex)
                {
                    MessageBox.Show(this, ex.Message, "Duplicate failed", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
            else if (sender == bDeactivate)
            {
                if (LevelData == null || SelectedObjectIndex is not int index)
                    return;
                if (LevelData.TileReferencesToObject(index).Any())
                {
                    MessageBox.Show(this, "The Object is referenced by a Tile. Cannot deactivate it", "Referenced Object", MessageBoxButton.OK, MessageBoxImage.Warning);
                    return;
                }
                int template = LevelData.FindInactiveObject(index);
                if (template < 0)
                {
                    MessageBox.Show(this, "No inactive Object Slot found", "Deactivate failed", MessageBoxButton.OK, MessageBoxImage.Error);
                    return;
                }
                LevelData.Object(index).CopyFrom(LevelData.Object(template));
                SelectedObjectIndex = null;
                RefreshObjectList();
                Render();
            }
            else if (sender == bExportCorrelations)
            {
                if (LevelData == null)
                    return;
                SaveFileDialog dialog = new()
                {
                    Filter = "CSV (*.csv)|*.csv|All files (*.*)|*.*",
                    FileName = string.IsNullOrEmpty(LevelPath) ? "correlations.csv" : Path.GetFileNameWithoutExtension(LevelPath) + "_correlations.csv",
                };
                if (dialog.ShowDialog(this) != true)
                    return;
                StringBuilder sb = new();
                sb.AppendLine("objectIndex,textureIndex,textureName,objectX,objectY,width,height,anchorCX,anchorCY,tileCX,tileCY,offsetCX,offsetCY,tileFields");
                foreach (Correlation item in GetAllCorrelations())
                    sb.AppendLine($"{item.ObjectIndex},{item.TextureIndex},\"{Helpers.CSV(item.TextureName)}\",{item.ObjectX},{item.ObjectY},{item.Width},{item.Height},{item.AnchorX},{item.AnchorY},{item.TileX},{item.TileY},{item.OffsetX},{item.OffsetY},\"{Helpers.CSV(item.TileFields)}\"");
                File.WriteAllText(dialog.FileName, sb.ToString(), Encoding.UTF8);
                MessageBox.Show(this, "Exported Correlations", "Export", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            else if (sender == bApplyColliderPreset)
            {
                if (LevelData == null || SelectedObjectIndex is not int index)
                    return;
                try
                {
                    int x = int.Parse(tbColliderX.Text);
                    int y = int.Parse(tbColliderY.Text);
                    int w = int.Parse(tbColliderW.Text);
                    int h = int.Parse(tbColliderH.Text);
                    ApplyColliderPreset(index, x, y, w, h);
                    Render();
                }
                catch (Exception ex)
                {
                    MessageBox.Show(this, ex.Message, "Apply Collider failed", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        private void RadioButton_Checked(object sender, RoutedEventArgs e) => Render();

        private void CheckBox_Changed(object sender, RoutedEventArgs e) => Render();

        private void Canvas_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (sender != cLevel || LevelData == null || rbPaintMode.IsChecked != true)
                return;
            Point pos = e.GetPosition(cLevel);
            CreateTile(pos, GetSelectedTileType());
        }

        private void Canvas_MouseRightButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (sender != cLevel)
                return;
            Point pos = e.GetPosition(cLevel);
            CreateTile(pos, 0);
        }

        private void Canvas_MouseMove(object sender, MouseEventArgs e)
        {
            if (sender != cLevel || LevelData == null || rbPaintMode.IsChecked != true || e.LeftButton != MouseButtonState.Pressed)
                return;
            Point pos = e.GetPosition(cLevel);
            CreateTile(pos, GetSelectedTileType());
        }

        private void Canvas_MouseLeftButtonUp(object sender, MouseButtonEventArgs e) { }

        private void ListBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (lbObjects.SelectedItem is ObjectListItem item)
                SelectObject(item.Index);
        }

        private void StaticObject_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (LevelData == null || rbObjectMode.IsChecked != true)
                return;
            if (sender is not FrameworkElement element || element.Tag is not int index)
                return;
            SelectObject(index);
            DragElement = element;
            DragObjectIndex = index;
            DragStartMouse = e.GetPosition(cLevel);
            ObjectRecord obj = LevelData.Object(index);
            DragStartX = obj.GetFloat(1);
            DragStartY = obj.GetFloat(2);
            element.CaptureMouse();
            e.Handled = true;
        }

        private void StaticObject_MouseMove(object sender, MouseEventArgs e)
        {
            if (LevelData == null || DragElement == null || DragObjectIndex is not int index || e.LeftButton != MouseButtonState.Pressed)
                return;
            Point pos = e.GetPosition(cLevel);
            float newX = DragStartX + (float)(pos.X - DragStartMouse.X);
            float newY = DragStartY + (float)(pos.Y - DragStartMouse.Y);
            ObjectRecord obj = LevelData.Object(index);
            obj.SetFloat(1, newX);
            obj.SetFloat(2, newY);
            obj.SetFloat(3, newX);
            obj.SetFloat(4, newY);
            Canvas.SetLeft(DragElement, newX);
            Canvas.SetTop(DragElement, newY);
            if (SelectedBorder != null)
            {
                Canvas.SetLeft(SelectedBorder, newX);
                Canvas.SetTop(SelectedBorder, newY);
            }
            UpdateSelectedInfo();
            UpdateCorrelationList();
        }

        private void StaticObject_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
        {
            DragElement?.ReleaseMouseCapture();
            DragElement = null;
            DragObjectIndex = null;
            RefreshObjectList();
            Render();
        }
    }
}