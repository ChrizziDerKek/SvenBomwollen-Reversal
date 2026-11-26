#include "LevelEditor.h"
#include "EditorGraphics.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

namespace
{
// Matches GameUi and the supplied arrow sprite.
constexpr POINT CursorHotspot{ 9, 9 };
using EditorGraphics::FitText;
using EditorGraphics::Rectangle;
i32 Snap(i32 value)
{
	return static_cast<i32>(std::round(static_cast<f32>(value) / TileSize)) * TileSize;
}
using TileBrush = PlacementPalette::TileBrush;
const std::vector<TileBrush>& TileBrushes = PlacementPalette::GetTileBrushes();
str Number(f32 value)
{
	std::ostringstream text;
	text << std::setprecision(7) << value;
	return text.str();
}
f32 ParseNumber(const str& text)
{
	size_t count = 0;
	const f32 value = std::stof(text, &count);
	if (count != text.size() || !std::isfinite(value))
		throw std::runtime_error("Enter a finite number without trailing characters.");
	return value;
}
i32 ParseInteger(const str& text)
{
	size_t count = 0;
	const i32 value = std::stoi(text, &count);
	if (count != text.size())
		throw std::runtime_error("Enter an integer without trailing characters.");
	return value;
}
} // namespace

LevelEditor::LevelEditor(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, size_t players, bool startBlank)
    : Engine(engine), Project(engine, pack, level), Font(engine, true, false, engine.GetData().GetThemeEdition()), Canvas(Rectangle(800, 600, 0xFF303030, false)), Bar(Rectangle(800, 26, 0xFFEDE4CE, false)), Selection(Rectangle(40, 40, 0xFF00DD00, true)), Wall(Rectangle(40, 40, 0xFFFF3030, true)), GridCell(Rectangle(40, 40, 0xFF809070, true)), ActiveTool(Rectangle(100, 26, 0xFFB8D498, false)), WaterCell(Rectangle(40, 40, 0xFF2266FF, true)), PuddleCell(Rectangle(40, 40, 0xFF00DDDD, true)), SpawnerCell(Rectangle(40, 40, 0xFFFFB000, true)), Panel(Rectangle(PanelWidth, 1, 0xFFEDE4CE, false)), FieldBar(Rectangle(PanelWidth - 8, FieldHeight - 2, 0xFFD8CCA8, false)), PanelTab(Rectangle(PanelWidth / 4, TabsHeight, 0xFFB8D498, false)), ListPanel(Rectangle(ListWidth, HeaderHeight + (ListRows + 2) * ListRowHeight, 0xFFEDE4CE, false)), ListHeader(Rectangle(ListWidth, HeaderHeight, 0xFFB8D498, false)), ListRow(Rectangle(ListWidth - 8, ListRowHeight, 0xFFD8CCA8, false)), AnchorMark(Rectangle(7, 7, 0xFFFFFF00, true)), Cursor(engine.LoadSprite(engine.GetData().ThemeAsset("Sven2/gfx/mousecursor.bmp"), 5, 1)), Players(players)
{
	if (engine.GetData().HasEdition(GameEdition::XXX))
	{
		const str music = engine.GetData().EditionAsset(GameEdition::XXX, "SvenXXX/sfx/editor.wav");
		try
		{
			engine.GetData().AssetPath(music);
			EditorMusic = music;
		}
		catch (const std::exception& error)
		{
			Log(str("Editor music unavailable; using level music: ") + error.what());
		}
	}
	if (startBlank)
		Project.New();
	BuildChoices();
}

LevelEditor::~LevelEditor()
{
	Preview.reset();
	if (!PreviewFolder.empty())
	{
		std::error_code error;
		std::filesystem::remove(PreviewFolder / L"level.lvl", error);
		std::filesystem::remove(PreviewFolder / L"level.lvl.tmp", error);
		std::filesystem::remove(PreviewFolder, error);
	}
}

void LevelEditor::BuildChoices()
{
	Choices.clear();
	ChoiceIndex = 0;
	if (CurrentCategory == Category::Scenery)
	{
		const TileSetInfo& tiles = Engine.GetData().TileSet(GetDocument().GetPack(), GetDocument().GetInfo());
		for (size_t index = 0; index < tiles.Tiles.size(); ++index)
			Choices.push_back({ ObjectKind::Scenery, static_cast<i32>(index), std::filesystem::u8path(tiles.Tiles[index].File).stem().u8string() });
	}
	else if (CurrentCategory == Category::Items)
	{
		for (const ItemDefinition& item : GetItemDefinitions())
			Choices.push_back({ item.Kind, 0, item.Name });
	}
	else
	{
		for (const CharacterDefinition& character : GetCharacterDefinitions())
		{
			if (character.XxxOnly && Engine.GetData().GetGameplayEdition() != GameEdition::XXX)
				continue;
			const bool include = (CurrentCategory == Category::Animals && character.Category == CharacterCategory::Animal) || (CurrentCategory == Category::Enemies && character.Category == CharacterCategory::Enemy) || (CurrentCategory == Category::Players && character.Category == CharacterCategory::Player) || (CurrentCategory == Category::Special && (character.Category == CharacterCategory::Mermaid || character.Category == CharacterCategory::Emitter));
			if (include)
				Choices.push_back({ character.Kind, 0, character.Name });
		}
	}
}

void LevelEditor::BuildObjects()
{
	Objects.clear();
	for (size_t slot = 0; slot < LevelFile::ObjectCount; ++slot)
	{
		const LevelObjectRecord& record = GetDocument().GetLevel().GetObjects()[slot];
		if (record.Kind() == static_cast<i32>(ObjectKind::Inactive))
			continue;
		if (!ObjectVisible(static_cast<ObjectKind>(record.Kind())))
			continue;
		const Sprite* image = GetDocument().GetSprite(record);
		if (!image)
			continue;
		const ObjectKind kind = static_cast<ObjectKind>(record.Kind());
		i32 frame = kind == ObjectKind::Scenery || IsItem(kind) ? record.Frame() : record.Facing();
		if (kind == ObjectKind::Mermaid)
			frame = 11;
		if (kind == ObjectKind::BubbleEmitter || kind == ObjectKind::YellowEmitter)
			frame = 15;
		SpriteDraw draw{ image, static_cast<i32>(record.X()), static_cast<i32>(record.Y()), std::clamp(frame, 0, image->Rows * image->Columns - 1), kind != ObjectKind::Scenery && record.Mirrored() };
		Objects.push_back({ slot, draw, record.Y() + record.Height() - record.DepthAdjustment(), kind == ObjectKind::Scenery ? record.RenderPass() : 0 });
	}
	const auto foreground = std::stable_partition(Objects.begin(), Objects.end(), [](const VisibleObject& object) { return object.Pass == 0; });
	const size_t count = static_cast<size_t>(std::distance(Objects.begin(), foreground));
	// Match gameplay's pair swaps, including equal-depth ties; foreground stays in slot order.
	for (size_t i = 0; i < count; ++i)
	{
		for (size_t j = i + 1; j < count; ++j)
		{
			if (Objects[i].Depth > Objects[j].Depth)
				std::swap(Objects[i], Objects[j]);
		}
	}
}

i32 LevelEditor::Pick(POINT point, bool cycle, bool sceneryOnly)
{
	BuildObjects();
	std::vector<i32> hits;
	for (auto object = Objects.rbegin(); object != Objects.rend(); ++object)
	{
		if (sceneryOnly && GetDocument().GetLevel().GetObjects()[object->Slot].Kind() != static_cast<i32>(ObjectKind::Scenery))
			continue;
		const SpriteDraw& draw = object->Draw;
		const Sprite& sprite = *draw.Sprite;
		i32 x = point.x - draw.X, y = point.y - draw.Y;
		if (x < 0 || y < 0 || x >= sprite.FrameWidth || y >= sprite.FrameHeight)
			continue;
		if (draw.Mirror)
			x = sprite.FrameWidth - x - 1;
		x += draw.Frame % sprite.Columns * sprite.FrameWidth;
		y += draw.Frame / sprite.Columns * sprite.FrameHeight;
		if (!sprite.Keyed || (sprite.Pixels[y * sprite.Width + x] & 0xFFFFFF) != 0xFF00FF)
			hits.push_back(static_cast<i32>(object->Slot));
	}
	if (hits.empty())
		return -1;
	const auto selected = std::find(hits.begin(), hits.end(), Selected);
	return cycle && selected != hits.end() ? hits[(std::distance(hits.begin(), selected) + 1) % hits.size()] : hits.front();
}

void LevelEditor::Save()
{
	FinishPaintStroke();
	const str path = Project.Save();
	Saved = true;
	Notice = "Saved " + path;
}

void LevelEditor::ExportFile(Property property)
{
	FinishPaintStroke();
	const bool level = property == Property::ExportLevel;
	wchar_t filename[32768]{};
	wcscpy_s(filename, level ? L"level.lvl" : (property == Property::ExportObjects ? L"objects.csv" : L"cells.csv"));
	const fpath directory = Engine.GetData().GetRoot().parent_path();
	OPENFILENAMEW dialog{};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = Engine.WindowHandle();
	dialog.lpstrFilter = level ? L"Sven level (*.lvl)\0*.lvl\0" : L"CSV inventory (*.csv)\0*.csv\0";
	dialog.lpstrDefExt = level ? L"lvl" : L"csv";
	dialog.lpstrFile = filename;
	dialog.nMaxFile = static_cast<DWORD>(std::size(filename));
	dialog.lpstrTitle = level ? L"Export current level" : (property == Property::ExportObjects ? L"Export object inventory" : L"Export cell inventory");
	dialog.lpstrInitialDir = directory.c_str();
	dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	if (!GetSaveFileNameW(&dialog))
	{
		if (CommDlgExtendedError() != 0)
			throw std::runtime_error("Cannot open the export filename dialog.");
		return;
	}
	if (level)
		GetDocument().ExportLevel(filename);
	else
		GetDocument().ExportInventory(filename, property == Property::ExportObjects ? LevelDocument::InventoryKind::Objects : LevelDocument::InventoryKind::Cells);
	Notice = "Exported " + fpath(filename).u8string() + ". Ctrl+S still saves the full project.";
}

