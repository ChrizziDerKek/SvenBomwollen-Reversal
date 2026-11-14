using Microsoft.Win32;
using System.ComponentModel;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace Editor;

public partial class MainWindow
{
    private readonly Stack<byte[]> RedoStack = new();
    private byte[]? SavedSnapshot;
    private bool PackDirty;
    private int? SelectedExportVersion => cbExportVersion.SelectedIndex switch { 1 => 6, 2 => 7, _ => null };
    private bool LevelDirty => LevelData != null && (SavedSnapshot == null || !LevelData.Data.SequenceEqual(SavedSnapshot));

    private void TryEdit(Action action, bool showError = false)
    {
        byte[]? before = LevelData?.Snapshot();
        try { action(); }
        catch (Exception ex)
        {
            if (before != null && LevelData != null) LevelData.Restore(before);
            IsDraggingObject = false; IsPaintingTiles = false; cLevel.ReleaseMouseCapture();
            tbStatus.Text = ex.Message;
            if (showError) MessageBox.Show(this, ex.Message, "Editor", MessageBoxButton.OK, MessageBoxImage.Error);
            RefreshObjectList(); UpdateSelectedInfo(); Render();
        }
    }

    private void Canvas_MouseLeftButtonDown(object sender, MouseButtonEventArgs e) => TryEdit(() => Canvas_MouseLeftButtonDownCore(sender, e));
    private void Canvas_MouseMove(object sender, MouseEventArgs e) => TryEdit(() => Canvas_MouseMoveCore(sender, e));
    private void Canvas_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
    {
        try { Canvas_MouseLeftButtonUpCore(sender, e); }
        catch (Exception ex)
        {
            if (LevelData != null && UndoStack.Count > 0) LevelData.Restore(UndoStack.Pop());
            IsDraggingObject = false; DragObjectIndex = -1; cLevel.ReleaseMouseCapture();
            tbStatus.Text = ex.Message; RefreshObjectList(); UpdateSelectedInfo(); Render();
        }
    }

