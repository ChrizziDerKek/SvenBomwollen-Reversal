using Microsoft.Win32;
using System.IO;
using System.Reflection;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Shapes;

namespace Editor
{
    public partial class MainWindow : Window
    {
        private readonly AssetManager Assets = new();
        private readonly EditorToolState Tool = new();
        private readonly Stack<byte[]> UndoStack = new();
        private const int MaxUndoStates = 50;

        private LevelFile? LevelData;
        private PackProject? Project;
        private PackLevelEntry? CurrentPackLevel;
        private bool UpdatingUi;
        private bool IsDraggingObject;
        private bool IsPaintingTiles;
        private int DragObjectIndex = -1;
        private Point DragStartMouse;
        private float DragStartX;
        private float DragStartY;

        public MainWindow()
        {
            InitializeComponent();
            Version? version = Assembly.GetExecutingAssembly().GetName().Version;
            if (version != null)
                Title = "Sven Zwo Level Editor";

            for (DirectoryInfo? folder = new DirectoryInfo(AppContext.BaseDirectory); folder != null; folder = folder.Parent)
            {
                string assets = System.IO.Path.Combine(folder.FullName, "assets");
                if (Directory.Exists(assets)) { Assets.LoadFolder(assets); break; }
            }
            PopulateTilesetCombo();
            PopulateCharacterPalette();
            PopulateTilePalette();
            RefreshStaticPalette();
            UpdateToolText();
            Render();
        }

        private void Button_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (sender == bOpenLevel)
                    OpenStandaloneLevel();
                else if (sender == bOpenAssets)
                    OpenAssetsFolder();
                else if (sender == bOpenPackFolder)
                    OpenExtractedPackFolder();
                else if (sender == bSaveStandalone)
                    SaveStandalone();
                else if (sender == bSaveReplace)
                    SaveReplaceInFolder();
                else if (sender == bNewBlank)
                    NewBlankLevel();
                else if (sender == bUndo)
                    Undo();
                else if (sender == bRebuildT3)
                    RebuildT3();
                else if (sender == bSelectTool)
                    SetTool(EditorToolKind.Select);
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, ex.Message, "Editor", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void ComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (UpdatingUi)
                return;
            if (sender == cbTileset && cbTileset.SelectedItem is string tileset)
            {
                TextureCatalog.UseTileset(tileset);
                if (CurrentPackLevel != null && Project != null) PackDirty = true;
                RefreshStaticPalette();
                UpdateLevelInfo();
                Render();
            }
        }

        private void CheckBox_Changed(object sender, RoutedEventArgs e)
        {
            if (cbSnapStatics == null)
                return;
            Tool.SnapStaticsToGrid = cbSnapStatics.IsChecked == true;
            RefreshObjectList();
            Render();
        }