void LevelEditor::Playtest()
{
	FinishPaintStroke();
	if (Preview)
	{
		Preview.reset();
		Notice = "Returned to the unchanged edit document.";
		return;
	}
	if (PreviewFolder.empty())
	{
		const str id = "_EditorPreview_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(GetTickCount64());
		const str relative = GetDocument().GetPack().AssetPrefix + id;
		PreviewFolder = Engine.GetData().GetRoot() / std::filesystem::u8path(relative);
		if (!std::filesystem::create_directory(PreviewFolder))
		{
			PreviewFolder.clear();
			throw std::runtime_error("Cannot reserve a private playtest directory.");
		}
		PreviewMap = relative + "/level.lvl";
	}
	const LevelInfo level = GetDocument().WritePreview(PreviewMap);
	PreviewPack = GetDocument().GetPack();
	PreviewSet.Name = "Editor playtest";
	PreviewSet.Levels = { level };
	Preview = std::make_unique<GameSession>(Engine, PreviewPack, PreviewSet, Players);
	Notice = "F5 or Escape: return to editing. Playtest scores and progress are temporary.";
}

bool LevelEditor::Handle(const WindowInput::Result& input, f32 seconds, const std::array<PlayerInput, 2>& players)
{
	Mouse = input.Mouse;
	WorldMouse = Viewport.WorldAt(Mouse);
	if (!Preview && Panning)
	{
		if (Mouse.x >= 0 && Mouse.x < 800 && Mouse.y >= 0 && Mouse.y < 600 && PanOrigin.x >= 0 && PanOrigin.x < 800 && PanOrigin.y >= 0 && PanOrigin.y < 600)
			Viewport.Pan(Mouse.x - PanOrigin.x, Mouse.y - PanOrigin.y);
		PanOrigin = Mouse;
		Panning = input.MiddleHeld;
		WorldMouse = Viewport.WorldAt(Mouse);
		return false;
	}
	try
	{
		if (Painting)
		{
			const bool held = Painting->Erase ? input.RightHeld : input.MouseHeld;
			const bool command = input.Back || input.Editor || input.Playtest || input.SaveEditor || input.Undo || input.Redo || input.Delete || input.Erase || input.Tab || input.Left || input.Right || input.Up || input.Down || input.MiddleHeld || input.Wheel != 0 || input.BindingKey >= 0;
			if (command)
			{
				FinishPaintStroke();
				WindowInput::Result action = input;
				action.Click = action.RightClick = action.MouseHeld = action.RightHeld = false;
				const bool close = input.Back ? false : Handle(action, seconds, players);
				UiPointerHeld = input.MouseHeld || input.RightHeld;
				return close;
			}
			if (!held)
			{
				FinishPaintStroke();
				UiPointerHeld = input.MouseHeld || input.RightHeld;
			}
			else
				ContinuePaintStroke();
			return false;
		}
		if (!Preview && Hud)
		{
			if (UiPointerHeld && !MovingPalette && !MovingList)
			{
				UiPointerHeld = input.MouseHeld || input.RightHeld;
				if (UiPointerHeld)
					return false;
			}
			if (PaletteVisible && Palette.IsRaw())
			{
				Raw.Sync(GetDocument(), Selected, SelectedCell);
				if (Raw.Handle(input, GetDocument(), InspectorBounds()))
				{
					if (input.Click || input.Enter || input.Erase || !input.Text.empty())
						DiscardArmed = false;
					CurrentTool = Tool::Select;
					SelectCells = Raw.IsCell();
					if (SelectCells)
						Selected = -1;
					if (input.Click)
						UiPointerHeld = input.MouseHeld;
					Dragging = false;
					return false;
				}
			}
			if (input.Click)
				Palette.Blur();
			if (PaletteVisible && Palette.HasSearchFocus() && !input.Click)
			{
				if (HandlePalette(input))
					return false;
			}
			if (MovingPalette)
			{
				HandlePalette(input);
				return false;
			}
			const RECT sidebar = PaletteBounds();
			if (PaletteVisible && input.Click && Mouse.x >= sidebar.left && Mouse.x < sidebar.right && Mouse.y >= sidebar.top && Mouse.y < sidebar.top + PlacementPalette::HeaderHeight)
			{
				HandlePalette(input);
				return false;
			}
			if (Inspector && PaletteVisible && Picker != PickerKind::None)
			{
				if (HandleInspector(input))
					return false;
			}
			else if (HandleObjectList(input) || HandleInspector(input) || HandlePalette(input))
				return false;
		}
		if (input.BindingKey == 'h' && !input.Control)
		{
			Hud = !Hud;
			Dragging = MovingPalette = MovingList = UiPointerHeld = false;
			ListSearchFocused = false;
		}
		if (input.Back && !Preview && (CurrentTool != Tool::Select || Selected >= 0 || Dragging || SelectCells))
		{
			CurrentTool = Tool::Select;
			Selected = -1;
			Dragging = SelectCells = DiscardArmed = false;
			Notice = "Selection mode. Click an object to select it.";
			return false;
		}
		if (input.Back || input.Editor)
		{
			if (Preview)
			{
				Playtest();
				return false;
			}
			if (Project.IsDirty() && !DiscardArmed)
			{
				DiscardArmed = true;
				Hud = Toolbar = true;
				Notice = "Unsaved edits: Escape again discards; Ctrl+S saves a separate pack.";
				return false;
			}
			return true;
		}
		if ((input.Click && LevelDocument::TileAt(Mouse.x, Mouse.y) >= 0 && !(ToolbarVisible() && Mouse.y < 26 && Mouse.x >= 700)) || input.SaveEditor || input.Undo || input.Redo || input.Playtest || (input.BindingKey >= 32 && input.BindingKey < 256))
			DiscardArmed = false;
		if (input.Playtest)
		{
			Playtest();
			return false;
		}
		if (Preview)
		{
			Preview->Update(seconds, players);
			return false;
		}
		if (CanvasAreaAt(Mouse) && (input.MiddleHeld || (input.Control && input.Wheel != 0)))
		{
			Dragging = false;
			if (input.MiddleHeld)
			{
				Panning = true;
				PanOrigin = Mouse;
			}
			else
				Viewport.ZoomAt(Mouse, input.Wheel);
			WorldMouse = Viewport.WorldAt(Mouse);
			Notice = "Zoom " + std::to_string(static_cast<i32>(Viewport.GetZoom() * 100)) + "% | Ctrl+wheel zoom | middle drag pan | 0 resets";
			return false;
		}
		if (input.BindingKey == '0' && !input.Control)
		{
			Viewport.Reset();
			Notice = "View reset to 100%. Ctrl+wheel zooms; middle drag pans.";
			WorldMouse = Viewport.WorldAt(Mouse);
		}
		if (input.BindingKey == 'l' && !input.Control)
		{
			ShowInspector(false);
			Palette.Show(PlacementPalette::Group::View);
		}
		if (input.BindingKey == 'b' && !input.Control)
		{
			PaletteVisible = !Hud || !PaletteVisible;
			Hud = true;
			if (PaletteVisible)
				ObjectBrowser = false;
			Palette.Blur();
			Dragging = false;
		}
		if (input.BindingKey == 'o' && !input.Control)
		{
			ObjectBrowser = !Hud || !ObjectBrowser;
			if (ObjectBrowser)
				PaletteVisible = false;
			Hud = true;
			Dragging = false;
			ListSearchFocused = false;
		}
		if (input.BindingKey == 'a' && !input.Control)
		{
			CenterScenery = !CenterScenery;
			Notice = CenterScenery ? "Scenery anchor: visible center. A switches to canvas origin." : "Scenery anchor: canvas origin. A switches to visible center.";
		}
		if (input.Tab)
			Toolbar = !Toolbar;
		if (input.BindingKey == 'i' && !input.Control)
		{
			ShowInspector(!Hud || !PaletteVisible || !Inspector);
			Hud = true;
			Dragging = false;
		}
		if (input.BindingKey == 'n' && !input.Control)
		{
			SnapScenery = !SnapScenery;
			Notice = SnapScenery ? "Scenery snaps to 40px. N toggles free placement." : "Free scenery: pixel drag/nudge; Shift+arrows moves 10px. Actors remain on grid.";
		}
		if (input.SaveEditor)
			Save();
		if (input.Undo)
		{
			GetDocument().Undo();
			Selected = -1;
			BuildChoices();
		}
		if (input.Redo)
		{
			GetDocument().Redo();
			Selected = -1;
			BuildChoices();
		}
		if (input.BindingKey == 'v')
		{
			CurrentTool = Tool::Select;
			SelectCells = false;
		}
		if (input.BindingKey == 't')
		{
			CurrentTool = Tool::Select;
			SelectCells = true;
			Selected = -1;
			InspectorTab = InspectorPage::Cell;
		}
		if (input.BindingKey == 'p')
			CurrentTool = Tool::Place;
		if (input.BindingKey == 'c')
		{
			CurrentTool = Tool::Collision;
			Selected = -1;
		}
		if ((input.Delete || input.Erase) && Selected >= 0)
		{
			GetDocument().Delete(Selected);
			Selected = -1;
		}
		if (input.BindingKey == 'r' && Selected >= 0)
			GetDocument().Rotate(Selected);
		if (input.RightClick && CurrentTool != Tool::Collision)
		{
			Dragging = false;
			Selected = -1;
			const bool sceneryTool = CurrentTool == Tool::Place && !Choices.empty() && Choices[ChoiceIndex].Kind == ObjectKind::Scenery;
			if ((CurrentTool == Tool::Select || sceneryTool) && CanvasAt(Mouse))
			{
				const i32 target = Pick(WorldMouse, false, sceneryTool);
				if (target >= 0 && GetDocument().GetLevel().GetObjects()[target].Kind() == static_cast<i32>(ObjectKind::Scenery))
				{
					GetDocument().Delete(target);
					Notice = "Scenery deleted. Ctrl+Z restores it.";
				}
			}
		}
		if (CurrentTool == Tool::Select && Selected >= 0 && (input.Left || input.Right || input.Up || input.Down))
		{
			const i32 step = !SnapScenery && GetDocument().GetLevel().GetObjects()[Selected].Kind() == static_cast<i32>(ObjectKind::Scenery) ? (input.Shift ? 10 : 1) : TileSize;
			GetDocument().Move(Selected, (static_cast<i32>(input.Right) - input.Left) * step, (static_cast<i32>(input.Down) - input.Up) * step, SnapScenery);
		}
		else if (input.Left || input.Right || input.Wheel != 0)
			ChangeChoice(input.Right ? 1 : (input.Left ? -1 : -input.Wheel));
		if (ToolbarVisible() && input.Click && Mouse.y >= 0 && Mouse.y < 26 && Mouse.x >= 0 && Mouse.x < 800)
		{
			UiPointerHeld = input.MouseHeld;
			Dragging = false;
			switch (Mouse.x / 100)
			{
			case 0:
				CurrentTool = Tool::Select;
				SelectCells = false;
				break;
			case 1:
				PaletteVisible = true;
				ObjectBrowser = false;
				Inspector = false;
				Palette.Show(PlacementPalette::Group::All);
				CurrentTool = Tool::Place;
				break;
			case 2:
				PaletteVisible = true;
				ObjectBrowser = false;
				Inspector = false;
				Palette.Show(PlacementPalette::Group::Tiles);
				CurrentTool = Tool::Collision;
				Selected = -1;
				break;
			case 3:
				GetDocument().Undo();
				Selected = -1;
				BuildChoices();
				break;
			case 4:
				GetDocument().Redo();
				Selected = -1;
				BuildChoices();
				break;
			case 5:
				Save();
				break;
			case 6:
				Playtest();
				break;
			case 7:
			{
				WindowInput::Result close;
				close.Editor = true;
				return Handle(close, 0);
			}
			}
			return false;
		}
		if ((input.Click || (input.RightClick && CurrentTool == Tool::Collision)) && CanvasAt(Mouse))
		{
			ResetArmed = false;
			SelectedCell = LevelDocument::TileAt(WorldMouse.x, WorldMouse.y);
			if (input.Shift || (CurrentTool == Tool::Select && SelectCells))
			{
				CurrentTool = Tool::Select;
				SelectCells = true;
				Selected = -1;
				Dragging = false;
				InspectorTab = InspectorPage::Cell;
			}
			else if (CurrentTool == Tool::Collision)
			{
				Selected = -1;
				GetDocument().BeginTileStroke();
				Painting = PaintGesture{ &GetDocument(), TileChoice, input.RightClick };
				Notice = input.RightClick ? "Erasing stroke; release to finish. Ctrl+Z undoes the stroke." : "Painting stroke; release to finish. Ctrl+Z undoes the stroke.";
				ContinuePaintStroke();
				if (!(input.RightClick ? input.RightHeld : input.MouseHeld))
					FinishPaintStroke();
			}
			else if (CurrentTool == Tool::Place && !Choices.empty())
			{
				const Placement placement = PlacementAt(WorldMouse);
				const POINT origin{ placement.Draw.X, placement.Draw.Y };
				const Choice& choice = Choices[ChoiceIndex];
				const CharacterDefinition* character = FindCharacterDefinition(choice.Kind);
				const LevelDocument::AnimalPlacement* animal = character && character->Category == CharacterCategory::Animal ? &Palette.GetAnimalPlacement() : nullptr;
				Selected = GetDocument().Place(choice.Kind, placement.Tile, choice.Texture, choice.Kind == ObjectKind::Scenery ? &origin : nullptr, Palette.GetSceneryOrder(), animal);
				if (choice.Kind == ObjectKind::Scenery)
				{
					SelectCells = false;
					Notice = "Scenery placed. Left-click places more; right-click deletes; Esc selects.";
				}
			}
			else
			{
				Selected = Pick(WorldMouse, input.Control);
				DragOrigin = WorldMouse;
				Dragging = Selected >= 0;
			}
		}
		if (Dragging && !input.MouseHeld)
		{
			Dragging = false;
			if (CanvasAt(Mouse))
				GetDocument().Move(Selected, WorldMouse.x - DragOrigin.x, WorldMouse.y - DragOrigin.y, SnapScenery);
		}
	}
	catch (const std::exception& error)
	{
		Dragging = false;
		Notice = error.what();
	}
	return false;
}