    private bool CanDiscardLevel(bool replacingProject = false) => (!LevelDirty && (!replacingProject || !PackDirty)) || MessageBox.Show(this,
        "There are unsaved level or pack changes. Discard them?", "Unsaved level", MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes;

    private void Window_Closing(object? sender, CancelEventArgs e) => e.Cancel = !CanDiscardLevel(true);

    private void ExtendedButton_Click(object sender, RoutedEventArgs e) => TryEdit(() =>
    {
        if (sender == bOpenArchive) OpenArchive();
        else if (sender == bSaveArchive) SaveArchive();
        else if (sender == bCreateArchive) CreateArchive();
        else if (sender == bVerifySignature)
        {
            OpenFileDialog dialog = new() { Filter = "Sven packs|*.pak;*.dat|All files|*.*", FileName = Project?.ArchivePath ?? "" };
            if (dialog.ShowDialog(this) == true)
                tbSignatureStatus.Text = Path.GetFileName(dialog.FileName) + ": " + PackSignature.Verify(File.ReadAllBytes(dialog.FileName));
        }
        else if (sender == bImportProjectLevel)
        {
            if (Project == null) throw new InvalidOperationException("Open a pack project first.");
            OpenFileDialog dialog = new() { Filter = "Sven level|*.lvl" };
            if (dialog.ShowDialog(this) != true || !CanDiscardLevel()) return;
            LoadLevel(dialog.FileName, null);
            tbStatus.Text = "Imported level. Add to a levelset, then save the pack.";
        }
        else if (sender == bAddPackLevel) AddPackLevel();
        else if (sender == bRedo) Redo();
        else if (sender == bApplyAnimal && LevelData != null && Tool.SelectedObjectIndex is int animalIndex)
        {
            PushUndo(); AnimalFactory.SetSettings(LevelData.Object(animalIndex), cbSelectedFacing.SelectedIndex, cbSelectedAnger.SelectedIndex);
            UpdateSelectedInfo(); Render();
        }
        else if (sender == bApplyTransform && LevelData != null && Tool.SelectedObjectIndex is int transformIndex)
        {
            float x = float.Parse(tbObjectX.Text, System.Globalization.CultureInfo.InvariantCulture);
            float y = float.Parse(tbObjectY.Text, System.Globalization.CultureInfo.InvariantCulture);
            int depth = int.Parse(tbObjectDepth.Text);
            if (!float.IsFinite(x) || !float.IsFinite(y) || Math.Abs(x) > 10000 || Math.Abs(y) > 10000)
                throw new InvalidOperationException("Position must be finite and within 10,000 pixels of the level.");
            PushUndo(); LevelOperations.MoveObject(LevelData, transformIndex, x, y, true);
            LevelData.Object(transformIndex).SetInt(24, depth);
            RefreshObjectList(); UpdateSelectedInfo(); Render();
        }
        else if (sender == bSetTimeLimit && LevelData != null)
        {
            float seconds = float.Parse(tbTimeLimit.Text, System.Globalization.CultureInfo.InvariantCulture);
            if (!float.IsFinite(seconds) || seconds < 0) throw new InvalidOperationException("Time must be a finite nonnegative number.");
            PushUndo(); LevelData.TimeLimit = seconds; UpdateLevelInfo();
        }
        else if (sender == bSetTeleporters && LevelData != null)
        {
            int[] raw = tbTeleporters.Text.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries).Select(int.Parse).ToArray();
            if (raw.Distinct().Count() != raw.Length || raw.Any(i => i < 0 || i >= Constants.TileCount || LevelData.TileByRawIndex(i).GetInt(0) != 10))
                throw new InvalidOperationException("Destinations must be distinct raw indices of puddle tiles (T10).");
            PushUndo(); LevelData.SetTeleporters(raw); UpdateLevelInfo();
        }
        else if (sender == bApplyObject && LevelData != null && Tool.SelectedObjectIndex is int objectIndex)
        {
            int[] values = tbObjectFields.Text.Split(',', StringSplitOptions.TrimEntries).Select(int.Parse).ToArray();
            if (values.Length != 25) throw new InvalidOperationException("Enter exactly 25 raw integers for fields 1?25.");
            float x = BitConverter.Int32BitsToSingle(values[0]), y = BitConverter.Int32BitsToSingle(values[1]);
            if (!float.IsFinite(x) || !float.IsFinite(y) || values[5] is < 1 or > 8192 || values[6] is < 1 or > 8192)
                throw new InvalidOperationException("Object position or size is invalid.");
            ObjectRecord obj = LevelData.Object(objectIndex);
            if (values[11] != obj.GetInt(12) || values[20] != obj.GetInt(21))
                throw new InvalidOperationException("Use placement/movement tools to change object kinds or tile links.");
            PushUndo();
            obj.WriteInts(new[] { 0 }.Concat(values).ToArray());
            LevelOperations.MoveObject(LevelData, objectIndex, x, y, true);
            RefreshObjectList(); UpdateSelectedInfo(); Render();
        }
        else if (sender == bEarlier || sender == bLater)
        {
            if (LevelData == null || Tool.SelectedObjectIndex is not int index) return;
            PushUndo(); Tool.SelectedObjectIndex = LevelOperations.ReorderStatic(LevelData, index, sender == bLater);
            RefreshObjectList(); UpdateSelectedInfo(); Render();
        }
        else if (sender == bDeleteObject) DeleteSelected();
        else if (sender == bReadTile && LevelData != null)
        {
            int raw = int.Parse(tbRawTileIndex.Text);
            tbRawTileFields.Text = string.Join(",", LevelData.TileByRawIndex(raw).GetInts());
            tbStatus.Text = $"Raw tile {raw}; teleporters: {LevelData.TeleporterCount}.";
        }
        else if (sender == bApplyTile && LevelData != null)
        {
            int raw = int.Parse(tbRawTileIndex.Text);
            int[] fields = tbRawTileFields.Text.Split(',').Select(x => int.Parse(x.Trim())).ToArray();
            if (fields.Length != 6) throw new InvalidOperationException("Enter exactly six integers.");
            TileRecord tile = LevelData.TileByRawIndex(raw);
            PushUndo(); tile.WriteInts(fields); Render(); tbStatus.Text = $"Updated tile {raw}.";
        }
        else if (sender == bTileInventory && LevelData != null)
        {
            SaveFileDialog dialog = new() { Filter = "CSV inventory|*.csv", FileName = "tiles.csv" };
            if (dialog.ShowDialog(this) != true) return;
            var rows = new List<string> { "raw,cx,cy,f0,f1,f2,f3,f4,f5" };
            for (int i = 0; i < Constants.TileCount; i++)
            {
                Helpers.RawToGridCell(i % 25, i / 25, out int x, out int y);
                rows.Add($"{i},{x},{y}," + string.Join(",", LevelData.TileByRawIndex(i).GetInts()));
            }
            File.WriteAllLines(dialog.FileName, rows);
        }
    }, showError: true);

