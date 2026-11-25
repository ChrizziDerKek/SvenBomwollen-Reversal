#include "LevelEditor.h"
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
Sprite Rectangle(i32 width, i32 height, u32 color, bool outline)
{
	Sprite sprite{ std::vector<u32>(width * height, 0xFFFF00FF), width, width, height, 1, 1, true };
	for (i32 y = 0; y < height; ++y)
	{
		for (i32 x = 0; x < width; ++x)
		{
			if (!outline || x == 0 || y == 0 || x == width - 1 || y == height - 1)
				sprite.Pixels[y * width + x] = color;
		}
	}
	return sprite;
}
str FitText(const BitmapFont& font, str text, i32 width)
{
	if (font.Width(text) <= width)
		return text;
	while (!text.empty() && font.Width(text + "...") > width)
	{
		size_t end = text.size() - 1;
		while (end > 0 && (static_cast<u8>(text[end]) & 0xC0) == 0x80)
			--end;
		text.resize(end);
	}
	return text + "...";
}
i32 Snap(i32 value)
{
	return static_cast<i32>(std::round(static_cast<f32>(value) / TileSize)) * TileSize;
}
struct TileBrush
{
	cstr Name;
	OccupancyCode Terrain;
	SpawnerCode Spawner;
	bool SpawnerOnly;
};
const std::vector<TileBrush> TileBrushes = {
	{ "Clear terrain", OccupancyCode::Empty, SpawnerCode::None, false },
	{ "Collision", OccupancyCode::Wall, SpawnerCode::None, false },
	{ "Puddle destination", OccupancyCode::Puddle, SpawnerCode::None, false },
	{ "Pond / water entry", OccupancyCode::Water, SpawnerCode::None, false },
	{ "Random distraction spawner", OccupancyCode::Empty, SpawnerCode::Distraction, true },
	{ "Random powerup spawner", OccupancyCode::Empty, SpawnerCode::Powerup, true },
	{ "Random mixed spawner", OccupancyCode::Empty, SpawnerCode::Mixed, true },
	{ "Remove spawner", OccupancyCode::Empty, SpawnerCode::None, true }
};
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
    : Engine(engine), Project(engine, pack, level), Font(engine, true, false, engine.GetData().GetThemeEdition()), Bar(Rectangle(800, 26, 0xFFEDE4CE, false)), Selection(Rectangle(40, 40, 0xFF00DD00, true)), Wall(Rectangle(40, 40, 0xFFFF3030, true)), GridCell(Rectangle(40, 40, 0xFF809070, true)), ActiveTool(Rectangle(100, 26, 0xFFB8D498, false)), WaterCell(Rectangle(40, 40, 0xFF2266FF, true)), PuddleCell(Rectangle(40, 40, 0xFF00DDDD, true)), SpawnerCell(Rectangle(40, 40, 0xFFFFB000, true)), Panel(Rectangle(PanelWidth, 1, 0xFFEDE4CE, false)), FieldBar(Rectangle(PanelWidth - 8, FieldHeight - 2, 0xFFD8CCA8, false)), PanelHeader(Rectangle(PanelWidth, HeaderHeight, 0xFFB8D498, false)), PanelTab(Rectangle(PanelWidth / 4, TabsHeight, 0xFFB8D498, false)), Cursor(engine.LoadSprite(engine.GetData().ThemeAsset("Sven2/gfx/mousecursor.bmp"), 5, 1)), Players(players)
{
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

i32 LevelEditor::Pick(POINT point, bool cycle)
{
	BuildObjects();
	std::vector<i32> hits;
	for (auto object = Objects.rbegin(); object != Objects.rend(); ++object)
	{
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
	const str path = Project.Save();
	Saved = true;
	Notice = "Saved " + path;
}

void LevelEditor::Playtest()
{
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
	try
	{
		if (!Preview && HandleInspector(input))
			return false;
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
				Toolbar = true;
				Notice = "Unsaved edits: Escape again discards; Ctrl+S saves a separate pack.";
				return false;
			}
			return true;
		}
		if ((input.Click && LevelDocument::TileAt(Mouse.x, Mouse.y) >= 0 && !(Toolbar && Mouse.y < 26 && Mouse.x >= 700)) || input.SaveEditor || input.Undo || input.Redo || input.Playtest || (input.BindingKey >= 32 && input.BindingKey < 256))
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
		if (input.Tab)
			Toolbar = !Toolbar;
		if (input.BindingKey == 'i' && !input.Control)
		{
			Inspector = !Inspector;
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
		}
		if (CurrentTool == Tool::Select && Selected >= 0 && (input.Left || input.Right || input.Up || input.Down))
		{
			const i32 step = !SnapScenery && GetDocument().GetLevel().GetObjects()[Selected].Kind() == static_cast<i32>(ObjectKind::Scenery) ? (input.Shift ? 10 : 1) : TileSize;
			GetDocument().Move(Selected, (static_cast<i32>(input.Right) - input.Left) * step, (static_cast<i32>(input.Down) - input.Up) * step, SnapScenery);
		}
		else if (input.Left || input.Right || input.Wheel != 0)
			ChangeChoice(input.Right ? 1 : (input.Left ? -1 : -input.Wheel));
		if (Toolbar && input.Click && Mouse.y >= 0 && Mouse.y < 52 && Mouse.x >= 0 && Mouse.x < 800)
		{
			if (Mouse.y < 26)
			{
				switch (Mouse.x / 100)
				{
				case 0:
					CurrentTool = Tool::Select;
					SelectCells = false;
					break;
				case 1:
					CurrentTool = Tool::Place;
					break;
				case 2:
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
					close.Back = true;
					return Handle(close, 0);
				}
				}
			}
			else if (CurrentTool == Tool::Collision && Mouse.x < 670)
				ChangeChoice(Mouse.x < 310 ? -1 : 1);
			else if (Mouse.x < 260)
			{
				const i32 count = static_cast<i32>(Category::Count);
				CurrentCategory = static_cast<Category>((static_cast<i32>(CurrentCategory) + (Mouse.x < 80 ? count - 1 : 1)) % count);
				BuildChoices();
			}
			else if (Mouse.x < 670)
			{
				ChangeChoice(Mouse.x < 310 ? -1 : 1);
			}
			else if (Mouse.x < 740)
			{
				Inspector = !Inspector;
				Dragging = false;
			}
			else
				ShowGrid = !ShowGrid;
			return false;
		}
		if ((input.Click || (input.RightClick && CurrentTool == Tool::Collision)) && CanvasAt(Mouse))
		{
			ResetArmed = false;
			SelectedCell = LevelDocument::TileAt(Mouse.x, Mouse.y);
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
				const TileBrush& brush = TileBrushes[TileChoice];
				if (brush.SpawnerOnly)
					GetDocument().SetSpawner(SelectedCell, input.RightClick ? SpawnerCode::None : brush.Spawner);
				else
					GetDocument().PaintTerrain(SelectedCell, input.RightClick ? OccupancyCode::Empty : brush.Terrain);
			}
			else if (CurrentTool == Tool::Place && !Choices.empty())
				Selected = GetDocument().Place(Choices[ChoiceIndex].Kind, SelectedCell, Choices[ChoiceIndex].Texture, !SnapScenery && Choices[ChoiceIndex].Kind == ObjectKind::Scenery ? &Mouse : nullptr);
			else
			{
				Selected = Pick(Mouse, input.Control);
				DragOrigin = Mouse;
				Dragging = Selected >= 0;
			}
		}
		if (Dragging && !input.MouseHeld)
		{
			Dragging = false;
			if (CanvasAt(Mouse))
				GetDocument().Move(Selected, Mouse.x - DragOrigin.x, Mouse.y - DragOrigin.y, SnapScenery);
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
		Commands = Preview->Draws();
		Commands.push_back({ &Bar, 0, 74, 0 });
		Font.Append(Commands, Preview->GetResults() ? "Playtest finished. F5 returns to editing." : "PLAYTEST - F5 returns to editing; progress is temporary.", 8, 78);
	}
	else
	{
		Commands.clear();
		const TileSetInfo& tiles = Engine.GetData().TileSet(GetDocument().GetPack(), GetDocument().GetInfo());
		Commands.push_back({ Engine.LoadSprite(tiles.Background, 1, 1, true), 0, 0, 0 });
		BuildObjects();
		for (const VisibleObject& object : Objects)
		{
			SpriteDraw draw = object.Draw;
			if (Dragging && static_cast<i32>(object.Slot) == Selected)
			{
				const bool snap = SnapScenery || GetDocument().GetLevel().GetObjects()[object.Slot].Kind() != static_cast<i32>(ObjectKind::Scenery);
				draw.X += snap ? Snap(Mouse.x - DragOrigin.x) : Mouse.x - DragOrigin.x;
				draw.Y += snap ? Snap(Mouse.y - DragOrigin.y) : Mouse.y - DragOrigin.y;
			}
			Commands.push_back(draw);
		}
		if (CurrentTool == Tool::Place && !Choices.empty() && CanvasAt(Mouse))
		{
			const i32 tile = LevelDocument::TileAt(Mouse.x, Mouse.y);
			if (tile >= 0)
			{
				try
				{
					SpriteDraw ghost = GetDocument().PlacementPreview(Choices[ChoiceIndex].Kind, tile, Choices[ChoiceIndex].Texture);
					if (!SnapScenery && Choices[ChoiceIndex].Kind == ObjectKind::Scenery)
					{
						ghost.X = Mouse.x;
						ghost.Y = Mouse.y;
					}
					Commands.push_back(ghost);
				}
				catch (const std::exception& error)
				{
					Notice = error.what();
				}
			}
		}
		if (ShowGrid)
		{
			for (i32 y = 0; y < Rows; ++y)
			{
				for (i32 x = 0; x < Columns; ++x)
				{
					const LevelCell& cell = GetDocument().GetLevel().GetCells()[(y + 1) * Stride + x + 1];
					const Sprite* outline = cell.Terrain == OccupancyCode::Wall ? &Wall : (cell.Terrain == OccupancyCode::Water ? &WaterCell : (cell.Terrain == OccupancyCode::Puddle ? &PuddleCell : &GridCell));
					Commands.push_back({ outline, x * TileSize, y * TileSize, 0 });
					if (cell.Terrain == OccupancyCode::Puddle || cell.Terrain == OccupancyCode::Water)
						Font.Append(Commands, cell.Terrain == OccupancyCode::Puddle ? "P" : "W", x * TileSize + 3, y * TileSize + 3);
					if (IsSpawner(cell.Spawner))
					{
						Commands.push_back({ &SpawnerCell, x * TileSize, y * TileSize, 0 });
						Font.Append(Commands, cell.Spawner == SpawnerCode::Distraction ? "D" : (cell.Spawner == SpawnerCode::Powerup ? "E" : "M"), x * TileSize + 22, y * TileSize + 3);
					}
				}
			}
		}
		if (SelectedCell >= 0 && (SelectCells || (Inspector && InspectorTab == InspectorPage::Cell)))
			Commands.push_back({ &Selection, (SelectedCell % Stride - 1) * TileSize, (SelectedCell / Stride - 1) * TileSize, 0 });
		if (Selected >= 0 && GetDocument().GetLevel().GetObjects()[Selected].Kind() != static_cast<i32>(ObjectKind::Inactive))
		{
			const LevelObjectRecord& record = GetDocument().GetLevel().GetObjects()[Selected];
			const Sprite* image = GetDocument().GetSprite(record);
			if (image)
			{
				if (ObjectOutline.FrameWidth != image->FrameWidth || ObjectOutline.FrameHeight != image->FrameHeight)
					ObjectOutline = Rectangle(image->FrameWidth, image->FrameHeight, 0xFF00DD00, true);
				const bool snap = SnapScenery || record.Kind() != static_cast<i32>(ObjectKind::Scenery);
				const i32 dx = Dragging ? (snap ? Snap(Mouse.x - DragOrigin.x) : Mouse.x - DragOrigin.x) : 0;
				const i32 dy = Dragging ? (snap ? Snap(Mouse.y - DragOrigin.y) : Mouse.y - DragOrigin.y) : 0;
				Commands.push_back({ &ObjectOutline, static_cast<i32>(record.X()) + dx, static_cast<i32>(record.Y()) + dy, 0 });
			}
		}
		if (Toolbar)
		{
			Commands.push_back({ &Bar, 0, 0, 0 });
			Commands.push_back({ &Bar, 0, 26, 0 });
			Commands.push_back({ &ActiveTool, static_cast<i32>(CurrentTool) * 100, 0, 0 });
			constexpr cstr tools[] = { "Select", "Place", "Tiles", "Undo", "Redo", "Save copy", "Playtest", "Close" };
			for (i32 index = 0; index < 8; ++index)
				Font.Append(Commands, tools[index], index * 100 + 6, 5);
			constexpr cstr categories[] = { "Animals", "Enemies", "Items", "Scenery", "Players", "Special" };
			Font.Append(Commands, CurrentTool == Tool::Collision ? "< Tile brush >" : str("< Type: ") + categories[static_cast<i32>(CurrentCategory)] + " >", 6, 31);
			const str choice = CurrentTool == Tool::Collision ? TileBrushes[TileChoice].Name : (Choices.empty() ? str("No entries") : Choices[ChoiceIndex].Name);
			Font.Append(Commands, "< " + FitText(Font, choice, 350) + " >", 270, 31);
			Font.Append(Commands, "Props    Grid", 674, 31);
		}
		if (Toolbar)
		{
			Commands.push_back({ &Bar, 0, 574, 0 });
			const str status = (Project.IsDirty() ? "* " : "") + Notice;
			Font.Append(Commands, FitText(Font, status, 788), 6, 579);
		}
		if (Inspector)
			DrawInspector();
	}
	if (Mouse.x >= 0 && Mouse.x < 800 && Mouse.y >= 0 && Mouse.y < 600)
		Commands.push_back({ Cursor, Mouse.x, Mouse.y, 0 });
	return Commands;
}