const std::vector<SpriteDraw>& LevelEditor::Draws()
{
	if (Preview)
	{
		Commands = Preview->Draws(Hud, Hud);
		if (Hud)
		{
			Commands.push_back({ &Bar, 0, 74, 0 });
			Font.Append(Commands, Preview->GetResults() ? "Playtest finished. F5 returns to editing." : "PLAYTEST - F5 returns; H hides HUD; progress is temporary.", 8, 78);
		}
	}
	else
	{
		Commands.clear();
		Commands.push_back({ &Canvas, 0, 0, 0 });
		const TileSetInfo& tiles = Engine.GetData().TileSet(GetDocument().GetPack(), GetDocument().GetInfo());
		if (IsLayerVisible(Layer::Background))
			Commands.push_back({ Engine.LoadSprite(tiles.Background, 1, 1, true), 0, 0, 0 });
		BuildObjects();
		for (const VisibleObject& object : Objects)
		{
			SpriteDraw draw = object.Draw;
			if (Dragging && static_cast<i32>(object.Slot) == Selected)
			{
				const bool snap = SnapScenery || GetDocument().GetLevel().GetObjects()[object.Slot].Kind() != static_cast<i32>(ObjectKind::Scenery);
				draw.X += snap ? Snap(WorldMouse.x - DragOrigin.x) : WorldMouse.x - DragOrigin.x;
				draw.Y += snap ? Snap(WorldMouse.y - DragOrigin.y) : WorldMouse.y - DragOrigin.y;
			}
			Commands.push_back(draw);
			if (IsLayerVisible(Layer::Labels))
				Font.Append(Commands, "#" + std::to_string(object.Slot), draw.X + 2, draw.Y + 2);
		}
		if (CurrentTool == Tool::Place && !Choices.empty() && CanvasAt(Mouse))
		{
			const i32 tile = LevelDocument::TileAt(WorldMouse.x, WorldMouse.y);
			if (tile >= 0)
			{
				try
				{
					const Placement placement = PlacementAt(WorldMouse);
					Commands.push_back(placement.Draw);
					Commands.push_back({ &AnchorMark, placement.Anchor.x - 3, placement.Anchor.y - 3, 0 });
				}
				catch (const std::exception& error)
				{
					Notice = error.what();
				}
			}
		}
		if (IsLayerVisible(Layer::Grid) || IsLayerVisible(Layer::Tiles))
		{
			for (i32 y = 0; y < Rows; ++y)
			{
				for (i32 x = 0; x < Columns; ++x)
				{
					const LevelCell& cell = GetDocument().GetLevel().GetCells()[(y + 1) * Stride + x + 1];
					if (IsLayerVisible(Layer::Grid))
						Commands.push_back({ &GridCell, x * TileSize, y * TileSize, 0 });
					const Sprite* outline = cell.Terrain == OccupancyCode::Wall ? &Wall : (cell.Terrain == OccupancyCode::Water ? &WaterCell : (cell.Terrain == OccupancyCode::Puddle ? &PuddleCell : &GridCell));
					if (IsLayerVisible(Layer::Tiles) && (cell.Terrain == OccupancyCode::Wall || cell.Terrain == OccupancyCode::Water || cell.Terrain == OccupancyCode::Puddle))
						Commands.push_back({ outline, x * TileSize, y * TileSize, 0 });
					if (IsLayerVisible(Layer::Tiles) && (cell.Terrain == OccupancyCode::Puddle || cell.Terrain == OccupancyCode::Water))
						Font.Append(Commands, cell.Terrain == OccupancyCode::Puddle ? "P" : "W", x * TileSize + 3, y * TileSize + 3);
					if (IsLayerVisible(Layer::Tiles) && IsSpawner(cell.Spawner))
					{
						Commands.push_back({ &SpawnerCell, x * TileSize, y * TileSize, 0 });
						Font.Append(Commands, cell.Spawner == SpawnerCode::Distraction ? "D" : (cell.Spawner == SpawnerCode::Powerup ? "E" : "M"), x * TileSize + 22, y * TileSize + 3);
					}
				}
			}
		}
		if (SelectedCell >= 0 && (SelectCells || (Inspector && InspectorTab == InspectorPage::Cell)))
			Commands.push_back({ &Selection, (SelectedCell % Stride - 1) * TileSize, (SelectedCell / Stride - 1) * TileSize, 0 });
		if (Selected >= 0 && GetDocument().GetLevel().GetObjects()[Selected].Kind() != static_cast<i32>(ObjectKind::Inactive) && ObjectVisible(static_cast<ObjectKind>(GetDocument().GetLevel().GetObjects()[Selected].Kind())))
		{
			const LevelObjectRecord& record = GetDocument().GetLevel().GetObjects()[Selected];
			const Sprite* image = GetDocument().GetSprite(record);
			if (image)
			{
				if (ObjectOutline.FrameWidth != image->FrameWidth || ObjectOutline.FrameHeight != image->FrameHeight)
					ObjectOutline = Rectangle(image->FrameWidth, image->FrameHeight, 0xFF00DD00, true);
				const bool snap = SnapScenery || record.Kind() != static_cast<i32>(ObjectKind::Scenery);
				const i32 dx = Dragging ? (snap ? Snap(WorldMouse.x - DragOrigin.x) : WorldMouse.x - DragOrigin.x) : 0;
				const i32 dy = Dragging ? (snap ? Snap(WorldMouse.y - DragOrigin.y) : WorldMouse.y - DragOrigin.y) : 0;
				Commands.push_back({ &ObjectOutline, static_cast<i32>(record.X()) + dx, static_cast<i32>(record.Y()) + dy, 0 });
			}
		}
		Viewport.Transform(Commands);
		if (ToolbarVisible())
		{
			Commands.push_back({ &Bar, 0, 0, 0 });
			Commands.push_back({ &ActiveTool, static_cast<i32>(CurrentTool) * 100, 0, 0 });
			constexpr cstr tools[] = { "Select", "Place", "Tiles", "Undo", "Redo", "Save copy", "Playtest", "Close" };
			for (i32 index = 0; index < 8; ++index)
				Font.Append(Commands, tools[index], index * 100 + 6, 5);
		}
		if (ToolbarVisible())
		{
			Commands.push_back({ &Bar, 0, 574, 0 });
			const str status = (Project.IsDirty() ? "* " : "") + Notice;
			Font.Append(Commands, FitText(Font, status, 788), 6, 579);
		}
		if (Hud && PaletteVisible)
		{
			if (!Choices.empty())
				Palette.Select(CurrentTool == Tool::Collision ? ObjectKind::Inactive : Choices[ChoiceIndex].Kind, CurrentTool == Tool::Collision ? TileChoice : Choices[ChoiceIndex].Texture);
			Palette.Append(Commands, GetDocument(), PaletteBounds(), Font);
		}
		if (Hud && PaletteVisible && Inspector && Picker == PickerKind::None)
			DrawInspector();
		if (Hud && PaletteVisible && Palette.IsView())
			DrawView();
		if (Hud && PaletteVisible && Palette.IsRaw())
		{
			Raw.Sync(GetDocument(), Selected, SelectedCell);
			Raw.Append(Commands, GetDocument(), InspectorBounds(), Font);
		}
		if (Hud && ObjectBrowser)
			DrawObjectList();
		if (Hud && PaletteVisible && Inspector && Picker != PickerKind::None)
			DrawInspector();
	}
	if (Mouse.x >= 0 && Mouse.x < 800 && Mouse.y >= 0 && Mouse.y < 600)
		Commands.push_back({ Cursor, Mouse.x - CursorHotspot.x, Mouse.y - CursorHotspot.y, 0 });
	return Commands;
}