    private void OpenArchive()
    {
        OpenFileDialog dialog = new() { Filter = "Sven packs|*.pak;*.dat|All files|*.*" };
        if (dialog.ShowDialog(this) != true || !CanDiscardLevel(true)) return;
        PackProject project = PackProject.OpenArchive(dialog.FileName);
        Project = project; PackDirty = false; LevelData = null; CurrentPackLevel = null; SavedSnapshot = null;
        UndoStack.Clear(); RedoStack.Clear(); Tool.SelectedObjectIndex = null;
        Assets.AddFolder(project.RootFolder);
        RefreshProject();
        if (project.Levels.Count > 0) LoadPackLevel(project.Levels[0]);
        else { RefreshObjectList(); UpdateLevelInfo(); Render(); }
    }

    private void RefreshProject()
    {
        lbProjectLevels.Items.Clear();
        if (Project == null) return;
        foreach (var entry in Project.Levels) lbProjectLevels.Items.Add(entry);
        tbProjectInfo.Text = Project.ArchivePath ?? Project.RootFolder;
        tbSignatureStatus.Text = Project.SignatureStatus;
        PopulateTilesetCombo(); RefreshStaticPalette();
    }

    private void SaveArchive()
    {
        if (Project?.Archive == null) throw new InvalidOperationException("Open a pack or create a new pack from a template first.");
        SaveFileDialog dialog = new() { Filter = "Sven pack|*.pak|Sven data|*.dat", FileName = Path.GetFileName(Project.ArchivePath) };
        if (dialog.ShowDialog(this) != true) return;
        if (CurrentPackLevel != null && LevelData != null)
        {
            CurrentPackLevel = Project.UpdateLevelMetadata(CurrentPackLevel, TextureCatalog.CurrentTilesetName, tbMusicPath.Text);
            File.WriteAllBytes(CurrentPackLevel.FullPath, LevelData.Data);
        }
        else if (LevelData != null && LevelDirty) throw new InvalidOperationException("Add this level to a levelset before saving the pack.");
        string? set = CurrentPackLevel?.LevelSet, name = CurrentPackLevel?.Name;
        PackProject saved = Project.SaveArchive(dialog.FileName!, SelectedExportVersion);
        if (!ReferenceEquals(saved, Project))
        {
            Project = saved;
            CurrentPackLevel = saved.Levels.FirstOrDefault(e => e.LevelSet == set && e.Name == name);
            if (CurrentPackLevel != null && LevelData != null) LevelData.SourcePath = CurrentPackLevel.FullPath;
            Assets.AddFolder(saved.RootFolder);
        }
        PackDirty = false;
        SavedSnapshot = LevelData?.Snapshot(); tbStatus.Text = "Pack saved; archive structure verified. " + Project.SignatureStatus;
        RefreshProject();
    }

    private void CreateArchive()
    {
        if (Project?.Document == null) throw new InvalidOperationException("Open a pack or extracted pack folder to use as a resource template.");
        SaveFileDialog dialog = new() { Filter = "Sven pack|*.pak", FileName = "Custom.pak" };
        if (dialog.ShowDialog(this) != true || !CanDiscardLevel(true)) return;
        PackProject project = PackProject.CreateFromTemplate(Project, dialog.FileName);
        Project = project; PackDirty = false; LevelData = LevelFile.CreateBlank("custom"); CurrentPackLevel = null; SavedSnapshot = null;
        UndoStack.Clear(); RedoStack.Clear(); Tool.SelectedObjectIndex = 0;
        Assets.AddFolder(project.RootFolder); RefreshProject(); RefreshObjectList(); UpdateLevelInfo(); Render();
        tbStatus.Text = "New resource pack created. Add this level to a levelset, then Save Pack As.";
    }