        private void TabControl_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (UpdatingUi || e.Source != tcPalette)
                return;
            SyncToolFromActivePaletteTab();
        }

        private void ListBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (UpdatingUi)
                return;

            if (sender == lbStaticPalette && lbStaticPalette.SelectedItem is StaticPaletteItem staticItem)
            {
                Tool.SelectedStaticTextureId = staticItem.TextureId;
                SetTool(EditorToolKind.PlaceStatic);
            }
            else if (sender == lbCharacterPalette && lbCharacterPalette.SelectedItem is CharacterPaletteItem characterItem)
            {
                Tool.SelectedCharacter = characterItem.Kind;
                SetTool(characterItem.Kind == CharacterKind.SvenStart ? EditorToolKind.MoveSven : EditorToolKind.PlaceCharacter);
            }
            else if (sender == lbTilePalette && lbTilePalette.SelectedItem is TilePaletteItem tileItem)
            {
                Tool.SelectedTileType = tileItem.Type;
                SetTool(EditorToolKind.PaintTile);
            }
            else if (sender == lbObjects && lbObjects.SelectedItem is ObjectListItem objectItem)
            {
                Tool.SelectedObjectIndex = objectItem.Index;
                SetTool(EditorToolKind.Select, refresh: false);
                UpdateSelectedInfo();
                Render();
            }
        }

        private void ListBox_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (sender == lbProjectLevels && lbProjectLevels.SelectedItem is PackLevelEntry entry)
                LoadPackLevel(entry);
        }

        private void Canvas_MouseLeftButtonDownCore(object sender, MouseButtonEventArgs e)
        {
            if (LevelData == null)
                return;

            cLevel.Focus();
            Point point = e.GetPosition(cLevel);
            if (Helpers.PointToRawTile(point, out int trx, out int tryy)) tbRawTileIndex.Text = (tryy * Constants.TileWidth + trx).ToString();

            if (Tool.ActiveTool == EditorToolKind.PlaceStatic)
            {
                if (!Helpers.PointToCell(point, out int cx, out int cy))
                    return;
                PushUndo();
                int index = LevelOperations.SpawnStatic(LevelData, Tool.SelectedStaticTextureId, cx, cy);
                if (!Tool.SnapStaticsToGrid)
                    LevelOperations.MoveObject(LevelData, index, (float)point.X, (float)point.Y, finalizeActorTile: false);

                // Static placement is intentionally one-shot:
                // after placing a static, clear the palette selection and return to Select/Move.
                FinishStaticPlacement(index);
                e.Handled = true;
                return;
            }

            if (Tool.ActiveTool == EditorToolKind.PlaceCharacter)
            {
                if (!Helpers.PointToCell(point, out int cx, out int cy))
                    return;
                PushUndo();
                int index = LevelOperations.SpawnCharacter(LevelData, Tool.SelectedCharacter, cx, cy, Math.Max(0, cbAnimalFacing.SelectedIndex), Math.Max(0, cbSheepAnger.SelectedIndex));
                SelectObjectAfterEdit(index);
                e.Handled = true;
                return;
            }

            if (Tool.ActiveTool == EditorToolKind.MoveSven)
            {
                if (!Helpers.PointToCell(point, out int cx, out int cy))
                    return;
                PushUndo();
                LevelOperations.MoveSven(LevelData, cx, cy);
                SelectObjectAfterEdit(0);
                e.Handled = true;
                return;
            }

            if (Tool.ActiveTool == EditorToolKind.PaintTile)
            {
                if (!Helpers.PointToCell(point, out int cx, out int cy))
                    return;
                PushUndo();
                IsPaintingTiles = true;
                PaintTileAt(point, Tool.SelectedTileType);
                cLevel.CaptureMouse();
                e.Handled = true;
                return;
            }

            if ((Keyboard.Modifiers & (ModifierKeys.Control | ModifierKeys.Alt)) != 0)
            {
                ShowObjectPicker(point);
                e.Handled = true;
                return;
            }

            int? hit = HitTestTopObject(point);
            if (hit.HasValue)
            {
                PushUndo();
                Tool.SelectedObjectIndex = hit.Value;
                DragObjectIndex = hit.Value;
                DragStartMouse = point;
                ObjectRecord obj = LevelData.Object(hit.Value);
                DragStartX = obj.GetFloat(1);
                DragStartY = obj.GetFloat(2);
                IsDraggingObject = true;
                cLevel.CaptureMouse();
                UpdateSelectedInfo();
                RefreshObjectList();
                Render();
                e.Handled = true;
            }
            else
            {
                Tool.SelectedObjectIndex = null;
                UpdateSelectedInfo();
                RefreshObjectList();
                Render();
            }
        }

        private void Canvas_MouseRightButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (LevelData == null)
                return;
            Point point = e.GetPosition(cLevel);
            if (Tool.ActiveTool == EditorToolKind.PaintTile && Helpers.PointToCell(point, out int cx, out int cy))
            {
                TryEdit(() => { PushUndo(); LevelOperations.PaintTile(LevelData, cx, cy, 0); Render(); });
            }
            else ShowObjectPicker(point);
            e.Handled = true;
        }

        private void Canvas_MouseMoveCore(object sender, MouseEventArgs e)
        {
            if (LevelData == null)
                return;
            Point point = e.GetPosition(cLevel);
            if (IsPaintingTiles && e.LeftButton == MouseButtonState.Pressed)
            {
                PaintTileAt(point, Tool.SelectedTileType);
                return;
            }

            if (!IsDraggingObject || DragObjectIndex < 0 || e.LeftButton != MouseButtonState.Pressed)
                return;

            double dx = point.X - DragStartMouse.X;
            double dy = point.Y - DragStartMouse.Y;
            LevelOperations.MoveObject(LevelData, DragObjectIndex, DragStartX + (float)dx, DragStartY + (float)dy, finalizeActorTile: false);
            UpdateSelectedInfo();
            RefreshObjectList();
            Render();
        }

        private void Canvas_MouseLeftButtonUpCore(object sender, MouseButtonEventArgs e)
        {
            if (LevelData != null && IsDraggingObject && DragObjectIndex >= 0)
            {
                ObjectRecord obj = LevelData.Object(DragObjectIndex);
                float x = obj.GetFloat(1), y = obj.GetFloat(2);
                if (obj.GetInt(12) == 1 && Tool.SnapStaticsToGrid)
                {
                    x = (float)Math.Round(x / 40) * 40; y = (float)Math.Round(y / 40) * 40;
                }
                LevelOperations.MoveObject(LevelData, DragObjectIndex, x, y, finalizeActorTile: true);
                RefreshObjectList();
                UpdateSelectedInfo();
                Render();
            }
            IsDraggingObject = false;
            IsPaintingTiles = false;
            DragObjectIndex = -1;
            cLevel.ReleaseMouseCapture();
        }

        private void OpenStandaloneLevel()
        {
            OpenFileDialog dialog = new() { Filter = "Sven level (*.lvl)|*.lvl|All files (*.*)|*.*" };
            if (dialog.ShowDialog(this) != true)
                return;
            if (!CanDiscardLevel(true)) return;
            LoadLevel(dialog.FileName, null);
            Project = null; PackDirty = false;
            CurrentPackLevel = null;
            tbProjectInfo.Text = "Standalone mode";
            lbProjectLevels.Items.Clear();
        }

        private void OpenAssetsFolder()
        {
            OpenFolderDialog dialog = new();
            if (dialog.ShowDialog(this) != true)
                return;
            Assets.LoadFolder(dialog.FolderName);
            RefreshStaticPalette();
            UpdateLevelInfo();
            Render();
        }

        private void OpenExtractedPackFolder()
        {
            OpenFolderDialog dialog = new();
            if (dialog.ShowDialog(this) != true)
                return;
            if (!CanDiscardLevel(true)) return;
            PackProject project = PackProject.LoadFromExtractedFolder(dialog.FolderName);
            Assets.AddFolder(dialog.FolderName);
            Project = project; PackDirty = false; LevelData = null; CurrentPackLevel = null; SavedSnapshot = null;
            UndoStack.Clear(); RedoStack.Clear(); Tool.SelectedObjectIndex = null;
            tbProjectInfo.Text = Project.XmlPath == null
                ? $"Pack folder loaded, but no XML found.\n{dialog.FolderName}"
                : $"Pack folder loaded.\nXML: {System.IO.Path.GetFileName(Project.XmlPath)}";
            lbProjectLevels.Items.Clear();
            foreach (PackLevelEntry level in Project.Levels)
                lbProjectLevels.Items.Add(level);
            PopulateTilesetCombo();
            RefreshStaticPalette();
            Render();
        }

        private void LoadPackLevel(PackLevelEntry entry)
        {
            if (!CanDiscardLevel()) return;
            if (!File.Exists(entry.FullPath))
                throw new FileNotFoundException("Level file not found.", entry.FullPath);
            TextureCatalog.UseTileset(entry.Tileset);
            PopulateTilesetCombo();
            LoadLevel(entry.FullPath, entry);
        }

        private void LoadLevel(string path, PackLevelEntry? packEntry)
        {
            LevelData = LevelFile.Load(path);
            SavedSnapshot = LevelData.Snapshot();
            RedoStack.Clear();
            CurrentPackLevel = packEntry;
            tbMusicPath.Text = packEntry?.Music ?? "";
            UndoStack.Clear();
            Tool.SelectedObjectIndex = null;
            if (packEntry == null && TextureCatalog.TryGetTilesetForLevelFile(path, out string tileset))
            {
                TextureCatalog.UseTileset(tileset);
                PopulateTilesetCombo();
            }
            RefreshStaticPalette();
            RefreshObjectList();
            UpdateLevelInfo();
            UpdateSelectedInfo();
            Render();
        }

        private void SaveStandalone()
        {
            if (LevelData == null)
                return;
            SaveFileDialog dialog = new()
            {
                Filter = "Sven level (*.lvl)|*.lvl|All files (*.*)|*.*",
                FileName = LevelData.SourcePath == null ? "edited.lvl" : System.IO.Path.GetFileNameWithoutExtension(LevelData.SourcePath) + "_edited.lvl"
            };
            if (dialog.ShowDialog(this) != true)
                return;
            File.WriteAllBytes(dialog.FileName, LevelData.ExportBytes(SelectedExportVersion));
            LevelData.SourcePath = dialog.FileName;
            tbSignatureStatus.Text = "Standalone .lvl: no publisher signature; version " + (SelectedExportVersion ?? LevelData.Version) + ".";
            SavedSnapshot = LevelData.Snapshot();
            UpdateLevelInfo();
        }

        private void SaveReplaceInFolder()
        {
            if (LevelData == null)
                return;
            string? target = CurrentPackLevel?.FullPath ?? LevelData.SourcePath;
            if (string.IsNullOrWhiteSpace(target))
            {
                SaveStandalone();
                return;
            }
            if (CurrentPackLevel != null && Project != null)
                CurrentPackLevel = Project.UpdateLevelMetadata(CurrentPackLevel, TextureCatalog.CurrentTilesetName, tbMusicPath.Text);
            File.WriteAllBytes(target, LevelData.ExportBytes(SelectedExportVersion));
            LevelData.SourcePath = target;
            PackDirty |= Project?.Archive != null && CurrentPackLevel != null;
            SavedSnapshot = LevelData.Snapshot();
            MessageBox.Show(this, $"Saved:\n{target}", "Save", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void NewBlankLevel()
        {
            if (!CanDiscardLevel()) return;
            SavedSnapshot = null;
            RedoStack.Clear();
            PushUndoIfLevelExists();
            LevelData = LevelFile.CreateBlank("blank");
            CurrentPackLevel = null;
            Tool.SelectedObjectIndex = 0;
            UndoStack.Clear();
            RefreshObjectList();
            UpdateLevelInfo();
            UpdateSelectedInfo();
            Render();
        }

        private void Undo()
        {
            if (LevelData == null || UndoStack.Count == 0)
                return;
            RedoStack.Push(LevelData.Snapshot());
            LevelData.Restore(UndoStack.Pop());
            RefreshObjectList();
            UpdateLevelInfo();
            UpdateSelectedInfo();
            Render();
        }

        private void RebuildT3()
        {
            if (LevelData == null)
                return;
            PushUndo();
            LevelOperations.RebuildT3(LevelData);
            UpdateLevelInfo();
            Render();
        }

        private void PushUndoIfLevelExists()
        {
            if (LevelData != null)
                PushUndo();
        }

        private void PushUndo()
        {
            if (LevelData == null)
                return;
            RedoStack.Clear();
            UndoStack.Push(LevelData.Snapshot());
            if (UndoStack.Count > MaxUndoStates)
            {
                var newest = UndoStack.Take(MaxUndoStates).Reverse().ToArray();
                UndoStack.Clear();
                foreach (var state in newest) UndoStack.Push(state);
            }
            // Level files are small, so snapshot undo is intentionally simple for this first clean milestone.
        }

        private void PaintTileAt(Point point, int type)
        {
            if (LevelData == null || !Helpers.PointToCell(point, out int cx, out int cy))
                return;
            LevelOperations.PaintTile(LevelData, cx, cy, type);
            Render();
        }

        private void SetTool(EditorToolKind tool, bool refresh = true)
        {
            Tool.ActiveTool = tool;
            if (tool == EditorToolKind.PaintTile) cbLayerTiles.IsChecked = true;
            UpdateToolText();
            if (refresh)
                Render();
        }

        private void UpdateToolText()
        {
            string detail = Tool.ActiveTool switch
            {
                EditorToolKind.PlaceStatic => $"Static {Tool.SelectedStaticTextureId:00}",
                EditorToolKind.PlaceCharacter => Tool.SelectedCharacter.ToString(),
                EditorToolKind.PaintTile => $"Tile T{Tool.SelectedTileType:00}",
                EditorToolKind.MoveSven => "Move Sven",
                _ => "Select / Move",
            };
            tbActiveTool.Text = $"Tool: {detail}";
        }

        private void SelectObject(int index)
        {
            Tool.SelectedObjectIndex = index;
            SetTool(EditorToolKind.Select, refresh: false);
            RefreshObjectList();
            UpdateSelectedInfo();
            Render();
        }

        private void SelectObjectAfterEdit(int index)
        {
            Tool.SelectedObjectIndex = index;
            RefreshObjectList();
            UpdateSelectedInfo();
            Render();
        }

        private void FinishStaticPlacement(int index)
        {
            bool previousUpdating = UpdatingUi;
            UpdatingUi = true;
            lbStaticPalette.SelectedItem = null;
            UpdatingUi = previousUpdating;

            Tool.SelectedObjectIndex = index;
            SetTool(EditorToolKind.Select, refresh: false);
            RefreshObjectList();
            UpdateSelectedInfo();
            Render();
        }

        private void SyncToolFromActivePaletteTab()
        {
            if (tcPalette.SelectedIndex == 0)
            {
                if (lbStaticPalette.SelectedItem is StaticPaletteItem staticItem)
                {
                    Tool.SelectedStaticTextureId = staticItem.TextureId;
                    SetTool(EditorToolKind.PlaceStatic);
                }
                else
                {
                    SetTool(EditorToolKind.Select);
                }
            }
            else if (tcPalette.SelectedIndex == 1 && lbCharacterPalette.SelectedItem is CharacterPaletteItem characterItem)
            {
                Tool.SelectedCharacter = characterItem.Kind;
                SetTool(characterItem.Kind == CharacterKind.SvenStart ? EditorToolKind.MoveSven : EditorToolKind.PlaceCharacter);
            }
            else if (tcPalette.SelectedIndex == 2 && lbTilePalette.SelectedItem is TilePaletteItem tileItem)
            {
                Tool.SelectedTileType = tileItem.Type;
                SetTool(EditorToolKind.PaintTile);
            }
            else SetTool(EditorToolKind.Select);
        }

        private void PopulateTilesetCombo()
        {
            UpdatingUi = true;
            cbTileset.Items.Clear();
            foreach (string name in TextureCatalog.TilesetNames())
                cbTileset.Items.Add(name);
            cbTileset.SelectedItem = TextureCatalog.CurrentTilesetName;
            UpdatingUi = false;
        }

        private void PopulateCharacterPalette()
        {
            lbCharacterPalette.Items.Clear();
            foreach (var kind in new[] { CharacterKind.Alien, CharacterKind.Chicken, CharacterKind.Goat, CharacterKind.Pig, CharacterKind.Beer, CharacterKind.Teddy, CharacterKind.Bone, CharacterKind.Artifact })
                lbCharacterPalette.Items.Add(new CharacterPaletteItem(kind, Helpers.ObjectKindName((int)kind), AnimalFactory.IsAnimal((int)kind) ? "Creates animal from explicit game constructor defaults." : "Creates object and linked tile."));
            lbCharacterPalette.Items.Add(new CharacterPaletteItem(CharacterKind.SvenStart, "Sven Start", "Move existing object[0]; never spawns a clone."));
            lbCharacterPalette.Items.Add(new CharacterPaletteItem(CharacterKind.Sheep, "Sheep", "Creates object + linked T2 tile."));
            lbCharacterPalette.Items.Add(new CharacterPaletteItem(CharacterKind.DogWotan, "Dog / Wotan", "Creates object + linked T4 tile."));
            lbCharacterPalette.Items.Add(new CharacterPaletteItem(CharacterKind.FarmerLars, "Farmer / Lars", "Creates object + linked T5 tile."));
            lbCharacterPalette.Items.Add(new CharacterPaletteItem(CharacterKind.GirlBromse, "Girl / Bromse", "Creates object + linked T6 tile."));
        }

        private void PopulateTilePalette()
        {
            lbTilePalette.Items.Clear();
            foreach (TilePaletteItem item in new[]
            {
                new TilePaletteItem(0, "Air / Clear", "0,0,0,0,0,0"),
                new TilePaletteItem(1, "Collider", "1,0,0,0,0,0"),
                new TilePaletteItem(10, "Puddle Teleporter", "10,0,0,0,0,0"),
                new TilePaletteItem(11, "Pond / Water", "11,0,0,0,0,0"),
                new TilePaletteItem(140, "Random distraction spawner", "field5=140: beer / teddy / bone / artifact"),
                new TilePaletteItem(141, "Random player pickup spawner", "field5=141: five player effects"),
                new TilePaletteItem(142, "Random mixed spawner", "field5=142: all nine items"),
            })
                lbTilePalette.Items.Add(item);
        }

        private void RefreshStaticPalette()
        {
            bool previousUpdating = UpdatingUi;
            UpdatingUi = true;

            int previous = Tool.SelectedStaticTextureId;
            bool preserveSelection = Tool.ActiveTool == EditorToolKind.PlaceStatic;

            lbStaticPalette.Items.Clear();
            foreach (KeyValuePair<int, TextureCatalog.Texture> pair in TextureCatalog.Current.OrderBy(x => x.Key))
            {
                if (Assets.TryGetImagePixelSize(pair.Value.File, out int pixelWidth, out int pixelHeight))
                    TextureCatalog.UpdateTextureSize(pair.Key, pixelWidth, pixelHeight);

                ImageSource? preview = Assets.GetImage(pair.Value.File, chromaKey: true);
                lbStaticPalette.Items.Add(new StaticPaletteItem(pair.Key, pair.Value.Name, pair.Value.Width, pair.Value.Height, preview));
            }

            if (preserveSelection)
            {
                foreach (StaticPaletteItem item in lbStaticPalette.Items.OfType<StaticPaletteItem>())
                {
                    if (item.TextureId == previous)
                    {
                        lbStaticPalette.SelectedItem = item;
                        UpdatingUi = previousUpdating;
                        return;
                    }
                }
            }

            lbStaticPalette.SelectedItem = null;
            UpdatingUi = previousUpdating;
        }

        private void RefreshObjectList()
        {
            if (lbObjects == null)
                return;
            UpdatingUi = true;
            lbObjects.Items.Clear();
            if (LevelData != null)
            {
                for (int i = 0; i < Constants.ObjectCount; i++)
                {
                    ObjectRecord obj = LevelData.Object(i);
                    int kind = obj.GetInt(12);
                    if (!ShouldShowObjectKind(kind))
                        continue;
                    string name = kind == 1 ? TextureCatalog.TextureName(obj.GetInt(18)) : Helpers.ObjectKindName(kind);
                    string label = $"[{i:000}] f12={kind,2} {name}  ({obj.GetFloat(1):0.#},{obj.GetFloat(2):0.#})";
                    if (!string.IsNullOrWhiteSpace(tbObjectSearch?.Text) && !label.Contains(tbObjectSearch.Text, StringComparison.OrdinalIgnoreCase)) continue;
                    lbObjects.Items.Add(new ObjectListItem(i, label));
                }
                if (Tool.SelectedObjectIndex is int selected)
                {
                    foreach (ObjectListItem item in lbObjects.Items.OfType<ObjectListItem>())
                    {
                        if (item.Index == selected)
                        {
                            lbObjects.SelectedItem = item;
                            break;
                        }
                    }
                }
            }
            UpdatingUi = false;
        }

        private void UpdateLevelInfo()
        {
            if (LevelData == null)
            {
                tbLevelInfo.Text = "No level loaded";
                return;
            }
            tbTimeLimit.Text = LevelData.TimeLimit.ToString(System.Globalization.CultureInfo.InvariantCulture);
            tbTeleporters.Text = string.Join(",", Enumerable.Range(0, 16).Select(LevelData.TeleporterDestination).Where(i => i >= 0));
            string source = LevelData.SourcePath ?? "new/unsaved";
            string pack = CurrentPackLevel == null ? "Standalone" : $"Pack level: {CurrentPackLevel.LevelSet} / {CurrentPackLevel.Name}";
            tbLevelInfo.Text = $"Name: {LevelData.Name}\nVersion: {LevelData.Version} ({LevelData.HeaderUnknown0},{LevelData.HeaderUnknown1})\nTileset: {TextureCatalog.CurrentTilesetName}\nBackground: {TextureCatalog.CurrentBackgroundFile()}\nSource: {source}\n{pack}";
        }

        private void UpdateSelectedInfo()
        {
            if (LevelData == null || Tool.SelectedObjectIndex is not int index)
            {
                tbSelectedInfo.Text = "Nothing selected";
                bApplyAnimal.IsEnabled = cbSelectedFacing.IsEnabled = cbSelectedAnger.IsEnabled = false;
                return;
            }
            ObjectRecord obj = LevelData.Object(index);
            bApplyAnimal.IsEnabled = cbSelectedFacing.IsEnabled = AnimalFactory.IsAnimal(obj.GetInt(12));
            cbSelectedAnger.IsEnabled = obj.GetInt(12) == 2;
            cbSelectedFacing.SelectedIndex = AnimalFactory.IsAnimal(obj.GetInt(12)) ? Math.Clamp(obj.GetInt(20), 0, 3) : 0;
            cbSelectedAnger.SelectedIndex = obj.GetInt(12) == 2 ? Math.Clamp(obj.GetInt(22) / 2, 0, 5) : 3;
            tbObjectX.Text = obj.GetFloat(1).ToString(System.Globalization.CultureInfo.InvariantCulture);
            tbObjectY.Text = obj.GetFloat(2).ToString(System.Globalization.CultureInfo.InvariantCulture);
            tbObjectDepth.Text = obj.GetInt(24).ToString();
            tbObjectFields.Text = string.Join(",", obj.GetInts().Skip(1));
            int kind = obj.GetInt(12);
            tbSelectedInfo.Text = $"object[{index}]\nf12 kind = {kind} {Helpers.ObjectKindName(kind)}\ntex f18  = {obj.GetInt(18)} {TextureCatalog.TextureName(obj.GetInt(18))}\npos     = {obj.GetFloat(1):0.###}, {obj.GetFloat(2):0.###}\nsize    = {obj.GetInt(6)} x {obj.GetInt(7)}\nfield20 = {obj.GetInt(20)}\nfield21 = {obj.GetInt(21)}\nfield22 = {obj.GetInt(22)}\ndepth offset f24 = {obj.GetInt(24)}\ndraw depth = {DrawOrder.Depth(obj):0.###}";
        }

        private bool ShouldShowObjectKind(int kind)
        {
            if (!Helpers.IsVisibleObjectKind(kind))
                return false;
            return kind switch
            {
                0 => cbLayerSven.IsChecked == true,
                1 => cbLayerStatics.IsChecked == true,
                2 or 11 or 12 or 13 => cbLayerSheep.IsChecked == true,
                3 or 4 or 5 or 6 => cbLayerEnemies.IsChecked == true,
                _ => cbLayerUnknown.IsChecked == true,
            };
        }

        private List<int> HitTestObjects(Point point)
        {
            List<int> hits = new();
            if (LevelData == null)
                return hits;

            foreach (int i in DrawOrder.Indices(LevelData).AsEnumerable().Reverse())
            {
                ObjectRecord obj = LevelData.Object(i);
                if (ShouldShowObjectKind(obj.GetInt(12)) && Helpers.ObjectRect(obj).Contains(point)) hits.Add(i);
            }

            return hits;
        }

        private int? HitTestTopObject(Point point)
        {
            List<int> hits = HitTestObjects(point);
            return hits.Count == 0 ? null : hits[0];
        }

        private void ShowObjectPicker(Point point)
        {
            if (LevelData == null)
                return;
            List<int> hits = HitTestObjects(point);
            if (hits.Count == 0)
                return;
            ContextMenu menu = new();
            foreach (int index in hits)
            {
                ObjectRecord obj = LevelData.Object(index);
                int kind = obj.GetInt(12);
                string name = kind == 1 ? TextureCatalog.TextureName(obj.GetInt(18)) : Helpers.ObjectKindName(kind);
                MenuItem item = new() { Header = $"[{index:000}] {name}" };
                item.Click += (_, _) => SelectObject(index);
                menu.Items.Add(item);
            }
            menu.IsOpen = true;
        }
    }
}