std::vector<SoundRequest> LevelEditor::TakeAudioEvents()
{
	return Preview ? Preview->TakeAudioEvents() : std::vector<SoundRequest>{};
}

bool LevelEditor::RequestWindowClose()
{
	FinishPaintStroke();
	if ((Project.IsDirty() || EditingField || Raw.HasFocus()) && !DiscardArmed)
	{
		Preview.reset();
		DiscardArmed = true;
		Hud = Toolbar = true;
		Notice = "Unsaved edits: close again to quit/discard, or Ctrl+S to save.";
		return false;
	}
	return true;
}

bool LevelEditor::CanvasAreaAt(POINT point) const
{
	return point.x >= 0 && point.x < 800 && point.y >= 0 && point.y < 600 && (!ToolbarVisible() || (point.y >= 26 && point.y < 574)) && !(Hud && PaletteVisible && Inspector && point.x >= InspectorBounds().left && point.x < InspectorBounds().right && point.y >= InspectorBounds().top && point.y < InspectorBounds().bottom) && !(Hud && ObjectBrowser && point.x >= ObjectListBounds().left && point.x < ObjectListBounds().right && point.y >= ObjectListBounds().top && point.y < ObjectListBounds().bottom) && !(Hud && PaletteVisible && point.x >= PaletteBounds().left && point.x < PaletteBounds().right && point.y >= PaletteBounds().top && point.y < PaletteBounds().bottom);
}

bool LevelEditor::CanvasAt(POINT point) const
{
	const POINT world = Viewport.WorldAt(point);
	return CanvasAreaAt(point) && LevelDocument::TileAt(world.x, world.y) >= 0;
}

LevelEditor::Layer LevelEditor::ObjectLayer(ObjectKind kind) const
{
	if (kind == ObjectKind::Scenery)
		return Layer::Scenery;
	if (IsItem(kind))
		return Layer::Items;
	const CharacterDefinition* definition = FindCharacterDefinition(kind);
	if (!definition)
		return Layer::Special;
	switch (definition->Category)
	{
	case CharacterCategory::Player:
		return Layer::Players;
	case CharacterCategory::Animal:
		return Layer::Animals;
	case CharacterCategory::Enemy:
		return Layer::Enemies;
	default:
		return Layer::Special;
	}
}

bool LevelEditor::ObjectVisible(ObjectKind kind) const
{
	return IsLayerVisible(ObjectLayer(kind));
}

void LevelEditor::ChangeChoice(i32 step)
{
	const i32 count = static_cast<i32>(CurrentTool == Tool::Collision ? TileBrushes.size() : Choices.size());
	if (!count)
		return;
	i32& choice = CurrentTool == Tool::Collision ? TileChoice : ChoiceIndex;
	choice = (choice + step % count + count) % count;
}

std::vector<LevelEditor::Field> LevelEditor::Fields() const
{
	const LevelFile& level = GetDocument().GetLevel();
	if (InspectorTab == InspectorPage::Level)
	{
		str destinations;
		for (i32 tile : level.GetPuddles())
			destinations += (destinations.empty() ? "" : ",") + std::to_string(tile);
		std::vector<Field> fields = {
			{ Property::Title, "Level title", GetDocument().GetInfo().Name },
			{ Property::Time, "Timer (sec)", Number(level.GetTimeLimit()) },
			{ Property::TileSet, "Theme", GetDocument().GetThemes()[GetDocument().GetThemeIndex()].Name, FieldMode::Action },
			{ Property::Music, "Music path", GetDocument().GetInfo().Music },
			{ Property::Ufo, "Show UFO", level.GetHidesUfo() ? "No" : "Yes", FieldMode::Cycle },
			{ Property::Destinations, "Puddle cells", destinations },
			{ Property::Rebuild, "Puddle table", "Rebuild all", FieldMode::Action }
		};
		if (level.GetVersion() == 7)
		{
			constexpr cstr weather[] = { "None", "Snow", "Rain", "Bubbles" };
			constexpr cstr bonus[] = { "Sven", "Farmer", "Dog (legacy)", "Girl", "Alien" };
			fields.push_back({ Property::Weather, "Weather", weather[static_cast<i32>(level.GetWeatherMode())], FieldMode::Cycle });
			fields.push_back({ Property::Bonus, "Controller", bonus[static_cast<i32>(level.GetBonusMode())], FieldMode::Cycle });
		}
		return fields;
	}
	if (InspectorTab == InspectorPage::Cell)
	{
		if (SelectedCell < 0)
			return {};
		const LevelCell& cell = level.GetCells()[SelectedCell];
		str terrain = std::to_string(static_cast<i32>(cell.Terrain));
		for (const TileBrush& brush : TileBrushes)
			if (!brush.SpawnerOnly && brush.Terrain == cell.Terrain)
				terrain = brush.Name;
		str spawner = "None";
		for (const TileBrush& brush : TileBrushes)
			if (brush.SpawnerOnly && brush.Spawner == cell.Spawner && IsSpawner(cell.Spawner))
				spawner = brush.Name;
		std::vector<Field> fields = {
			{ Property::CellIndex, "Raw cell", std::to_string(SelectedCell), FieldMode::Text },
			{ Property::Terrain, "Terrain", terrain, FieldMode::Cycle },
			{ Property::Spawner, "Spawner", spawner, FieldMode::Cycle },
			{ Property::Direction, "Direction 0..7", std::to_string(cell.Direction) }
		};
		if (level.GetVersion() == 7)
			fields.push_back({ Property::Help, "Help ID", std::to_string(cell.HelpId) });
		return fields;
	}
	if (InspectorTab == InspectorPage::Project)
	{
		return {
			{ Property::PackName, "Pack name", Project.GetName() },
			{ Property::Episode, "Levelset", Project.GetEntries()[Project.GetActive()].Episode },
			{ Property::ChooseLevel, "Project levels", std::to_string(Project.GetActive() + 1) + "/" + std::to_string(Project.GetEntries().size()), FieldMode::Action },
			{ Property::NewLevel, "Add blank level", "Create", FieldMode::Action },
			{ Property::Duplicate, "Duplicate level", "Create copy", FieldMode::Action },
			{ Property::ImportLevel, "Import .lvl", "Browse...", FieldMode::Action },
			{ Property::NewPack, "New blank pack", "Create...", FieldMode::Action },
			{ Property::OpenPack, "Open full pack", "Browse copies...", FieldMode::Action },
			{ Property::ExportLevel, "Export .lvl", "Save as...", FieldMode::Action },
			{ Property::ExportObjects, "Object CSV", "Save as...", FieldMode::Action },
			{ Property::ExportCells, "Cell CSV", "Save as...", FieldMode::Action }
		};
	}
	if (Selected < 0 || level.GetObjects()[Selected].Kind() == static_cast<i32>(ObjectKind::Inactive))
		return { { Property::FindObject, "Object list", "Browse...", FieldMode::Action } };
	const LevelObjectRecord& record = level.GetObjects()[Selected];
	const ObjectKind kind = static_cast<ObjectKind>(record.Kind());
	const CharacterDefinition* definition = FindCharacterDefinition(kind);
	std::vector<Field> fields = {
		{ Property::Slot, "Object slot", std::to_string(Selected), FieldMode::ReadOnly },
		{ Property::Kind, "Kind", definition ? definition->Name : std::to_string(record.Kind()), FieldMode::ReadOnly },
		{ Property::X, "X (px)", Number(record.X()) },
		{ Property::Y, "Y (px)", Number(record.Y()) },
		{ Property::Depth, "Depth offset", GetDocument().DrawsAtBottom(Selected) ? "Floor" : std::to_string(record.DepthAdjustment()), GetDocument().DrawsAtBottom(Selected) ? FieldMode::ReadOnly : FieldMode::Text }
	};
	if (kind == ObjectKind::Scenery)
	{
		fields.push_back({ Property::Pass, "Render pass", record.RenderPass() ? "Foreground" : "World sorted", FieldMode::Cycle });
		fields.push_back({ Property::Bottom, "Draw at bottom", GetDocument().DrawsAtBottom(Selected) ? "Yes" : "No", FieldMode::Cycle });
		fields.push_back({ Property::Frame, "Texture frame", std::to_string(record.Frame()) });
		fields.push_back({ Property::Texture, "Texture ID", std::to_string(record.TextureOrPermanent()) });
		fields.push_back({ Property::Earlier, "Draw earlier", GetDocument().DrawsAtBottom(Selected) ? "Floor" : "Apply", GetDocument().DrawsAtBottom(Selected) ? FieldMode::ReadOnly : FieldMode::Action });
		fields.push_back({ Property::Later, "Draw later", GetDocument().DrawsAtBottom(Selected) ? "Floor" : "Apply", GetDocument().DrawsAtBottom(Selected) ? FieldMode::ReadOnly : FieldMode::Action });
		fields.push_back({ Property::NaturalDepth, "Natural depth", "Reset", FieldMode::Action });
	}
	if (definition && (definition->Category == CharacterCategory::Animal || definition->Category == CharacterCategory::Enemy))
		fields.push_back({ Property::Facing, "Facing", std::to_string(record.Facing()), FieldMode::Cycle });
	if (definition && definition->Category == CharacterCategory::Animal)
		fields.push_back({ Property::Impatience, "Impatience 0..10", std::to_string(record.Impatience()) });
	if (IsItem(kind))
		fields.push_back({ Property::Permanent, "Permanent", record.TextureOrPermanent() ? "Yes" : "No", FieldMode::Cycle });
	fields.push_back({ Property::FindObject, "Object list", "Browse...", FieldMode::Action });
	return fields;
}