    private void AddPackLevel()
    {
        if (Project == null || LevelData == null) throw new InvalidOperationException("Open a pack and a level first.");
        CurrentPackLevel = Project.AddLevel(LevelData, tbLevelSetName.Text.Trim(), tbPackLevelName.Text.Trim(),
            TextureCatalog.CurrentTilesetName, string.IsNullOrWhiteSpace(tbMusicPath.Text) ? CurrentPackLevel?.Music ?? "" : tbMusicPath.Text.Trim());
        PackDirty = Project.Archive != null;
        RefreshProject(); UpdateLevelInfo(); tbStatus.Text = "Level added to project. Save Pack As to write the archive.";
    }

    private void Redo()
    {
        if (LevelData == null || RedoStack.Count == 0) return;
        UndoStack.Push(LevelData.Snapshot()); LevelData.Restore(RedoStack.Pop());
        RefreshObjectList(); UpdateSelectedInfo(); Render();
    }

    private void DeleteSelected()
    {
        if (LevelData == null || Tool.SelectedObjectIndex is not int index || index <= 0) return;
        PushUndo(); LevelOperations.DeleteObject(LevelData, index); Tool.SelectedObjectIndex = null;
        RefreshObjectList(); UpdateSelectedInfo(); Render();
    }

    private void Window_PreviewKeyDown(object sender, KeyEventArgs e)
    {
        if (Keyboard.FocusedElement is TextBox || Keyboard.FocusedElement is System.Windows.Controls.Primitives.TextBoxBase) return;
        bool ctrl = Keyboard.Modifiers.HasFlag(ModifierKeys.Control);
        Action? action = e.Key switch
        {
            Key.Z when ctrl => Undo,
            Key.Y when ctrl => Redo,
            Key.S when ctrl => () => { if (Project?.Archive != null) SaveArchive(); else SaveReplaceInFolder(); },
            Key.O when ctrl => OpenArchive,
            Key.Escape or Key.V => () => { SetTool(EditorToolKind.Select); lbStaticPalette.SelectedItem = null; },
            Key.Delete => DeleteSelected,
            Key.Left => () => Nudge(-1, 0), Key.Right => () => Nudge(1, 0),
            Key.Up => () => Nudge(0, -1), Key.Down => () => Nudge(0, 1),
            _ => null
        };
        if (action == null) return;
        e.Handled = true; TryEdit(action);
    }

    private void Nudge(int dx, int dy)
    {
        if (LevelData == null || Tool.SelectedObjectIndex is not int index) return;
        ObjectRecord obj = LevelData.Object(index);
        int step = Helpers.IsActorKind(obj.GetInt(12)) || obj.GetInt(12) == 0 ? 40 : Keyboard.Modifiers.HasFlag(ModifierKeys.Shift) ? 10 : 1;
        PushUndo(); LevelOperations.MoveObject(LevelData, index, obj.GetFloat(1) + dx * step, obj.GetFloat(2) + dy * step, true);
        RefreshObjectList(); UpdateSelectedInfo(); Render();
    }

    private void ObjectSearch_Changed(object sender, TextChangedEventArgs e)
    {
        if (lbObjects != null) RefreshObjectList();
    }

    private Point? PanStart;
    private double PanHorizontal, PanVertical;
    private void Level_PanDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton != MouseButton.Middle) return;
        PanStart = e.GetPosition(this); PanHorizontal = svLevel.HorizontalOffset; PanVertical = svLevel.VerticalOffset;
        svLevel.CaptureMouse(); e.Handled = true;
    }
    private void Level_PanMove(object sender, MouseEventArgs e)
    {
        if (PanStart is not Point start) return;
        Point point = e.GetPosition(this);
        svLevel.ScrollToHorizontalOffset(PanHorizontal + start.X - point.X);
        svLevel.ScrollToVerticalOffset(PanVertical + start.Y - point.Y);
        e.Handled = true;
    }
    private void Level_PanUp(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton != MouseButton.Middle || PanStart == null) return;
        PanStart = null; svLevel.ReleaseMouseCapture(); e.Handled = true;
    }

    private void Level_MouseWheel(object sender, MouseWheelEventArgs e)
    {
        if (!Keyboard.Modifiers.HasFlag(ModifierKeys.Control)) return;
        double zoom = Math.Clamp(stZoom.ScaleX * (e.Delta > 0 ? 1.15 : 1 / 1.15), .25, 4);
        stZoom.ScaleX = stZoom.ScaleY = zoom;
        tbStatus.Text = $"Zoom {zoom:P0}"; e.Handled = true;
    }
}