std::vector<SoundRequest> LevelEditor::TakeAudioEvents()
{
	return Preview ? Preview->TakeAudioEvents() : std::vector<SoundRequest>{};
}

bool LevelEditor::RequestWindowClose()
{
	if ((Project.IsDirty() || EditingField) && !DiscardArmed)
	{
		Preview.reset();
		DiscardArmed = true;
		Toolbar = true;
		Notice = "Unsaved edits: close again to quit/discard, or Ctrl+S to save.";
		return false;
	}
	return true;
}

bool LevelEditor::CanvasAt(POINT point) const
{
	return LevelDocument::TileAt(point.x, point.y) >= 0 && (!Toolbar || (point.y >= 52 && point.y < 574)) && !(Inspector && point.x >= InspectorBounds().left && point.x < InspectorBounds().right && point.y >= InspectorBounds().top && point.y < InspectorBounds().bottom);
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
			{ Property::OpenPack, "Open full pack", "Browse copies...", FieldMode::Action }
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
		{ Property::Depth, "Depth offset", std::to_string(record.DepthAdjustment()) }
	};
	if (kind == ObjectKind::Scenery)
	{
		fields.push_back({ Property::Pass, "Render pass", record.RenderPass() ? "Foreground" : "World sorted", FieldMode::Cycle });
		fields.push_back({ Property::Frame, "Texture frame", std::to_string(record.Frame()) });
		fields.push_back({ Property::Texture, "Texture ID", std::to_string(record.TextureOrPermanent()) });
		fields.push_back({ Property::Earlier, "Draw earlier", "Apply", FieldMode::Action });
		fields.push_back({ Property::Later, "Draw later", "Apply", FieldMode::Action });
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
	case Property::OpenPack:
		Engine.ReloadPacks();
		CatalogChanged = true;
		Picker = PickerKind::Pack;
		PendingPack = -1;
		Search.clear();
		PickerFirst = PickerSelected = 0;
		break;
	case Property::FindObject:
	case Property::ChooseLevel:
		Picker = property == Property::FindObject ? PickerKind::Object : PickerKind::Level;
		Search.clear();
		PickerFirst = PickerSelected = 0;
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

RECT LevelEditor::InspectorBounds() const
{
	const i32 height = PanelCollapsed ? HeaderHeight : (Picker != PickerKind::None ? HeaderHeight + (PickerRows + 1) * FieldHeight + 40 : HeaderHeight + TabsHeight + static_cast<i32>(Fields().size()) * FieldHeight + 48);
	const i32 x = std::clamp(static_cast<i32>(PanelPosition.x), 0, 800 - PanelWidth);
	const i32 y = std::clamp(static_cast<i32>(PanelPosition.y), Toolbar ? 52 : 0, (Toolbar ? 574 : 600) - height);
	return { x, y, x + PanelWidth, y + height };
}

bool LevelEditor::HandleInspector(const WindowInput::Result& input)
{
	if (MovingPanel)
	{
		PanelPosition = { Mouse.x - PanelDrag.x, Mouse.y - PanelDrag.y };
		const RECT clamped = InspectorBounds();
		PanelPosition = { clamped.left, clamped.top };
		MovingPanel = input.MouseHeld;
		return true;
	}

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
	if (!Inspector)
		return false;
	if (Picker != PickerKind::None && (input.Back || input.Editor))
	{
		Picker = PickerKind::None;
		return true;
	}
	if (input.BindingKey == 'i' && !input.Control && Picker == PickerKind::None)
	{
		Inspector = false;
		return true;
	}
	const RECT bounds = InspectorBounds();
	const bool inside = Mouse.x >= bounds.left && Mouse.x < bounds.right && Mouse.y >= bounds.top && Mouse.y < bounds.bottom;
	if (inside && input.Click && Mouse.y < bounds.top + HeaderHeight)
	{
		if (Mouse.x >= bounds.right - HeaderHeight)
			PanelCollapsed = !PanelCollapsed;
		else
		{
			PanelDrag = { Mouse.x - bounds.left, Mouse.y - bounds.top };
			MovingPanel = input.MouseHeld;
			Dragging = false;
		}
		return true;
	}
	if (Picker != PickerKind::None)
		return HandlePicker(input);
	if (!inside)
		return false;
	if (!PanelCollapsed && (input.Click || input.RightClick))
	{
		DiscardArmed = false;
		Dragging = false;
		const i32 content = bounds.top + HeaderHeight;
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
	Commands.push_back({ &PanelHeader, bounds.left, bounds.top, 0 });
	Font.Append(Commands, "Properties - drag to move", bounds.left + 6, bounds.top + 4);
	Font.Append(Commands, PanelCollapsed ? "+" : "-", bounds.right - 18, bounds.top + 4);
	if (PanelCollapsed)
		return;
	if (Picker != PickerKind::None)
	{
		DrawPicker();
		return;
	}
	constexpr cstr tabs[] = { "Level", "Object", "Cell", "Project" };
	const i32 content = bounds.top + HeaderHeight;
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
	Font.Append(Commands, EditingField ? "Enter applies; Esc cancels; Ctrl+A selects." : "Click edits; I closes; header +/- folds.", bounds.left + 8, bounds.bottom - 40);
	Font.Append(Commands, SelectCells ? "Cell selection: V returns to objects." : "Shift+click: cell; Ctrl+click: overlap.", bounds.left + 8, bounds.bottom - 20);
}

void LevelEditor::ResetSelection()
{
	Selected = SelectedCell = -1;
	Dragging = EditingField = false;
	Picker = PickerKind::None;
	BuildChoices();
}

std::vector<LevelEditor::PickerEntry> LevelEditor::PickerEntries() const
{
	std::vector<PickerEntry> entries;
	if (Picker == PickerKind::Theme)
	{
		const std::vector<TileSetInfo>& themes = GetDocument().GetThemes();
		for (size_t index = 0; index < themes.size(); ++index)
			entries.push_back({ index, themes[index].Name });
	}
	else if (Picker == PickerKind::Pack)
	{
		const std::vector<LevelPackInfo>& packs = Engine.GetData().GetPacks();
		for (size_t index = 0; index < packs.size(); ++index)
			entries.push_back({ index, packs[index].AssetPrefix + packs[index].Name });
	}
	else if (Picker == PickerKind::Level)
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
	const str search = lowercase(Search);
	entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const PickerEntry& entry) { return lowercase(entry.Label).find(search) == str::npos; }), entries.end());
	return entries;
}