void LevelEditor::ApplyField(Property property, const str& text, i32 step)
{
	const LevelFile& level = GetDocument().GetLevel();
	if (property != Property::NewPack)
		ResetArmed = false;
	switch (property)
	{
	case Property::Title:
		GetDocument().SetMetadata(text, GetDocument().GetInfo().Music);
		break;
	case Property::Music:
		GetDocument().SetMetadata(GetDocument().GetInfo().Name, text);
		break;
	case Property::Time:
		GetDocument().SetTimeLimit(ParseNumber(text));
		break;
	case Property::TileSet:
		Picker = PickerKind::Theme;
		Search.clear();
		PickerFirst = PickerSelected = 0;
		break;
	case Property::Ufo:
		GetDocument().SetUfoVisible(level.GetHidesUfo());
		break;
	case Property::Weather:
		GetDocument().SetWeather(static_cast<WeatherMode>((static_cast<i32>(level.GetWeatherMode()) + step + 4) % 4));
		break;
	case Property::Bonus:
	{
		const std::vector<BonusMode> modes = { BonusMode::Sven, BonusMode::Farmer, BonusMode::Girl, BonusMode::Alien };
		const auto current = std::find(modes.begin(), modes.end(), level.GetBonusMode());
		const i32 index = current == modes.end() ? 0 : static_cast<i32>(std::distance(modes.begin(), current));
		GetDocument().SetBonusMode(modes[(index + step + 4) % 4]);
		break;
	}
	case Property::Destinations:
	{
		std::vector<i32> cells;
		std::istringstream list(text);
		str cell;
		while (std::getline(list, cell, ','))
			cells.push_back(ParseInteger(cell));
		GetDocument().SetTeleporters(cells);
		break;
	}
	case Property::Rebuild:
		GetDocument().RebuildTeleporters();
		break;
	case Property::CellIndex:
	{
		const i32 tile = ParseInteger(text);
		if (tile < 0 || tile >= static_cast<i32>(LevelFile::CellCount) || tile % Stride < 1 || tile % Stride > Columns || tile / Stride < 1 || tile / Stride > Rows)
			throw std::runtime_error("Choose a raw cell inside the visible level.");
		SelectedCell = tile;
		break;
	}
	case Property::Terrain:
	{
		const std::vector<OccupancyCode> types = { OccupancyCode::Empty, OccupancyCode::Wall, OccupancyCode::Puddle, OccupancyCode::Water };
		const auto current = std::find(types.begin(), types.end(), level.GetCells()[SelectedCell].Terrain);
		const i32 index = current == types.end() ? 0 : static_cast<i32>(std::distance(types.begin(), current));
		GetDocument().PaintTerrain(SelectedCell, types[(index + step + 4) % 4]);
		break;
	}
	case Property::Spawner:
	{
		const std::vector<SpawnerCode> types = { SpawnerCode::None, SpawnerCode::Distraction, SpawnerCode::Powerup, SpawnerCode::Mixed };
		const auto current = std::find(types.begin(), types.end(), level.GetCells()[SelectedCell].Spawner);
		const i32 index = current == types.end() ? 0 : static_cast<i32>(std::distance(types.begin(), current));
		GetDocument().SetSpawner(SelectedCell, types[(index + step + 4) % 4]);
		break;
	}
	case Property::Direction:
		GetDocument().SetCellDirection(SelectedCell, ParseInteger(text));
		break;
	case Property::Help:
		GetDocument().SetCellHelp(SelectedCell, ParseInteger(text));
		break;
	case Property::ExportLevel:
	case Property::ExportObjects:
	case Property::ExportCells:
		ExportFile(property);
		break;
	case Property::OpenPack:
		Engine.ReloadPacks();
		CatalogChanged = true;
		Picker = PickerKind::Pack;
		PendingPack = -1;
		Search.clear();
		PickerFirst = PickerSelected = 0;
		break;
	case Property::FindObject:
		ObjectBrowser = true;
		PaletteVisible = false;
		ListCollapsed = false;
		ListSearch.clear();
		ListFirst = 0;
		ListSearchFocused = true;
		break;
	case Property::ChooseLevel:
		Picker = PickerKind::Level;
		PendingDelete.reset();
		Search.clear();
		PickerFirst = PickerSelected = 0;
		FocusProjectLevel();
		break;
	case Property::PackName:
		Project.SetName(text);
		break;
	case Property::Episode:
		Project.SetEpisode(text);
		break;
	case Property::NewPack:
		if (Project.IsDirty() && !ResetArmed)
		{
			ResetArmed = true;
			Notice = "Unsaved project: save first, or click New blank pack again to discard.";
			break;
		}
		Project.New();
		ResetSelection();
		ResetArmed = false;
		Notice = "New blank pack. Set its name/levelset, place animals, then Ctrl+S.";
		break;
	case Property::NewLevel:
		Project.AddBlank();
		ResetSelection();
		Notice = "Blank level added to the project. Other levels keep their edits.";
		break;
	case Property::Duplicate:
		Project.Duplicate();
		ResetSelection();
		break;
	case Property::ImportLevel:
	{
		wchar_t path[32768]{};
		OPENFILENAMEW dialog{};
		dialog.lStructSize = sizeof(dialog);
		dialog.hwndOwner = Engine.WindowHandle();
		dialog.lpstrFilter = L"Sven levels (*.lvl)\0*.lvl\0All files\0*.*\0";
		dialog.lpstrFile = path;
		dialog.nMaxFile = static_cast<DWORD>(std::size(path));
		dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetOpenFileNameW(&dialog))
		{
			Project.Import(path);
			ResetSelection();
			Notice = "Imported level with current theme/music. Inspect its assets before playtest.";
		}
		break;
	}
	case Property::Bottom:
		GetDocument().SetDrawAtBottom(Selected, !GetDocument().DrawsAtBottom(Selected));
		break;
	case Property::NaturalDepth:
		GetDocument().ResetSceneryDepth(Selected);
		Notice = "Natural ground depth restored. Actors can pass in front of or behind this scenery.";
		break;
	case Property::Earlier:
	case Property::Later:
		GetDocument().Reorder(Selected, property == Property::Later);
		break;
	default:
	{
		using ObjectProperty = LevelDocument::ObjectProperty;
		const LevelObjectRecord& record = level.GetObjects().at(Selected);
		ObjectProperty target;
		f32 value = 0;
		switch (property)
		{
		case Property::X:
			target = ObjectProperty::X;
			value = ParseNumber(text);
			break;
		case Property::Y:
			target = ObjectProperty::Y;
			value = ParseNumber(text);
			break;
		case Property::Depth:
			target = ObjectProperty::Depth;
			value = ParseNumber(text);
			break;
		case Property::Frame:
			target = ObjectProperty::Frame;
			value = ParseNumber(text);
			break;
		case Property::Texture:
			target = ObjectProperty::Texture;
			value = ParseNumber(text);
			break;
		case Property::Impatience:
			target = ObjectProperty::Impatience;
			value = ParseNumber(text);
			break;
		case Property::Pass:
			target = ObjectProperty::Pass;
			value = record.RenderPass() ? 0.0f : 1.0f;
			break;
		case Property::Permanent:
			target = ObjectProperty::Permanent;
			value = record.TextureOrPermanent() ? 0.0f : 1.0f;
			break;
		case Property::Facing:
		{
			target = ObjectProperty::Facing;
			const CharacterDefinition* definition = FindCharacterDefinition(static_cast<ObjectKind>(record.Kind()));
			const i32 count = definition->Category == CharacterCategory::Animal ? 4 : 8;
			value = static_cast<f32>((record.Facing() + step + count) % count);
			break;
		}
		default:
			throw std::runtime_error("This property is read-only.");
		}
		GetDocument().SetObjectProperty(Selected, target, value);
		break;
	}
	}
}

void LevelEditor::ShowInspector(bool show)
{
	Palette.Show(show ? PlacementPalette::Group::Properties : PlacementPalette::Group::All);
	Inspector = show;
	PaletteVisible = Hud = true;
	ObjectBrowser = false;
	Dragging = false;
}

RECT LevelEditor::InspectorBounds() const
{
	const RECT bounds = PaletteBounds();
	return { bounds.left, bounds.top + PlacementPalette::HeaderHeight + PlacementPalette::TabHeight, bounds.right, bounds.bottom };
}

bool LevelEditor::HandleInspector(const WindowInput::Result& input)
{

	if (EditingField && input.Click)
	{
		ApplyField(CurrentField, FieldText);
		EditingField = false;
	}
	if (EditingField)
	{
		if (input.Back || input.Editor)
		{
			EditingField = false;
			Notice = "Property edit cancelled.";
			return true;
		}
		if (input.Enter || input.SaveEditor || input.Playtest)
		{
			ApplyField(CurrentField, FieldText);
			EditingField = false;
			DiscardArmed = false;
			Notice = "Property applied. Ctrl+Z undoes it.";
			return !input.SaveEditor && !input.Playtest;
		}
		if (input.Erase)
		{
			if (ReplaceText)
				FieldText.clear();
			else if (!FieldText.empty())
			{
				size_t end = FieldText.size() - 1;
				while (end > 0 && (static_cast<u8>(FieldText[end]) & 0xC0) == 0x80)
					--end;
				FieldText.resize(end);
			}
			ReplaceText = false;
		}
		if (input.Control && input.BindingKey == 'a')
			ReplaceText = true;
		if (!input.Text.empty() && !input.Control)
		{
			if (ReplaceText)
				FieldText.clear();
			if (FieldText.size() + input.Text.size() <= 1024)
				FieldText += input.Text;
			ReplaceText = false;
			DiscardArmed = false;
		}
		return true;
	}
	if (!PaletteVisible || !Inspector)
		return false;
	if (Picker != PickerKind::None && (input.Back || input.Editor))
	{
		Picker = PickerKind::None;
		PendingDelete.reset();
		return true;
	}
	if (input.BindingKey == 'i' && !input.Control && Picker == PickerKind::None)
	{
		ShowInspector(false);
		return true;
	}
	const RECT bounds = InspectorBounds();
	const bool inside = Mouse.x >= bounds.left && Mouse.x < bounds.right && Mouse.y >= bounds.top && Mouse.y < bounds.bottom;
	if (inside && input.Click)
		UiPointerHeld = input.MouseHeld;

	if (Picker != PickerKind::None)
		return HandlePicker(input);
	if (!inside)
		return false;
	if (input.Click || input.RightClick)
	{
		DiscardArmed = false;
		Dragging = false;
		const i32 content = bounds.top;
		if (Mouse.y < content + TabsHeight)
		{
			InspectorTab = static_cast<InspectorPage>((Mouse.x - bounds.left) / (PanelWidth / 4));
			if (InspectorTab == InspectorPage::Cell || InspectorTab == InspectorPage::Object)
			{
				CurrentTool = Tool::Select;
				SelectCells = InspectorTab == InspectorPage::Cell;
				if (SelectCells)
					Selected = -1;
			}
		}
		else
		{
			const std::vector<Field> fields = Fields();
			const size_t row = (Mouse.y - content - TabsHeight) / FieldHeight;
			if (row < fields.size())
			{
				const Field& field = fields[row];
				if (field.Mode == FieldMode::Text)
				{
					ResetArmed = false;
					CurrentField = field.Id;
					FieldText = field.Value;
					EditingField = ReplaceText = true;
				}
				else if (field.Mode != FieldMode::ReadOnly)
					ApplyField(field.Id, "", input.RightClick ? -1 : 1);
			}
		}
	}
	return input.Click || input.RightClick || input.Wheel != 0;
}

void LevelEditor::DrawInspector()
{
	const RECT bounds = InspectorBounds();
	const i32 height = bounds.bottom - bounds.top;
	if (Panel.FrameHeight != height)
		Panel = Rectangle(PanelWidth, height, 0xFFEDE4CE, false);
	Commands.push_back({ &Panel, bounds.left, bounds.top, 0 });
	if (Picker != PickerKind::None)
	{
		DrawPicker();
		return;
	}
	constexpr cstr tabs[] = { "Level", "Object", "Cell", "Project" };
	const i32 content = bounds.top;
	Commands.push_back({ &PanelTab, bounds.left + static_cast<i32>(InspectorTab) * (PanelWidth / 4), content, 0 });
	for (i32 tab = 0; tab < 4; ++tab)
		Font.Append(Commands, tabs[tab], bounds.left + 6 + tab * (PanelWidth / 4), content + 4);
	const std::vector<Field> fields = Fields();
	for (size_t row = 0; row < fields.size(); ++row)
	{
		const Field& field = fields[row];
		const i32 y = content + TabsHeight + static_cast<i32>(row) * FieldHeight;
		Commands.push_back({ &FieldBar, bounds.left + 4, y, 0 });
		Font.Append(Commands, FitText(Font, field.Label, 124), bounds.left + 8, y + 4);
		const bool editing = EditingField && field.Id == CurrentField;
		str value = editing ? FieldText : field.Value;
		if (editing)
		{
			while (!value.empty() && Font.Width(value + "_") > 174)
			{
				size_t end = 1;
				while (end < value.size() && (static_cast<u8>(value[end]) & 0xC0) == 0x80)
					++end;
				value.erase(0, end);
			}
			value += "_";
		}
		Font.Append(Commands, FitText(Font, value, 174), bounds.left + 138, y + 4);
	}
	if (fields.empty())
		Font.Append(Commands, "Click a cell, even underneath scenery.", bounds.left + 8, content + TabsHeight + 4);
	Font.Append(Commands, EditingField ? "Enter applies; Esc cancels; Ctrl+A selects." : "Click edits; I returns to placement.", bounds.left + 8, bounds.bottom - 40);
	Font.Append(Commands, SelectCells ? "Cell selection: V returns to objects." : "Shift+click: cell; Ctrl+click: overlap.", bounds.left + 8, bounds.bottom - 20);
}

void LevelEditor::ResetSelection()
{
	PendingDelete.reset();
	Selected = SelectedCell = -1;
	Palette.Reset();
	ListSearch.clear();
	ListFirst = 0;
	ListSearchFocused = false;
	Dragging = EditingField = false;
	Picker = PickerKind::None;
	BuildChoices();
}

std::vector<LevelEditor::PickerEntry> LevelEditor::PickerEntries() const
{
	return PickerEntries(Picker, Search);
}

std::vector<LevelEditor::PickerEntry> LevelEditor::PickerEntries(PickerKind pickerKind, const str& filter) const
{
	std::vector<PickerEntry> entries;
	if (pickerKind == PickerKind::Theme)
	{
		const std::vector<TileSetInfo>& themes = GetDocument().GetThemes();
		for (size_t index = 0; index < themes.size(); ++index)
			entries.push_back({ index, themes[index].Name });
	}
	else if (pickerKind == PickerKind::Pack)
	{
		const std::vector<LevelPackInfo>& packs = Engine.GetData().GetPacks();
		for (size_t index = 0; index < packs.size(); ++index)
			entries.push_back({ index, packs[index].AssetPrefix + packs[index].Name });
	}
	else if (pickerKind == PickerKind::Level)
	{
		for (size_t index = 0; index < Project.GetEntries().size(); ++index)
		{
			const LevelProject::Entry& entry = Project.GetEntries()[index];
			entries.push_back({ index, entry.Episode + " / " + entry.Document->GetInfo().Name });
		}
	}
	else
	{
		const std::array<LevelObjectRecord, LevelFile::ObjectCount>& objects = GetDocument().GetLevel().GetObjects();
		for (size_t index = 0; index < objects.size(); ++index)
		{
			const LevelObjectRecord& record = objects[index];
			const ObjectKind kind = static_cast<ObjectKind>(record.Kind());
			if (kind == ObjectKind::Inactive)
				continue;
			const CharacterDefinition* character = FindCharacterDefinition(kind);
			const ItemDefinition* item = FindItemDefinition(kind);
			str name = character ? character->Name : (item ? item->Name : "Scenery");
			if (kind == ObjectKind::Scenery)
				name = std::filesystem::u8path(Engine.GetData().TileSet(GetDocument().GetPack(), GetDocument().GetInfo()).Tiles.at(record.TextureOrPermanent()).File).stem().u8string();
			entries.push_back({ index, std::to_string(index) + " " + name });
		}
	}
	auto lowercase = [](str value) { std::transform(value.begin(), value.end(), value.begin(), [](u8 c) { return static_cast<char>(std::tolower(c)); }); return value; };
	const str search = lowercase(filter);
	entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const PickerEntry& entry) { return lowercase(entry.Label).find(search) == str::npos; }), entries.end());
	return entries;
}

void LevelEditor::ChoosePicker(size_t index)
{
	PendingDelete.reset();
	const PickerKind kind = Picker;
	if (kind == PickerKind::Theme)
	{
		GetDocument().SetTileSet(index);
		BuildChoices();
		Notice = "Theme changed. Existing scenery art retained; palette and background updated.";
	}
	else if (kind == PickerKind::Object)
	{
		SelectObject(index);
	}
	else if (kind == PickerKind::Pack)
	{
		if (Project.IsDirty() && PendingPack != static_cast<i32>(index))
		{
			PendingPack = static_cast<i32>(index);
			Notice = "Unsaved project: save first, or choose this pack again to discard.";
			return;
		}
		Project.Open(Engine.GetData().GetPacks().at(index));
		ResetSelection();
		InspectorTab = InspectorPage::Project;
		Notice = "Opened a full working copy. Save writes a separate named pack.";
	}
	else if (kind == PickerKind::Level)
	{
		Project.Select(index);
		ResetSelection();
		InspectorTab = InspectorPage::Level;
	}
	Picker = PickerKind::None;
}

void LevelEditor::FocusProjectLevel()
{
	const std::vector<PickerEntry> entries = PickerEntries();
	const auto current = std::find_if(entries.begin(), entries.end(), [&](const PickerEntry& entry) { return entry.Index == Project.GetActive(); });
	PickerSelected = current == entries.end() ? 0 : static_cast<size_t>(std::distance(entries.begin(), current));
	PickerFirst = PickerSelected >= PickerRows ? PickerSelected - PickerRows + 1 : 0;
}

void LevelEditor::ManageLevels(LevelAction action, size_t index)
{
	FinishPaintStroke();
	if (action == LevelAction::Delete)
	{
		if (Project.GetEntries().size() == 1)
		{
			PendingDelete.reset();
			Notice = "Cannot delete the last project level.";
			return;
		}
		const LevelProject::Entry& entry = Project.GetEntries().at(index);
		if (PendingDelete != entry.Id)
		{
			PendingDelete = entry.Id;
			Notice = "Delete " + entry.Document->GetInfo().Name + "? Click Delete again; Esc cancels.";
			return;
		}
	}
	PendingDelete.reset();
	if (action == LevelAction::Undo || action == LevelAction::Redo)
	{
		const bool changed = action == LevelAction::Undo ? Project.Undo() : Project.Redo();
		Notice = changed ? "Project action restored. Level edits and their separate history are retained." : "No project action to restore.";
	}
	else
	{
		Project.Select(index);
		if (action == LevelAction::Delete)
		{
			Project.DeleteLevel();
			Notice = "Level removed from project. Undo here restores it; Ctrl+S saves the pack.";
		}
		else
			Notice = Project.MoveLevel(action == LevelAction::Later) ? "Level order changed within its levelset. Ctrl+S saves the pack." : "Already at this end of the levelset.";
	}
	const str search = Search;
	ResetSelection();
	Picker = PickerKind::Level;
	Search = search;
	FocusProjectLevel();
}

bool LevelEditor::HandlePicker(const WindowInput::Result& input)
{
	if (Picker == PickerKind::Level)
	{
		if (input.SaveEditor || input.Playtest)
		{
			PendingDelete.reset();
			return false;
		}
		const bool toolbarUndo = input.Click && ToolbarVisible() && Mouse.y >= 0 && Mouse.y < 26 && Mouse.x >= 300 && Mouse.x < 500;
		if (input.Undo || input.Redo || toolbarUndo)
		{
			ManageLevels(input.Redo || (toolbarUndo && Mouse.x >= 400) ? LevelAction::Redo : LevelAction::Undo);
			return true;
		}
		const RECT bounds = InspectorBounds();
		if (input.Click && Mouse.x >= bounds.left && Mouse.x < bounds.right && Mouse.y >= bounds.bottom - 108 && Mouse.y < bounds.bottom - 52)
		{
			if (Mouse.y >= bounds.bottom - 80)
				ManageLevels(Mouse.x < bounds.left + PanelWidth / 2 ? LevelAction::Undo : LevelAction::Redo);
			else
			{
				const std::vector<PickerEntry> entries = PickerEntries();
				if (!entries.empty())
					ManageLevels(static_cast<LevelAction>((Mouse.x - bounds.left) * 3 / PanelWidth), entries[std::min(PickerSelected, entries.size() - 1)].Index);
			}
			return true;
		}
		if (input.Click || input.RightClick || input.Enter || input.Up || input.Down || input.Wheel != 0 || input.Erase || !input.Text.empty() || (input.Control && input.BindingKey == 'a'))
			PendingDelete.reset();
	}
	const str previous = Search;
	if (input.Control && input.BindingKey == 'a')
		Search.clear();
	if (input.Erase && !Search.empty())
	{
		size_t end = Search.size() - 1;
		while (end > 0 && (static_cast<u8>(Search[end]) & 0xC0) == 0x80)
			--end;
		Search.resize(end);
	}
	if (!input.Text.empty() && !input.Control && Search.size() + input.Text.size() <= 256)
		Search += input.Text;
	if (Search != previous)
		PickerFirst = PickerSelected = 0;
	const std::vector<PickerEntry> entries = PickerEntries();
	if (entries.empty())
		return true;
	PickerSelected = std::min(PickerSelected, entries.size() - 1);
	if (input.Down)
		PickerSelected = std::min(PickerSelected + 1, entries.size() - 1);
	if (input.Up && PickerSelected > 0)
		--PickerSelected;
	if (input.Wheel != 0)
		PickerSelected = static_cast<size_t>(std::clamp(static_cast<i32>(PickerSelected) - input.Wheel, 0, static_cast<i32>(entries.size() - 1)));
	if (PickerSelected < PickerFirst)
		PickerFirst = PickerSelected;
	if (PickerSelected >= PickerFirst + PickerRows)
		PickerFirst = PickerSelected - PickerRows + 1;
	if (input.Enter)
	{
		ChoosePicker(entries[PickerSelected].Index);
		return true;
	}
	const RECT bounds = InspectorBounds();
	const i32 top = bounds.top + FieldHeight;
	if (input.Click && Mouse.x >= bounds.left && Mouse.x < bounds.right && Mouse.y >= top && Mouse.y < top + PickerRows * FieldHeight)
	{
		const size_t row = PickerFirst + (Mouse.y - top) / FieldHeight;
		if (row < entries.size())
			ChoosePicker(entries[row].Index);
	}
	return true;
}