void LevelEditor::ChoosePicker(size_t index)
{
	const PickerKind kind = Picker;
	if (kind == PickerKind::Theme)
	{
		GetDocument().SetTileSet(index);
		BuildChoices();
		Notice = "Theme changed. Existing scenery art retained; palette and background updated.";
	}
	else if (kind == PickerKind::Object)
	{
		Selected = static_cast<i32>(index);
		InspectorTab = InspectorPage::Object;
		CurrentTool = Tool::Select;
		SelectCells = false;
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

bool LevelEditor::HandlePicker(const WindowInput::Result& input)
{
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
	const i32 top = bounds.top + HeaderHeight + FieldHeight;
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
	Font.Append(Commands, FitText(Font, title + ": " + Search + "_", PanelWidth - 16), bounds.left + 8, bounds.top + HeaderHeight + 4);
	const std::vector<PickerEntry> entries = PickerEntries();
	for (size_t row = 0; row < PickerRows && PickerFirst + row < entries.size(); ++row)
	{
		const i32 y = bounds.top + HeaderHeight + FieldHeight + static_cast<i32>(row) * FieldHeight;
		if (PickerFirst + row == PickerSelected)
			Commands.push_back({ &FieldBar, bounds.left + 4, y, 0 });
		Font.Append(Commands, FitText(Font, entries[PickerFirst + row].Label, PanelWidth - 16), bounds.left + 8, y + 4);
	}
	Font.Append(Commands, "Type to search; arrows/wheel; Enter picks.", bounds.left + 8, bounds.bottom - 36);
	Font.Append(Commands, "Esc cancels; Ctrl+A clears search.", bounds.left + 8, bounds.bottom - 18);
}