void LevelEditor::DrawPicker()
{
	const RECT bounds = InspectorBounds();
	const str title = Picker == PickerKind::Theme ? "Themes" : (Picker == PickerKind::Object ? "Objects" : (Picker == PickerKind::Pack ? "Packs" : "Levels"));
	Font.Append(Commands, FitText(Font, title + ": " + Search + "_", PanelWidth - 16), bounds.left + 8, bounds.top + 4);
	const std::vector<PickerEntry> entries = PickerEntries();
	for (size_t row = 0; row < PickerRows && PickerFirst + row < entries.size(); ++row)
	{
		const i32 y = bounds.top + FieldHeight + static_cast<i32>(row) * FieldHeight;
		if (PickerFirst + row == PickerSelected)
			Commands.push_back({ &FieldBar, bounds.left + 4, y, 0 });
		const PickerEntry& entry = entries[PickerFirst + row];
		const str label = Picker == PickerKind::Level ? std::to_string(entry.Index + 1) + ". " + entry.Label : entry.Label;
		Font.Append(Commands, FitText(Font, label, PanelWidth - 16), bounds.left + 8, y + 4);
	}
	if (Picker == PickerKind::Level)
	{
		Commands.push_back({ &FieldBar, bounds.left + 4, bounds.bottom - 108, 0 });
		Commands.push_back({ &FieldBar, bounds.left + 4, bounds.bottom - 80, 0 });
		Font.Append(Commands, "Earlier", bounds.left + 8, bounds.bottom - 104);
		Font.Append(Commands, "Later", bounds.left + PanelWidth / 3 + 8, bounds.bottom - 104);
		Font.Append(Commands, PendingDelete ? "Delete again?" : "Delete", bounds.left + PanelWidth * 2 / 3 + 4, bounds.bottom - 104);
		Font.Append(Commands, Project.CanUndo() ? "Undo project" : "Undo (-)", bounds.left + 8, bounds.bottom - 76);
		Font.Append(Commands, Project.CanRedo() ? "Redo project" : "Redo (-)", bounds.left + PanelWidth / 2 + 8, bounds.bottom - 76);
	}
	Font.Append(Commands, "Type to search; arrows/wheel; Enter picks.", bounds.left + 8, bounds.bottom - 36);
	Font.Append(Commands, "Esc cancels; Ctrl+A clears search.", bounds.left + 8, bounds.bottom - 18);
}

POINT LevelEditor::SceneryAnchor(const SpriteDraw& draw)
{
	if (!CenterScenery)
		return { 0, 0 };
	if (AnchorSprite != draw.Sprite || AnchorFrame != draw.Frame)
	{
		const Sprite& image = *draw.Sprite;
		i32 left = image.FrameWidth, right = -1, top = image.FrameHeight, bottom = -1;
		const i32 sourceX = draw.Frame % image.Columns * image.FrameWidth, sourceY = draw.Frame / image.Columns * image.FrameHeight;
		for (i32 y = 0; y < image.FrameHeight; ++y)
		{
			for (i32 x = 0; x < image.FrameWidth; ++x)
			{
				if (image.Keyed && (image.Pixels[(sourceY + y) * image.Width + sourceX + x] & 0xFFFFFF) == 0xFF00FF)
					continue;
				left = std::min(left, x);
				right = std::max(right, x);
				top = std::min(top, y);
				bottom = std::max(bottom, y);
			}
		}
		AnchorOffset = right < left ? POINT{ image.FrameWidth / 2, image.FrameHeight / 2 } : POINT{ (left + right) / 2, (top + bottom) / 2 };
		AnchorSprite = draw.Sprite;
		AnchorFrame = draw.Frame;
	}
	return AnchorOffset;
}

LevelEditor::Placement LevelEditor::PlacementAt(POINT point)
{
	const i32 tile = LevelDocument::TileAt(point.x, point.y);
	const Choice& choice = Choices.at(ChoiceIndex);
	SpriteDraw draw = GetDocument().PlacementPreview(choice.Kind, tile, choice.Texture);
	const CharacterDefinition* character = FindCharacterDefinition(choice.Kind);
	if (character && character->Category == CharacterCategory::Animal)
		draw.Frame = Palette.GetAnimalPlacement().Facing;
	POINT anchor{ (tile % Stride - 1) * TileSize + TileSize / 2, (tile / Stride - 1) * TileSize + TileSize / 2 };
	if (choice.Kind == ObjectKind::Scenery)
	{
		if (!SnapScenery)
			anchor = point;
		else if (!CenterScenery)
			anchor = { (tile % Stride - 1) * TileSize, (tile / Stride - 1) * TileSize };
		const POINT offset = SceneryAnchor(draw);
		draw.X = anchor.x - offset.x;
		draw.Y = anchor.y - offset.y;
	}
	return { tile, draw, anchor };
}

RECT LevelEditor::ObjectListBounds() const
{
	const i32 height = ListCollapsed ? HeaderHeight : HeaderHeight + (ListRows + 2) * ListRowHeight;
	const i32 x = std::clamp(static_cast<i32>(ListPosition.x), 0, 800 - ListWidth);
	const i32 y = std::clamp(static_cast<i32>(ListPosition.y), ToolbarVisible() ? 26 : 0, (ToolbarVisible() ? 574 : 600) - height);
	return { x, y, x + ListWidth, y + height };
}

void LevelEditor::SelectObject(size_t index)
{
	Selected = static_cast<i32>(index);
	InspectorTab = InspectorPage::Object;
	CurrentTool = Tool::Select;
	SelectCells = Dragging = false;
	const std::vector<PickerEntry> entries = PickerEntries(PickerKind::Object, ListSearch);
	const auto found = std::find_if(entries.begin(), entries.end(), [&](const PickerEntry& entry) { return entry.Index == index; });
	if (found != entries.end())
	{
		const size_t row = static_cast<size_t>(std::distance(entries.begin(), found));
		if (row < ListFirst || row >= ListFirst + ListRows)
			ListFirst = row / ListRows * ListRows;
	}
}

bool LevelEditor::HandleObjectList(const WindowInput::Result& input)
{
	if (!ObjectBrowser)
		return false;
	if (MovingList)
	{
		ListPosition = { Mouse.x - ListDrag.x, Mouse.y - ListDrag.y };
		const RECT clamped = ObjectListBounds();
		ListPosition = { clamped.left, clamped.top };
		MovingList = input.MouseHeld;
		UiPointerHeld = input.MouseHeld;
		return true;
	}
	const RECT bounds = ObjectListBounds();
	const bool inside = Mouse.x >= bounds.left && Mouse.x < bounds.right && Mouse.y >= bounds.top && Mouse.y < bounds.bottom;
	if (inside && input.Click && EditingField)
	{
		ApplyField(CurrentField, FieldText);
		EditingField = false;
	}
	if (input.Click && !inside)
		ListSearchFocused = false;
	if (ListSearchFocused)
	{
		const str previous = ListSearch;
		if (input.Back || input.Editor)
		{
			ListSearchFocused = false;
			return true;
		}
		if (input.Control && input.BindingKey == 'a')
			ListSearch.clear();
		if (input.Erase && !ListSearch.empty())
		{
			size_t end = ListSearch.size() - 1;
			while (end > 0 && (static_cast<u8>(ListSearch[end]) & 0xC0) == 0x80)
				--end;
			ListSearch.resize(end);
		}
		if (!input.Text.empty() && !input.Control && ListSearch.size() + input.Text.size() <= 256)
			ListSearch += input.Text;
		if (input.Enter)
		{
			ListSearchFocused = false;
			const std::vector<PickerEntry> entries = PickerEntries(PickerKind::Object, ListSearch);
			if (!entries.empty())
				SelectObject(entries.front().Index);
		}
		if (previous != ListSearch)
			ListFirst = 0;
		if (!input.Click && !input.RightClick && input.Wheel == 0)
			return !input.SaveEditor && !input.Playtest;
	}
	if (!inside)
		return false;
	const std::vector<PickerEntry> entries = PickerEntries(PickerKind::Object, ListSearch);
	const size_t maxFirst = entries.size() > ListRows ? entries.size() - ListRows : 0;
	if (input.Wheel != 0)
		ListFirst = static_cast<size_t>(std::clamp(static_cast<i32>(ListFirst) - input.Wheel * 3, 0, static_cast<i32>(maxFirst)));
	if (input.Click)
	{
		UiPointerHeld = input.MouseHeld;
		Dragging = false;
		DiscardArmed = false;
		if (Mouse.y < bounds.top + HeaderHeight)
		{
			ListSearchFocused = false;
			if (Mouse.x >= bounds.right - HeaderHeight)
				ListCollapsed = !ListCollapsed;
			else
			{
				ListDrag = { Mouse.x - bounds.left, Mouse.y - bounds.top };
				MovingList = input.MouseHeld;
			}
		}
		else if (!ListCollapsed && Mouse.y < bounds.top + HeaderHeight + ListRowHeight)
			ListSearchFocused = true;
		else if (!ListCollapsed && Mouse.y < bounds.top + HeaderHeight + (ListRows + 1) * ListRowHeight)
		{
			ListSearchFocused = false;
			const size_t row = ListFirst + (Mouse.y - bounds.top - HeaderHeight - ListRowHeight) / ListRowHeight;
			if (row < entries.size())
				SelectObject(entries[row].Index);
		}
	}
	return input.Click || input.RightClick || input.Wheel != 0;
}

void LevelEditor::DrawObjectList()
{
	const RECT bounds = ObjectListBounds();
	if (ListPanel.FrameHeight != bounds.bottom - bounds.top)
		ListPanel = Rectangle(ListWidth, bounds.bottom - bounds.top, 0xFFEDE4CE, false);
	Commands.push_back({ &ListPanel, bounds.left, bounds.top, 0 });
	Commands.push_back({ &ListHeader, bounds.left, bounds.top, 0 });
	Font.Append(Commands, "Objects - drag to move", bounds.left + 6, bounds.top + 4);
	Font.Append(Commands, ListCollapsed ? "+" : "-", bounds.right - 18, bounds.top + 4);
	if (ListCollapsed)
		return;
	Font.Append(Commands, FitText(Font, "Find: " + ListSearch + (ListSearchFocused ? "_" : ""), ListWidth - 12), bounds.left + 6, bounds.top + HeaderHeight + 4);
	const std::vector<PickerEntry> entries = PickerEntries(PickerKind::Object, ListSearch);
	ListFirst = std::min(ListFirst, entries.size() > ListRows ? entries.size() - ListRows : 0);
	for (size_t row = 0; row < ListRows && ListFirst + row < entries.size(); ++row)
	{
		const PickerEntry& entry = entries[ListFirst + row];
		const i32 y = bounds.top + HeaderHeight + ListRowHeight + static_cast<i32>(row) * ListRowHeight;
		if (Selected == static_cast<i32>(entry.Index))
			Commands.push_back({ &ListRow, bounds.left + 4, y, 0 });
		Font.Append(Commands, FitText(Font, entry.Label, ListWidth - 12), bounds.left + 6, y + 4);
	}
	Font.Append(Commands, std::to_string(entries.size()) + " objects | O closes | wheel scrolls", bounds.left + 6, bounds.bottom - 20);
}

RECT LevelEditor::PaletteBounds() const
{
	const i32 top = ToolbarVisible() ? 26 : 0, bottom = ToolbarVisible() ? 574 : 600;
	const i32 height = std::min(PaletteHeight, bottom - top);
	const i32 x = std::clamp(static_cast<i32>(PalettePosition.x), 0, 800 - PlacementPalette::Width);
	const i32 y = std::clamp(static_cast<i32>(PalettePosition.y), top, bottom - height);
	return { x, y, x + PlacementPalette::Width, y + height };
}

bool LevelEditor::HandlePalette(const WindowInput::Result& input)
{
	if (!PaletteVisible)
		return false;
	if (MovingPalette)
	{
		PalettePosition = { Mouse.x - PaletteDrag.x, Mouse.y - PaletteDrag.y };
		const RECT clamped = PaletteBounds();
		PalettePosition = { clamped.left, clamped.top };
		MovingPalette = UiPointerHeld = input.MouseHeld;
		return true;
	}
	const RECT bounds = PaletteBounds();
	const bool inside = Mouse.x >= bounds.left && Mouse.x < bounds.right && Mouse.y >= bounds.top && Mouse.y < bounds.bottom;
	if (inside && input.Click)
	{
		if (EditingField)
		{
			ApplyField(CurrentField, FieldText);
			EditingField = false;
		}
		ListSearchFocused = false;
	}
	if (inside && input.Click && Mouse.y < bounds.top + PlacementPalette::HeaderHeight)
	{
		UiPointerHeld = input.MouseHeld;
		Dragging = false;
		if (Mouse.x >= bounds.left + PlacementPalette::GridX)
			Layers[static_cast<size_t>(Layer::Grid)] = !IsLayerVisible(Layer::Grid);
		else
		{
			PaletteDrag = { Mouse.x - bounds.left, Mouse.y - bounds.top };
			MovingPalette = input.MouseHeld;
		}
		return true;
	}

	const PlacementPalette::Result result = Palette.Handle(input, GetDocument(), bounds);
	Inspector = Palette.IsProperties();
	if (Palette.IsRaw())
	{
		CurrentTool = Tool::Select;
		SelectCells = Raw.IsCell();
		if (SelectCells)
			Selected = -1;
	}
	if (Palette.IsView() && HandleView(input))
		return true;
	if (inside && input.Click)
	{
		UiPointerHeld = input.MouseHeld;
		Dragging = false;
		DiscardArmed = false;
	}
	if (result.Selected)
	{
		const PlacementPalette::Entry& entry = *result.Selected;
		if (entry.Brush >= 0)
		{
			Layers[static_cast<size_t>(Layer::Tiles)] = true;
			TileChoice = entry.Brush;
			CurrentTool = Tool::Collision;
			Selected = -1;
			SelectCells = Dragging = false;
			Notice = entry.Name + ". Click paints; right-click erases this layer.";
			return result.Consumed;
		}
		if (entry.Kind == ObjectKind::Scenery)
			CurrentCategory = Category::Scenery;
		else if (IsItem(entry.Kind))
			CurrentCategory = Category::Items;
		else
		{
			const CharacterDefinition* character = FindCharacterDefinition(entry.Kind);
			CurrentCategory = character->Category == CharacterCategory::Animal ? Category::Animals : (character->Category == CharacterCategory::Enemy ? Category::Enemies : (character->Category == CharacterCategory::Player ? Category::Players : Category::Special));
		}
		Layers[static_cast<size_t>(ObjectLayer(entry.Kind))] = true;
		BuildChoices();
		const auto found = std::find_if(Choices.begin(), Choices.end(), [&](const Choice& choice) { return choice.Kind == entry.Kind && choice.Texture == entry.Texture; });
		if (found == Choices.end())
			throw std::runtime_error("Palette entry is unavailable in this runtime.");
		ChoiceIndex = static_cast<i32>(std::distance(Choices.begin(), found));
		Palette.Select(entry.Kind, entry.Texture);
		Selected = -1;
		CurrentTool = Tool::Place;
		SelectCells = Dragging = false;
		Notice = "Place " + entry.Name + ". B hides the palette; H hides all UI.";
	}
	return result.Consumed;
}

void LevelEditor::DrawView()
{
	const RECT bounds = InspectorBounds();
	Font.Append(Commands, "Zoom: " + std::to_string(static_cast<i32>(Viewport.GetZoom() * 100)) + "%", bounds.left + 8, bounds.top + 4);
	Font.Append(Commands, "-", bounds.left + 174, bounds.top + 4);
	Font.Append(Commands, "+", bounds.left + 219, bounds.top + 4);
	Font.Append(Commands, "Reset", bounds.left + 254, bounds.top + 4);
	for (size_t row = 0; row < Layers.size(); ++row)
	{
		const i32 y = bounds.top + 28 + static_cast<i32>(row) * 28;
		Commands.push_back({ &FieldBar, bounds.left + 4, y, 0 });
		Font.Append(Commands, str(Layers[row] ? "[x] " : "[ ] ") + LayerNames[row], bounds.left + 8, y + 4);
	}
	Font.Append(Commands, "Ctrl+wheel: zoom at cursor", bounds.left + 8, bounds.bottom - 60);
	Font.Append(Commands, "Middle drag: pan | 0: reset", bounds.left + 8, bounds.bottom - 40);
	Font.Append(Commands, "Hidden objects cannot be picked.", bounds.left + 8, bounds.bottom - 20);
}

bool LevelEditor::HandleView(const WindowInput::Result& input)
{
	const RECT bounds = InspectorBounds();
	if (!input.Click || Mouse.x < bounds.left || Mouse.x >= bounds.right || Mouse.y < bounds.top || Mouse.y >= bounds.bottom)
		return false;
	UiPointerHeld = input.MouseHeld;
	Dragging = false;
	const i32 row = (Mouse.y - bounds.top) / 28 - 1;
	if (Mouse.y < bounds.top + 28)
	{
		const i32 x = Mouse.x - bounds.left;
		if (x >= 250)
			Viewport.Reset();
		else if (x >= 170)
			Viewport.ZoomAt({ 400, 300 }, x < 215 ? -1 : 1);
		WorldMouse = Viewport.WorldAt(Mouse);
		Notice = "Zoom " + std::to_string(static_cast<i32>(Viewport.GetZoom() * 100)) + "% | Ctrl+wheel zoom | middle drag pan | 0 resets";
	}
	else if (row >= 0 && row < static_cast<i32>(Layers.size()))
	{
		Layers[row] = !Layers[row];
		if (Selected >= 0 && !ObjectVisible(static_cast<ObjectKind>(GetDocument().GetLevel().GetObjects()[Selected].Kind())))
			Selected = -1;
	}
	return true;
}

void LevelEditor::FinishPaintStroke()
{
	if (Painting)
	{
		Painting->Document->EndTileStroke();
		Painting.reset();
		if (Notice.rfind("Skipped cell:", 0) != 0)
			Notice = "Stroke finished. Ctrl+Z undoes the last edit.";
	}
}

void LevelEditor::ContinuePaintStroke()
{
	PaintGesture& gesture = *Painting;
	if (!CanvasAt(Mouse))
	{
		gesture.LastTile = -1;
		return;
	}
	const i32 target = LevelDocument::TileAt(WorldMouse.x, WorldMouse.y);
	const i32 start = gesture.LastTile >= 0 ? gesture.LastTile : target;
	i32 x = start % Stride, y = start / Stride;
	const i32 endX = target % Stride, endY = target / Stride;
	const i32 dx = std::abs(endX - x), dy = -std::abs(endY - y);
	const i32 stepX = x < endX ? 1 : -1, stepY = y < endY ? 1 : -1;
	i32 error = dx + dy;
	const TileBrush& brush = TileBrushes[gesture.Brush];
	while (true)
	{
		const i32 tile = y * Stride + x;
		const POINT center{ (x - 1) * TileSize + TileSize / 2, (y - 1) * TileSize + TileSize / 2 };
		if (!gesture.Visited[tile] && (tile == target || CanvasAt(Viewport.ScreenAt(center))))
		{
			gesture.Visited[tile] = true;
			try
			{
				if (brush.SpawnerOnly)
					gesture.Document->SetSpawner(tile, gesture.Erase ? SpawnerCode::None : brush.Spawner);
				else
					gesture.Document->PaintTerrain(tile, gesture.Erase ? OccupancyCode::Empty : brush.Terrain);
			}
			catch (const std::exception& rejected)
			{
				Notice = str("Skipped cell: ") + rejected.what();
			}
		}
		if (x == endX && y == endY)
			break;
		const i32 twice = error * 2;
		if (twice >= dy)
		{
			error += dy;
			x += stepX;
		}
		if (twice <= dx)
		{
			error += dx;
			y += stepY;
		}
	}
	SelectedCell = target;
	gesture.LastTile = target;
}
