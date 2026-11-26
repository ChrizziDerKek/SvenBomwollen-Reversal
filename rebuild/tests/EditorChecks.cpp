#include "EditorChecks.h"
#include "Editor/LevelEditor.h"
#include "Editor/EditorGraphics.h"
#include "Game/GameUi.h"
#include "Scene/Characters/Characters.h"
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <limits>
#include <cmath>

namespace
{
void Require(bool condition, cstr message)
{
	if (!condition)
		throw std::runtime_error(message);
}
void MoveSidebar(LevelEditor& editor, i32 x, i32 y)
{
	const bool visible = editor.IsPaletteVisible();
	WindowInput::Result input;
	if (!visible)
	{
		input.BindingKey = 'b';
		editor.Handle(input, 0);
	}
	const RECT bounds = editor.GetPaletteBounds();
	input = {};
	input.Click = input.MouseHeld = true;
	input.Mouse = { bounds.left + 10, bounds.top + 8 };
	editor.Handle(input, 0);
	input.Click = false;
	input.Mouse = { x + 10, y + 8 };
	editor.Handle(input, 0);
	input.MouseHeld = false;
	editor.Handle(input, 0);
	if (!visible)
	{
		input = {};
		input.BindingKey = 'b';
		editor.Handle(input, 0);
	}
}
template <class T>
void Put(bytestream& bytes, size_t offset, T value)
{
	std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
void WriteFixture(const fpath& path, const bytestream& bytes)
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	file.write(reinterpret_cast<cstr>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	Require(static_cast<bool>(file), "Cannot write editor fixture.");
}
bool Rejects(const std::function<void()>& operation)
{
	try
	{
		operation();
	}
	catch (const std::exception&)
	{
		return true;
	}
	return false;
}
size_t PreviewFolders(const fpath& root)
{
	size_t count = 0;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root))
		if (entry.path().filename().u8string().rfind("_EditorPreview_", 0) == 0)
			++count;
	return count;
}

void CheckAuthoring(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, size_t grid, bool capture)
{
	using ObjectProperty = LevelDocument::ObjectProperty;
	LevelDocument document(engine, pack, level);
	std::vector<i32> empty;
	for (i32 y = 1; y <= 15; ++y)
	{
		for (i32 x = 1; x <= 20; ++x)
		{
			const i32 tile = y * 22 + x;
			const LevelCell& cell = document.GetLevel().GetCells()[tile];
			if (cell.Terrain == OccupancyCode::Empty && cell.ItemCode == ItemCode::None && cell.Spawner == SpawnerCode::None)
				empty.push_back(tile);
		}
	}
	Require(empty.size() > 40, "Authoring fixture lacks free cells.");
	const i32 tile = LevelDocument::TileAt(100, 140);
	const bytestream original = document.GetBytes();
	document.SetMetadata("Metadata-only edit", level.Music);
	Require(document.IsDirty() && document.GetBytes() == original && document.Undo() && !document.IsDirty() && document.Redo() && document.GetInfo().Name == "Metadata-only edit" && document.Undo(), "Metadata-only edits were not tracked independently of level bytes.");
	document.SetSpawner(tile, SpawnerCode::Mixed);
	Require(document.GetLevel().GetCells()[tile].Spawner == SpawnerCode::Mixed && document.GetLevel().GetCells()[tile].HelpId == (pack.Edition == GameEdition::XXX ? 3 : 0), "Spawner editing lost the packed help ID.");
	Require(document.Undo() && document.GetBytes() == original && document.Redo(), "Spawner painting is not byte-exact/undoable.");
	document.SetSpawner(tile, SpawnerCode::None);
	document.PaintTerrain(tile, OccupancyCode::Puddle);
	document.PaintTerrain(empty[20], OccupancyCode::Puddle);
	document.PaintTerrain(empty[21], OccupancyCode::Water);
	Require(document.GetLevel().GetPuddles() == std::vector<i32>{ tile, empty[20] } && document.GetBytes()[grid + tile * 24 + 5] == 0xA5 && document.GetBytes()[grid + tile * 24 + 13] == 0xB6, "Terrain/puddle editing damaged packed bytes or destination links.");
	const bytestream puddles = document.GetBytes();
	Require(Rejects([&]() { document.SetTeleporters({ tile, tile }); }) && document.GetBytes() == puddles, "Invalid destination edit was not atomic.");
	document.SetCellDirection(empty[21], 5);
	document.PaintTerrain(empty[21], OccupancyCode::Water);
	Require(document.GetLevel().GetCells()[empty[21]].Direction == 5, "Repainting unchanged water reset its direction.");
	document.PaintTerrain(tile, OccupancyCode::Empty);
	Require(document.GetLevel().GetPuddles() == std::vector<i32>{ empty[20] }, "Removing a puddle left a phantom destination.");
	Require(document.Undo() && document.GetLevel().GetPuddles().size() == 2, "Puddle removal cannot be undone as one edit.");
	document.SetTeleporters({ empty[20], tile });
	document.RebuildTeleporters();
	Require(document.GetLevel().GetPuddles().size() == 2, "Destination table rebuild failed.");
	LevelDocument limits(engine, pack, level);
	for (size_t index = 0; index < 16; ++index)
	{
		limits.SetSpawner(empty[index], SpawnerCode::Distraction);
		limits.PaintTerrain(empty[index + 20], OccupancyCode::Puddle);
	}
	const bytestream full = limits.GetBytes();
	Require(Rejects([&]() { limits.SetSpawner(empty[16], SpawnerCode::Distraction); }) && Rejects([&]() { limits.PaintTerrain(empty[36], OccupancyCode::Puddle); }) && limits.GetBytes() == full, "Native marker limits were exceeded or rejection mutated the document.");
	const POINT point{ 113, 157 };
	const i32 scenery = document.Place(ObjectKind::Scenery, tile, 0, &point);
	document.Move(scenery, 7, -3, false);
	Require(document.GetLevel().GetObjects()[scenery].X() == 120 && document.GetLevel().GetObjects()[scenery].Y() == 154, "Free scenery placement/drag snapped to grid.");
	document.SetObjectProperty(scenery, ObjectProperty::X, 119.5f);
	document.SetObjectProperty(scenery, ObjectProperty::Depth, 75);
	const i32 originalDepth = document.GetLevel().GetObjects()[scenery].DepthAdjustment();
	document.Reorder(scenery, true);
	Require(document.GetLevel().GetObjects()[scenery].DepthAdjustment() < originalDepth && document.Undo() && document.GetLevel().GetObjects()[scenery].DepthAdjustment() == originalDepth, "Draw-later failed or changed object slots.");
	document.SetObjectProperty(scenery, ObjectProperty::Pass, 1);
	Require(document.GetLevel().GetObjects()[scenery].RenderPass() == 1, "Foreground pass did not serialize.");
	document.SetObjectProperty(scenery, ObjectProperty::Pass, 0);
	const i32 animal = document.Place(ObjectKind::Goat, empty[22]);
	document.SetObjectProperty(animal, ObjectProperty::Facing, 3);
	document.SetObjectProperty(animal, ObjectProperty::Impatience, 8);
	Require(document.GetLevel().GetObjects()[animal].Facing() == 3 && document.GetLevel().GetObjects()[animal].Impatience() == 8, "Animal inspector fields were not serialized.");
	const bytestream beforeInvalid = document.GetBytes();
	Require(Rejects([&]() { document.SetObjectProperty(animal, ObjectProperty::Facing, 8); }) && Rejects([&]() { document.PaintTerrain(document.GetLevel().GetObjects()[animal].Tile(), OccupancyCode::Wall); }) && document.GetBytes() == beforeInvalid, "Invalid inspector edit damaged an actor or its collision link.");
	const str originalSet = document.GetInfo().TileSet;
	Require(document.GetPack().TileSets.size() > 1, "Other installed texture packs were not offered.");
	document.SetTileSet(1);
	Require(document.GetInfo().TileSet != originalSet && document.GetLevel().GetObjects()[scenery].Width() == document.GetSprite(document.GetLevel().GetObjects()[scenery])->FrameWidth, "Texture pack change did not update scenery dimensions.");
	Require(document.Undo() && document.GetInfo().TileSet == originalSet && document.Redo(), "Texture pack metadata did not participate in undo/redo.");
	document.SetTimeLimit(321.5f);
	document.SetMetadata("Neue Wiese & <01>", level.Music);
	Require(document.Undo() && document.GetInfo().Name == level.Name && document.Redo(), "Metadata history failed.");
	Require(Rejects([&]() { document.SetTimeLimit(-1); }) && Rejects([&]() { document.SetTimeLimit(std::numeric_limits<f32>::infinity()); }) && Rejects([&]() { document.SetMetadata("", level.Music); }), "Invalid level parameters were accepted.");
	document.SetUfoVisible(true);
	Require(!document.GetLevel().GetHidesUfo(), "UFO visibility has the wrong edition polarity.");
	if (pack.Edition == GameEdition::XXX)
	{
		document.SetCellHelp(empty[23], 7);
		document.SetSpawner(empty[23], SpawnerCode::Powerup);
		Require(document.GetLevel().GetCells()[empty[23]].HelpId == 7, "Spawner update overwrote new help ID.");
		document.SetSpawner(empty[23], SpawnerCode::None);
		document.SetWeather(WeatherMode::Rain);
		document.SetBonusMode(BonusMode::Girl);
		Require(document.GetLevel().GetWeatherMode() == WeatherMode::Rain && document.GetLevel().GetBonusMode() == BonusMode::Girl && !document.GetLevel().GetHidesUfo(), "XXX level flags overwrite unrelated settings.");
		document.SetBonusMode(BonusMode::Sven);
	}
	else
		Require(Rejects([&]() { document.SetWeather(WeatherMode::Snow); }), "v6 accepted an XXX-only header flag.");
	document.SetSpawner(empty[23], SpawnerCode::Mixed);
	const str scratch = pack.AssetPrefix + "_editor_check_authoring/level.lvl";
	const LevelInfo preview = document.WritePreview(scratch);
	LevelScene scene(engine, document.GetPack(), preview, 1);
	Require(scene.GetInitialTime() == 321.5f && scene.GetGrid().GetOccupancy(tile) == OccupancyCode::Puddle && scene.GetGrid().GetOccupancy(empty[20]) == OccupancyCode::Puddle, "Authored timer/teleports did not reach gameplay.");
	scene.GetItems().SpawnItems();
	size_t pickups = 0;
	for (const ItemDefinition& item : GetItemDefinitions())
		pickups += scene.GetObjects().Find(item.Kind).size();
	Require(pickups == 1, "Painted random spawner did not generate a real item.");
	const f32 beforeX = scene.GetPlayer().GetX(), beforeY = scene.GetPlayer().GetY();
	scene.GetPlayer().Teleport(tile);
	Require(scene.GetPlayer().GetX() != beforeX || scene.GetPlayer().GetY() != beforeY, "Painted puddle destination table did not teleport the player.");
	for (i32 tick = 0; tick < 120; ++tick)
		scene.Update(1.0f / 60, 0, 0, false);

	LevelEditor editor(engine, pack, level, 1);
	{
		WindowInput::Result hidePalette;
		hidePalette.BindingKey = 'b';
		editor.Handle(hidePalette, 0);
	}
	MoveSidebar(editor, 480, 26);
	auto send = [&](WindowInput::Result input) { return editor.Handle(input, 0); };
	auto key = [&](i32 binding) { WindowInput::Result input; input.BindingKey = binding; send(input); };
	auto click = [&](i32 x, i32 y) { WindowInput::Result input; input.Click = true; input.Mouse = { x, y }; send(input); };
	key('i');
	click(editor.GetInspectorBounds().left + 180, editor.GetInspectorBounds().top + 56); // Level timer field.
	WindowInput::Result input;
	input.Text = "not a number";
	send(input);
	input = {};
	input.Enter = true;
	send(input);
	Require(editor.GetDocument().GetLevel().GetTimeLimit() == LevelFile(engine.ReadAsset(level.Map), level.Map).GetTimeLimit(), "Invalid text entry changed the timer.");
	input = {};
	input.Back = true;
	send(input);
	click(editor.GetInspectorBounds().left + 180, editor.GetInspectorBounds().top + 56);
	input = {};
	input.Text = "75.5";
	send(input);
	input = {};
	input.Enter = true;
	send(input);
	Require(editor.GetDocument().GetLevel().GetTimeLimit() == 75.5f, "Inspector timer text did not apply.");
	click(editor.GetInspectorBounds().left + 180, editor.GetInspectorBounds().top + 28);
	input = {};
	input.Text = "Wiese mit Leerzeichen";
	input.Confirm = true; // Space must not submit a text field.
	send(input);
	input = {};
	input.Enter = true;
	send(input);
	Require(editor.GetDocument().GetInfo().Name == "Wiese mit Leerzeichen", "Spaces in the title submitted/corrupted the edit.");
	key('i');
	key('c');
	input = {};
	input.Wheel = -3; // Collision -> puddle -> water -> distraction spawner.
	send(input);
	click(100, 140);
	Require(editor.GetDocument().GetLevel().GetCells()[tile].Spawner == SpawnerCode::Distraction, "Tile brush did not place a random spawner.");
	input = {};
	input.RightClick = true;
	input.Mouse = { 100, 140 };
	send(input);
	Require(editor.GetDocument().GetLevel().GetCells()[tile].Spawner == SpawnerCode::None, "Spawner erase changed the wrong field.");
	key('p');
	if (!editor.IsPaletteVisible())
		key('b');
	const RECT palette = editor.GetPaletteBounds();
	click(palette.left + PlacementPalette::Width / PlacementPalette::TabsPerRow + 2, palette.top + 28);
	click(palette.left + 10, palette.top + 100);
	key('b');
	key('n');
	key('a'); // Legacy canvas-origin placement; centered placement has separate coverage.
	click(113, 157);
	Require(editor.GetDocument().GetLevel().GetObjects()[2].X() == 113 && editor.GetDocument().GetLevel().GetObjects()[2].Y() == 157, "Free placement tool ignored the unsnapped pointer.");
	key('v');
	input = {};
	input.Right = true;
	send(input);
	input.Shift = true;
	send(input);
	Require(editor.GetDocument().GetLevel().GetObjects()[2].X() == 124, "Free scenery keyboard nudge did not use 1/10px steps.");
	key('i');
	if (capture)
		engine.DrawFrame(editor.Draws(), pack.Edition == GameEdition::XXX ? "editor-properties-xxx.bmp" : "editor-properties-zwo.bmp");
	click(editor.GetInspectorBounds().left + 90, editor.GetInspectorBounds().top + 4); // Object inspector.
	click(editor.GetInspectorBounds().left + 180, editor.GetInspectorBounds().top + 84);
	input = {};
	input.Text = "125.5";
	send(input);
	input = {};
	input.Enter = true;
	send(input);
	Require(editor.GetDocument().GetLevel().GetObjects()[2].X() == 125.5f, "Object inspector did not apply a fractional scenery position.");
	if (capture)
		engine.DrawFrame(editor.Draws(), pack.Edition == GameEdition::XXX ? "editor-object-xxx.bmp" : "editor-object-zwo.bmp");
	Log("PASS: editor authoring terrain/puddles/spawner limits, packed fields, free scenery, depth/pass, texture packs, level/animal parameters, metadata history and inspector text routing.");
}

void CheckProjectWorkflow(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, bool capture)
{
	const str suffix = pack.Edition == GameEdition::XXX ? "xxx" : "zwo";
	const bytestream source = engine.ReadAsset(level.Map);
	LevelEditor editor(engine, pack, level, 1);
	{
		WindowInput::Result hidePalette;
		hidePalette.BindingKey = 'b';
		editor.Handle(hidePalette, 0);
	}
	MoveSidebar(editor, 480, 26);
	auto send = [&](WindowInput::Result input) { return editor.Handle(input, 0); };
	auto key = [&](i32 binding) { WindowInput::Result input; input.BindingKey = binding; send(input); };
	auto click = [&](i32 x, i32 y, bool control = false) { WindowInput::Result input; input.Click = true; input.Control = control; input.Mouse = { x, y }; send(input); };
	auto tab = [&](i32 index) { const RECT box = editor.GetInspectorBounds(); click(box.left + index * 80 + 10, box.top + 4); };
	auto field = [&](i32 row) { const RECT box = editor.GetInspectorBounds(); click(box.left + 180, box.top + 24 + row * 28); };
	LevelDocument& document = editor.GetDocument();
	const POINT position{ 100, 140 };
	const i32 first = document.Place(ObjectKind::Scenery, LevelDocument::TileAt(100, 140), 0, &position);
	const i32 second = document.Place(ObjectKind::Scenery, LevelDocument::TileAt(100, 140), 0, &position);
	const Sprite* image = document.GetSprite(document.GetLevel().GetObjects()[first]);
	POINT hit{};
	bool found = false;
	for (i32 y = 0; y < image->FrameHeight && !found; ++y)
	{
		for (i32 x = 0; x < image->FrameWidth && !found; ++x)
		{
			const POINT point{ position.x + x, position.y + y };
			const i32 tile = LevelDocument::TileAt(point.x, point.y);
			if (tile >= 0 && point.x < 460 && point.y < 574 && (image->Pixels[y * image->Width + x] & 0xFFFFFF) != 0xFF00FF && document.GetLevel().GetCells()[tile].Terrain == OccupancyCode::Empty)
			{
				hit = point;
				found = true;
			}
		}
	}
	Require(found, "Cannot find a covered cell for selection regression.");
	const i32 covered = LevelDocument::TileAt(hit.x, hit.y);
	document.PaintTerrain(covered, OccupancyCode::Puddle);
	const bytestream beforePicking = document.GetBytes();
	click(hit.x, hit.y);
	const i32 selected = editor.GetSelectedObject();
	Require(selected == first || selected == second, "Object selection picked a grid overlay instead of scenery.");
	click(hit.x, hit.y, true);
	Require(editor.GetSelectedObject() != selected && (editor.GetSelectedObject() == first || editor.GetSelectedObject() == second), "Ctrl+click did not cycle opaque overlapping sprites.");
	key('i');
	tab(2);
	click(hit.x, hit.y);
	Require(editor.GetSelectedCell() == covered && editor.GetSelectedObject() == -1 && document.GetBytes() == beforePicking, "Cell inspector selected/moved scenery over a puddle.");
	if (capture)
		engine.DrawFrame(editor.Draws(), ("editor-cell-covered-" + suffix + ".bmp").c_str());
	const RECT initial = editor.GetInspectorBounds();
	Require(initial.right - initial.left == 320 && initial.bottom - initial.top < 522, "Inspector was not made smaller.");
	MoveSidebar(editor, 80, 52);
	const RECT moved = editor.GetInspectorBounds();
	Require(moved.left == 80 && moved.top == 52 + PlacementPalette::HeaderHeight + PlacementPalette::TabHeight && document.GetBytes() == beforePicking, "Sidebar drag did not move its embedded properties or edited data.");
	click(600, 300);
	Require(editor.GetSelectedCell() == LevelDocument::TileAt(600, 300), "Former sidebar location still blocked canvas selection.");

	tab(1);
	field(0);
	WindowInput::Result input;
	input.Text = std::to_string(first) + " ";
	send(input);
	input = {};
	input.Enter = true;
	send(input);
	Require(editor.GetSelectedObject() == first, "Searchable object list could not select obscured scenery.");
	key('o'); // Close the object list and reopen embedded properties.
	key('i');
	tab(0);
	const std::vector<TileSetInfo>& themes = document.GetThemes();
	size_t themeIndex = themes.size();
	for (size_t index = 0; index < themes.size(); ++index)
	{
		if (!pack.AssetPrefix.empty() && themes[index].Background.rfind(pack.AssetPrefix, 0) != 0)
		{
			themeIndex = index;
			break;
		}
	}
	if (themeIndex == themes.size())
		themeIndex = themes.size() > 1 ? 1 : 0;
	const str themeName = themes[themeIndex].Name;
	const str background = themes[themeIndex].Background;
	field(2);
	input = {};
	input.Text = themeName;
	send(input);
	if (capture)
		engine.DrawFrame(editor.Draws(), ("editor-theme-picker-" + suffix + ".bmp").c_str());
	input = {};
	input.Enter = true;
	send(input);
	Require(document.GetSprite(document.GetLevel().GetObjects()[first]) == image && engine.GetData().TileSet(document.GetPack(), document.GetInfo()).Background == background, "Global theme choice skipped another edition or replaced existing scenery art.");
	Require(document.Undo() && document.GetBytes() == beforePicking && document.Redo(), "Theme remapping did not undo/redo exactly.");
	tab(3);
	field(6);
	Require(editor.GetDocument().GetBytes() != source && editor.GetProject().GetEntries().size() == 1, "New pack discarded dirty work without confirmation.");
	field(6);
	const size_t players = pack.Edition == GameEdition::XXX ? 2 : 1;
	size_t active = 0;
	for (const LevelObjectRecord& record : editor.GetDocument().GetLevel().GetObjects())
		active += record.Kind() != static_cast<i32>(ObjectKind::Inactive);
	Require(active == players && editor.GetDocument().GetLevel().GetPuddles().empty() && editor.GetProject().IsDirty(), "New project is not a clean authored blank with valid player starts.");
	LevelProject& project = editor.GetProject();
	project.SetName("Workflow & " + suffix);
	project.SetEpisode("Wiese <01>");
	project.Current().Place(ObjectKind::Sheep, LevelDocument::TileAt(100, 140));
	project.Current().SetTimeLimit(90);
	// Any installed theme must also work on a blank level.
	const std::vector<TileSetInfo>& blankThemes = project.Current().GetThemes();
	const auto target = std::find_if(blankThemes.begin(), blankThemes.end(), [&](const TileSetInfo& theme) { return theme.Background == background; });
	Require(target != blankThemes.end(), "Blank project lost the global theme library.");
	project.Current().SetTileSet(static_cast<size_t>(std::distance(blankThemes.begin(), target)));
	field(3); // Add blank level through project UI.
	Require(project.GetEntries().size() == 2 && project.GetActive() == 1, "Add blank level did not register a second project entry.");
	project.SetEpisode("Garten & 02");
	project.Current().Place(ObjectKind::Goat, LevelDocument::TileAt(100, 140));
	project.Select(0);
	Require(project.Current().GetLevel().GetTimeLimit() == 90, "Switching project levels lost unsaved edits.");
	project.Select(1);
	project.Duplicate();
	Require(project.GetEntries().size() == 3 && project.Current().GetBytes() == project.GetEntries()[1].Document->GetBytes(), "Duplicate level did not preserve authored data.");
	project.Import(engine.GetData().AssetPath(level.Map));
	Require(project.GetEntries().size() == 4 && project.Current().GetBytes() == source, "Import did not register or preserve the original level.");
	Require(Rejects([&]() { project.Import(engine.GetData().AssetPath(pack.Descriptor)); }) && project.GetEntries().size() == 4, "Invalid level import partially changed the project.");
	bytestream invalid = source;
	u32 nameLength = 0;
	std::memcpy(&nameLength, invalid.data() + 12, 4);
	Put(invalid, 16 + nameLength + 9048, std::numeric_limits<f32>::quiet_NaN());
	const fpath invalidPath = engine.GetData().GetRoot() / std::filesystem::u8path(pack.AssetPrefix + "_editor_invalid.lvl");
	WriteFixture(invalidPath, invalid);
	Require(Rejects([&]() { project.Import(invalidPath); }) && project.GetEntries().size() == 4, "Nonfinite imported coordinates reached the editor canvas.");
	Require(Rejects([&]() { project.SetName("../\n"); }) && Rejects([&]() { project.SetName("../outside"); }) && Rejects([&]() { project.Select(99); }), "Invalid project properties were accepted.");
	const str saved = project.Save();
	const bytestream descriptor = engine.ReadAsset(saved);
	const GameData previous(engine.GetData().GetRoot());
	const auto exported = std::find_if(previous.GetPacks().begin(), previous.GetPacks().end(), [&](const LevelPackInfo& value) { return value.Descriptor == saved; });
	Require(exported != previous.GetPacks().end() && exported->LevelSets.size() == 2 && !project.IsDirty(), "Named multi-level pack/episode XML did not round-trip.");
	size_t levels = 0;
	for (const LevelSetInfo& episode : exported->LevelSets)
	{
		for (const LevelInfo& info : episode.Levels)
		{
			LevelScene scene(engine, *exported, info, 1);
			++levels;
		}
	}
	Require(levels == 4, "Saved folder pack lost levels.");
	project.Current().SetTimeLimit(111);
	const fpath descriptorPath = engine.GetData().AssetPath(saved);
	const DWORD attributes = GetFileAttributesW(descriptorPath.c_str());
	Require(SetFileAttributesW(descriptorPath.c_str(), attributes | FILE_ATTRIBUTE_READONLY) != 0, "Cannot protect export for failed-save regression.");
	const bool rejected = Rejects([&]() { project.Save(); });
	SetFileAttributesW(descriptorPath.c_str(), attributes);
	Require(rejected && project.IsDirty() && engine.ReadAsset(saved) == descriptor, "Failed multi-level save replaced the last good descriptor or cleared dirty state.");
	for (const LevelSetInfo& episode : exported->LevelSets)
	{
		for (const LevelInfo& info : episode.Levels)
			Require(!engine.ReadAsset(info.Map).empty(), "Failed save removed a level referenced by the old descriptor.");
	}
	Require(project.Save() == saved && !project.IsDirty() && engine.ReadAsset(level.Map) == source, "Project retry changed identity/source or failed to commit.");
	if (capture)
		engine.DrawFrame(editor.Draws(), ("editor-project-" + suffix + ".bmp").c_str());
	LevelEditor fromTitle(engine, pack, level, 1, true);
	{
		WindowInput::Result hidePalette;
		hidePalette.BindingKey = 'b';
		fromTitle.Handle(hidePalette, 0);
	}
	Require(fromTitle.GetProject().IsDirty(), "Title-screen editor did not create a blank project.");
	WindowInput::Result play;
	play.Playtest = true;
	fromTitle.Handle(play, 0);
	Require(fromTitle.IsPlaytesting(), "Blank project cannot enter playtest.");
	LevelProject whole(engine, pack, level);
	whole.Open(pack);
	size_t originalLevels = 0;
	for (const LevelSetInfo& episode : pack.LevelSets)
		originalLevels += episode.Levels.size();
	Require(whole.GetEntries().size() == originalLevels && whole.Current().GetBytes() == engine.ReadAsset(pack.LevelSets.front().Levels.front().Map), "Opening a full working copy lost levels or altered source bytes.");
	engine.ReloadPacks();
	Log("PASS: editor selection under scenery/overlaps, compact movable/foldable inspector, global mixed-edition themes, blank/multiple/duplicate/imported levels, named folder export and failed-save preservation.");
}

void CheckProjectManagement(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, bool capture)
{
	const str suffix = pack.Edition == GameEdition::XXX ? "xxx" : "zwo";
	const bytestream source = engine.ReadAsset(level.Map);
	LevelProject project(engine, pack, level);
	project.New();
	project.SetName("Manage " + suffix);
	project.SetEpisode("A");
	project.Current().SetMetadata("A1", project.Current().GetInfo().Music);
	LevelDocument* first = &project.Current();
	project.AddBlank();
	project.SetEpisode("B");
	project.Current().SetMetadata("B1", project.Current().GetInfo().Music);
	LevelDocument* other = &project.Current();
	project.AddBlank();
	project.SetEpisode("A");
	project.Current().SetMetadata("A2", project.Current().GetInfo().Music);
	project.Current().SetTimeLimit(123.5f);
	LevelDocument* moved = &project.Current();
	const bytestream authored = moved->GetBytes();
	const str saved = project.Save();
	Require(!project.IsDirty() && project.MoveLevel(false) && &project.Current() == moved && project.GetActive() == 0 && project.GetEntries()[1].Document.get() == other && project.GetEntries()[2].Document.get() == first && project.IsDirty(), "Level reorder lost identity, changed another levelset or failed to dirty equal-episode ordering.");
	Require(!project.MoveLevel(false) && project.Undo() && !project.IsDirty() && project.GetActive() == 2 && &project.Current() == moved, "Boundary move created history or reorder undo lost the saved structure/selection.");
	Require(project.Redo() && project.IsDirty() && &project.Current() == moved && moved->GetBytes() == authored, "Reorder redo changed authored bytes or selected a different document.");
	project.Save();
	Require(project.Undo() && project.IsDirty() && project.Redo() && !project.IsDirty(), "Saving reordered levels lost the structural undo savepoint.");
	project.DeleteLevel();
	Require(project.GetEntries().size() == 2 && project.GetActive() == 0 && project.IsDirty(), "Deleting a level did not select a remaining level or dirty the project.");
	const bytestream descriptor = engine.ReadAsset(saved);
	const fpath descriptorPath = engine.GetData().AssetPath(saved);
	const DWORD attributes = GetFileAttributesW(descriptorPath.c_str());
	Require(SetFileAttributesW(descriptorPath.c_str(), attributes | FILE_ATTRIBUTE_READONLY) != 0, "Cannot protect project descriptor for deletion-save regression.");
	const bool rejected = Rejects([&]() { project.Save(); });
	SetFileAttributesW(descriptorPath.c_str(), attributes);
	Require(rejected && project.IsDirty() && engine.ReadAsset(saved) == descriptor, "Failed deletion save replaced the last playable descriptor or cleared dirty state.");
	project.Save();
	Require(project.Undo() && &project.Current() == moved && moved->GetBytes() == authored && project.IsDirty(), "Deletion undo after save did not restore the same edited document.");
	Require(moved->Undo() && moved->GetLevel().GetTimeLimit() != 123.5f && moved->Redo() && moved->GetBytes() == authored, "Structural history destroyed the restored document's edit history.");
	Require(project.Redo() && !project.IsDirty() && project.Undo(), "Deletion redo did not return to its saved structure.");
	project.Save();
	const GameData catalog(engine.GetData().GetRoot());
	const auto exported = std::find_if(catalog.GetPacks().begin(), catalog.GetPacks().end(), [&](const LevelPackInfo& value) { return value.Descriptor == saved; });
	Require(exported != catalog.GetPacks().end() && exported->LevelSets.size() == 2 && exported->LevelSets[0].Name == "A" && exported->LevelSets[0].Levels.size() == 2 && exported->LevelSets[0].Levels[0].Name == "A2" && exported->LevelSets[0].Levels[1].Name == "A1" && exported->LevelSets[1].Levels[0].Name == "B1", "Saved level ordering/membership differs from the project.");
	Require(engine.ReadAsset(exported->LevelSets[0].Levels[0].Map) == authored && engine.ReadAsset(level.Map) == source, "Reorder/delete save changed level bytes or source assets.");
	LevelScene playable(engine, *exported, exported->LevelSets[0].Levels[0], 1);
	Require(playable.GetInitialTime() == 123.5f, "Restored reordered level does not load into gameplay.");
	project.Select(1); // B is the only level in its levelset.
	Require(!project.MoveLevel(false) && !project.MoveLevel(true), "Reorder moved a level outside its levelset.");
	project.DeleteLevel();
	project.Save();
	const GameData deletedCatalog(engine.GetData().GetRoot());
	const auto deleted = std::find_if(deletedCatalog.GetPacks().begin(), deletedCatalog.GetPacks().end(), [&](const LevelPackInfo& value) { return value.Descriptor == saved; });
	Require(deleted != deletedCatalog.GetPacks().end() && deleted->LevelSets.size() == 1 && deleted->LevelSets[0].Name == "A", "Deleting a levelset's final level left an empty episode in the saved pack.");
	project.Select(1);
	project.DeleteLevel();
	Require(project.GetActive() == 0 && project.GetEntries().size() == 1 && Rejects([&]() { project.DeleteLevel(); }), "Deleting the last entry invalidated selection or allowed an empty project.");
	Require(project.Undo() && project.GetEntries().size() == 2 && project.GetActive() == 1 && project.Redo(), "Last-level rejection added history or deletion undo lost selection.");
	project.AddBlank();
	LevelDocument* added = &project.Current();
	Require(project.Undo() && project.GetEntries().size() == 1 && project.Redo() && &project.Current() == added, "Project history lost an added level.");
	project.Duplicate();
	Require(project.Undo() && project.GetEntries().size() == 2 && project.Redo() && project.GetEntries().size() == 3, "Project duplicate is not undoable.");
	project.Import(engine.GetData().AssetPath(level.Map));
	Require(project.Undo() && project.GetEntries().size() == 3 && project.Redo() && project.Current().GetBytes() == source, "Project import is not undoable.");
	project.Undo();
	project.SetName("Branched " + suffix);
	Require(!project.CanRedo() && project.Undo() && project.GetName() == "Manage " + suffix, "A new project action retained stale redo or name undo failed.");
	const str episode = project.GetEntries()[project.GetActive()].Episode;
	project.SetEpisode("Renamed episode");
	Require(project.Undo() && project.GetEntries()[project.GetActive()].Episode == episode, "Levelset assignment is not undoable.");
	project.Open(pack);
	Require(!project.CanUndo() && !project.CanRedo(), "Opening another pack retained old project history.");
	project.New();
	Require(!project.CanUndo() && !project.CanRedo() && !project.Undo() && !project.Redo(), "New project retained old structural history.");
	for (i32 index = 0; index < 140; ++index)
		project.SetName("History " + std::to_string(index));
	i32 undoCount = 0;
	while (project.Undo())
		++undoCount;
	Require(undoCount == 128, "Project history is not bounded to 128 actions.");

	LevelEditor editor(engine, pack, level, 1, true);
	MoveSidebar(editor, 480, 26);
	LevelProject& ui = editor.GetProject();
	ui.Current().SetMetadata("First", ui.Current().GetInfo().Music);
	for (i32 index = 1; index <= 11; ++index)
	{
		ui.AddBlank();
		ui.Current().SetMetadata("Entry " + std::to_string(index), ui.Current().GetInfo().Music);
	}
	LevelDocument* last = &ui.Current();
	auto send = [&](WindowInput::Result input) { editor.Handle(input, 0); };
	auto click = [&](i32 x, i32 y) { WindowInput::Result input; input.Click = true; input.Mouse = { x, y }; send(input); };
	auto open = [&]() { WindowInput::Result key; key.BindingKey = 'i'; send(key); const RECT bounds = editor.GetInspectorBounds(); click(bounds.left + 250, bounds.top + 4); click(bounds.left + 160, bounds.top + 24 + 2 * 28); };
	auto action = [&](i32 column, bool history = false) { const RECT bounds = editor.GetInspectorBounds(); click(bounds.left + (history ? column * 160 : column * 107) + 10, bounds.bottom - (history ? 76 : 104)); };
	open();
	action(0);
	Require(ui.GetActive() == 10 && &ui.Current() == last, "Level picker did not focus/scroll to the active level before reordering.");
	WindowInput::Result key;
	key.Undo = true;
	send(key);
	Require(ui.GetActive() == 11 && &ui.Current() == last, "Level list Ctrl+Z did not undo project ordering.");
	action(1, true);
	Require(ui.GetActive() == 10, "Level list Redo button did not redo project ordering.");
	click(350, 10); // Modal toolbar undo has the same project scope.
	Require(ui.GetActive() == 11 && &ui.Current() == last, "Toolbar undo in level management changed document history instead.");
	key = {};
	key.Text = "Entry 5";
	send(key);
	action(2);
	Require(ui.GetEntries().size() == 12, "Delete confirmation removed a level on its first click.");
	key = {};
	key.Back = true;
	send(key);
	const RECT bounds = editor.GetInspectorBounds();
	click(bounds.left + 160, bounds.top + 24 + 2 * 28); // Reopen, previous confirmation must be gone.
	key = {};
	key.Text = "Entry 5";
	send(key);
	action(2);
	Require(ui.GetEntries().size() == 12, "Esc/reopening retained a deletion confirmation.");
	if (capture)
		engine.DrawFrame(editor.Draws(), ("editor-level-delete-confirm-" + suffix + ".bmp").c_str());
	action(2);
	Require(ui.GetEntries().size() == 11 && std::none_of(ui.GetEntries().begin(), ui.GetEntries().end(), [](const LevelProject::Entry& entry) { return entry.Document->GetInfo().Name == "Entry 5"; }), "Filtered deletion removed the wrong project index.");
	key = {};
	key.Undo = true;
	send(key); // Undo must work even when the filter has no remaining matches.
	Require(ui.GetEntries().size() == 12 && ui.Current().GetInfo().Name == "Entry 5", "No-match level list swallowed project undo.");
	if (capture)
		engine.DrawFrame(editor.Draws(), ("editor-level-management-" + suffix + ".bmp").c_str());
	key = {};
	key.Enter = true;
	send(key);
	Require(editor.GetSelectedObject() == -1 && ui.Current().GetInfo().Name == "Entry 5", "Returning from level management left an invalid object selection.");
	LevelDocument& current = editor.GetDocument();
	const bytestream previous = current.GetBytes();
	current.SetTimeLimit(222);
	key = {};
	key.Undo = true;
	send(key);
	Require(current.GetBytes() == previous && ui.GetEntries().size() == 12, "Ordinary editor undo used project history after leaving the level list.");
	engine.ReloadPacks();
	Log("PASS: project reorder/delete/confirmation, same-levelset ordering, stable documents and separate bounded undo, dirty/savepoint restoration, failed-save/source protection and saved pack reload.");
}

void CheckEditorUsability(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, bool capture)
{
	const str suffix = pack.Edition == GameEdition::XXX ? "xxx" : "zwo";
	LevelEditor placement(engine, pack, level, 1);
	{
		WindowInput::Result hidePalette;
		hidePalette.BindingKey = 'b';
		placement.Handle(hidePalette, 0);
	}
	MoveSidebar(placement, 480, 26);
	auto send = [&](WindowInput::Result input) { return placement.Handle(input, 0); };
	auto key = [&](i32 binding) { WindowInput::Result input; input.BindingKey = binding; send(input); };
	auto click = [&](i32 x, i32 y) { WindowInput::Result input; input.Click = true; input.Mouse = { x, y }; send(input); };
	key('p');
	key('b');
	const RECT palette = placement.GetPaletteBounds();
	click(palette.left + PlacementPalette::Width / PlacementPalette::TabsPerRow + 2, palette.top + 28);
	click(palette.left + 10, palette.top + 100);
	key('b');
	key('n');
	WindowInput::Result pointer;
	pointer.Mouse = { 380, 300 };
	send(pointer);
	const Sprite* art = placement.GetDocument().GetSprite(ObjectKind::Scenery, 0);
	const std::vector<SpriteDraw> ghost = placement.Draws();
	const auto image = std::find_if(ghost.begin(), ghost.end(), [&](const SpriteDraw& draw) { return draw.Sprite == art; });
	Require(image != ghost.end(), "Placement ghost was not drawn.");
	i32 left = art->FrameWidth, right = -1, top = art->FrameHeight, bottom = -1;
	for (i32 y = 0; y < art->FrameHeight; ++y)
	{
		for (i32 x = 0; x < art->FrameWidth; ++x)
		{
			if (art->Keyed && (art->Pixels[y * art->Width + x] & 0xFFFFFF) == 0xFF00FF)
				continue;
			left = std::min(left, x);
			right = std::max(right, x);
			top = std::min(top, y);
			bottom = std::max(bottom, y);
		}
	}
	Require(image->X + (left + right) / 2 == pointer.Mouse.x && image->Y + (top + bottom) / 2 == pointer.Mouse.y, "Scenery anchor did not center its visible art on the pointer.");
	Require(ghost.back().X == pointer.Mouse.x - 9 && ghost.back().Y == pointer.Mouse.y - 9, "Editor cursor hotspot differs from the game's actual click position.");
	if (capture)
		engine.DrawFrame(ghost, ("editor-placement-anchor-" + suffix + ".bmp").c_str());
	click(pointer.Mouse.x, pointer.Mouse.y);
	Require(placement.GetDocument().GetLevel().GetObjects()[2].X() == image->X && placement.GetDocument().GetLevel().GetObjects()[2].Y() == image->Y, "Committed scenery position differs from its preview.");
	click(700, 500);
	Require(placement.GetDocument().GetLevel().GetObjects()[3].Kind() == static_cast<i32>(ObjectKind::Scenery), "Scenery placement did not remain active for another click.");
	key('p');
	key('a');
	click(113, 157);
	Require(placement.GetDocument().GetLevel().GetObjects()[4].X() == 113 && placement.GetDocument().GetLevel().GetObjects()[4].Y() == 157, "Canvas-origin anchor no longer supports exact legacy placement.");
	key('p');
	key('a');
	key('n');
	pointer.Mouse = { 113, 157 };
	send(pointer);
	const std::vector<SpriteDraw> snapped = placement.Draws();
	const auto lastGhost = std::find_if(snapped.rbegin(), snapped.rend(), [&](const SpriteDraw& draw) { return draw.Sprite == art; });
	Require(lastGhost != snapped.rend() && lastGhost->X + (left + right) / 2 == 100 && lastGhost->Y + (top + bottom) / 2 == 140, "Snapped anchor does not match the selected cell center.");

	LevelEditor browser(engine, pack, level, 1);
	{
		WindowInput::Result hidePalette;
		hidePalette.BindingKey = 'b';
		browser.Handle(hidePalette, 0);
	}
	MoveSidebar(browser, 480, 26);
	auto browse = [&](WindowInput::Result input) { return browser.Handle(input, 0); };
	auto browseKey = [&](i32 binding) { WindowInput::Result input; input.BindingKey = binding; browse(input); };
	for (i32 index = 0; index < 16; ++index)
		browser.GetDocument().Place(ObjectKind::Scenery, LevelDocument::TileAt(100, 140), 0);
	browser.GetDocument().SetTimeLimit(444);
	const bytestream beforeUi = browser.GetDocument().GetBytes();
	browseKey('o');
	Require(browser.IsObjectListVisible(), "O did not show the persistent object list.");
	RECT list = browser.GetObjectListBounds();
	WindowInput::Result input;
	input.Click = input.MouseHeld = true;
	input.Mouse = { list.left + 8, list.top + 52 };
	browse(input);
	Require(browser.GetSelectedObject() == 0 && browser.IsObjectListVisible(), "List selection closed the list or failed to select an object.");
	input.Mouse = { 350, 10 }; // Same held gesture crosses Undo; even a repeated press must stay captured.
	browse(input);
	Require(browser.GetDocument().GetBytes() == beforeUi, "Object-list gesture clicked through to Undo.");
	input.Click = input.MouseHeld = false;
	browse(input);
	input.Click = true;
	browse(input);
	Require(browser.GetDocument().GetLevel().GetTimeLimit() != 444, "Toolbar did not accept a new gesture after list capture released.");
	input = {};
	input.Mouse = { list.left + 8, list.top + 80 };
	input.Wheel = -3;
	browse(input);
	input = {};
	input.Click = true;
	input.Mouse = { list.left + 8, list.top + 52 };
	browse(input);
	Require(browser.GetSelectedObject() == 8 && browser.IsObjectListVisible(), "Persistent list scrolling selected the wrong stable slot.");
	input.Mouse.y = list.top + 28;
	browse(input);
	input = {};
	input.Text = "Sheep";
	browse(input);
	input = {};
	input.Enter = true;
	browse(input);
	Require(browser.GetSelectedObject() == 1 && browser.IsObjectListVisible(), "Persistent search did not select the named object and stay open.");
	if (capture)
		engine.DrawFrame(browser.Draws(), ("editor-object-browser-" + suffix + ".bmp").c_str());
	input = {};
	input.Delete = true;
	browse(input);
	Require(browser.GetDocument().GetLevel().GetObjects()[1].Kind() == static_cast<i32>(ObjectKind::Inactive), "Delete missed the list selection.");
	input = {};
	input.Undo = true;
	browse(input);
	Require(browser.GetDocument().GetLevel().GetObjects()[1].Kind() == static_cast<i32>(ObjectKind::Sheep), "List undo did not restore the object.");
	browseKey('i');
	const RECT properties = browser.GetInspectorBounds();
	input = {};
	input.Click = true;
	input.Mouse = { properties.left + 8, properties.top + 4 };
	browse(input);
	input.Mouse = { properties.left + 160, properties.top + 52 };
	browse(input);
	input = {};
	input.Text = "222";
	browse(input);
	input = {};
	input.Click = true;
	const RECT sidebar = browser.GetPaletteBounds();
	input.Mouse = { sidebar.left + 2 * (PlacementPalette::Width / PlacementPalette::TabsPerRow) + 2, sidebar.top + 28 };
	browse(input);
	browseKey('o');
	Require(browser.GetDocument().GetLevel().GetTimeLimit() == 222, "Switching focus to the list abandoned the pending inspector edit.");
	input = {};
	input.Back = true;
	browse(input);
	browseKey('h');
	Require(!browser.IsHudVisible() && !browser.IsObjectListVisible(), "H did not hide the whole editor interface.");
	const std::vector<SpriteDraw> hidden = browser.Draws();
	Require(std::none_of(hidden.begin(), hidden.end(), [](const SpriteDraw& draw) { return (draw.Sprite->FrameWidth == 800 && draw.Sprite->FrameHeight == 26) || draw.Sprite->FrameWidth == 320 || draw.Sprite->FrameWidth == 280; }), "Hidden editor panels or toolbars still cover the canvas.");
	const bytestream hiddenBefore = browser.GetDocument().GetBytes();
	input = {};
	input.Click = true;
	input.Mouse = { 350, 10 };
	browse(input);
	Require(browser.GetDocument().GetBytes() == hiddenBefore, "Hidden Undo still handled a canvas click.");
	if (capture)
		engine.DrawFrame(browser.Draws(), ("editor-hidden-ui-" + suffix + ".bmp").c_str());
	browseKey('h');
	Require(browser.IsHudVisible() && browser.IsObjectListVisible(), "Showing UI lost the persistent object list state.");
	input = {};
	input.Playtest = true;
	browse(input);
	Require(browser.IsPlaytesting(), "Usability test could not enter playtest.");
	const Sprite* clock = engine.LoadSprite(engine.GetData().EditionAsset(engine.GetData().GetThemeEdition(), "Sven2/gfx/timer.bmp"), 1, 1);
	const std::vector<SpriteDraw> withHud = browser.Draws();
	Require(std::any_of(withHud.begin(), withHud.end(), [&](const SpriteDraw& draw) { return draw.Sprite == clock; }), "Playtest lost its default HUD.");
	browseKey('h');
	const std::vector<SpriteDraw> withoutHud = browser.Draws();
	Require(std::none_of(withoutHud.begin(), withoutHud.end(), [&](const SpriteDraw& draw) { return draw.Sprite == clock || (draw.Sprite->FrameWidth == 800 && draw.Sprite->FrameHeight == 26); }), "H left playtest HUD/banner over the level.");
	if (capture)
		engine.DrawFrame(withoutHud, ("editor-playtest-hidden-hud-" + suffix + ".bmp").c_str());
	input = {};
	input.Playtest = true;
	browse(input);
	Require(!browser.IsPlaytesting() && browser.GetDocument().GetBytes() == hiddenBefore, "HUD toggles or playtest changed authored data.");
	Log("PASS: editor cursor hotspot, centered/origin anchors and exact preview/commit, persistent scenery placement, persistent searchable object list, UI gesture ownership, hidden interface hit testing and playtest HUD toggle.");
}

void CheckPaletteAndMusic(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, bool capture)
{
	const str suffix = pack.Edition == GameEdition::XXX ? "xxx" : "zwo";
	Sprite sheet{ std::vector<u32>(96 * 192, 0xFFFF00FF), 96, 96, 96, 2, 1, true };
	for (i32 y = 20; y < 60; ++y)
	{
		for (i32 x = 8; x < 88; ++x)
		{
			if (x < 40 || x >= 56 || y < 36 || y >= 48)
				sheet.Pixels[(96 + y) * sheet.Width + x] = 0xFFFF0000;
		}
	}
	const Sprite thumb = PlacementPalette::MakeThumbnail(sheet, 1);
	Require(thumb.FrameWidth == 64 && thumb.FrameHeight == 32 && thumb.Keyed, "Thumbnail lost aspect ratio or transparent crop.");
	Require(std::all_of(thumb.Pixels.begin(), thumb.Pixels.end(), [](u32 color) { return color == 0xFFFF00FF || color == 0xFFFF0000; }), "Thumbnail filtering bled the magenta key into opaque edges.");
	Require(std::any_of(thumb.Pixels.begin(), thumb.Pixels.end(), [](u32 color) { return color == 0xFFFF00FF; }) && PlacementPalette::MakeThumbnail(sheet, 0).FrameWidth == 1, "Transparent holes/empty frames were not preserved.");
	Require(Rejects([&]() { PlacementPalette::MakeThumbnail(sheet, 2); }), "Thumbnail accepted an out-of-range frame.");

	LevelEditor editor(engine, pack, level, 1);
	auto send = [&](WindowInput::Result input) { return editor.Handle(input, 0); };
	auto key = [&](i32 binding) { WindowInput::Result input; input.BindingKey = binding; send(input); };
	auto click = [&](i32 x, i32 y) { WindowInput::Result input; input.Click = true; input.Mouse = { x, y }; send(input); };
	auto chooseGroup = [&](i32 group) { const RECT bounds = editor.GetPaletteBounds(); click(bounds.left + group % PlacementPalette::TabsPerRow * (PlacementPalette::Width / PlacementPalette::TabsPerRow) + 2, bounds.top + PlacementPalette::HeaderHeight + group / PlacementPalette::TabsPerRow * PlacementPalette::TabRowHeight + 4); };
	auto search = [&](const str& name) {
		const RECT bounds = editor.GetPaletteBounds();
		click(bounds.left + 12, bounds.top + PlacementPalette::HeaderHeight + PlacementPalette::TabHeight + 4);
		WindowInput::Result text;
		text.Text = name;
		send(text);
		text = {};
		text.Enter = true;
		send(text);
	};
	Require(editor.IsPaletteVisible(), "Visual placement sidebar is not initially visible.");
	const bytestream original = editor.GetDocument().GetBytes();
	const std::vector<SpriteDraw> initial = editor.Draws();
	Require(std::count_if(initial.begin(), initial.end(), [](const SpriteDraw& draw) { return draw.Sprite->FrameWidth <= 64 && draw.Sprite->FrameHeight <= 64 && draw.X >= 0 && draw.X < PlacementPalette::Width && draw.Y >= 124 && draw.Y < 476; }) >= 6, "Placement sidebar did not render texture thumbnails.");
	if (capture)
		engine.DrawFrame(initial, ("editor-placement-sidebar-" + suffix + ".bmp").c_str());
	chooseGroup(2); // Actors includes animals, enemies, starts and special authored objects.
	search("Sheep");
	Require(editor.GetDocument().GetBytes() == original, "Choosing a thumbnail modified the level before placement.");
	if (capture)
		engine.DrawFrame(editor.Draws(), ("editor-palette-actors-" + suffix + ".bmp").c_str());
	click(620, 140);
	Require(editor.GetDocument().GetLevel().GetObjects()[2].Kind() == static_cast<i32>(ObjectKind::Sheep) && editor.GetDocument().GetLevel().GetCells()[LevelDocument::TileAt(620, 140)].Terrain == OccupancyCode::Sheep, "Actor thumbnail did not select a linked actor placement tool.");
	chooseGroup(3);
	search("Bone");
	click(660, 140);
	Require(editor.GetDocument().GetLevel().GetObjects()[3].Kind() == static_cast<i32>(ObjectKind::Bone) && editor.GetDocument().GetLevel().GetCells()[LevelDocument::TileAt(660, 140)].ItemCode == ItemCode::Bone, "Item thumbnail did not place its pickup record.");
	chooseGroup(1);
	const RECT bounds = editor.GetPaletteBounds();
	WindowInput::Result input;
	input.Wheel = -1;
	input.Mouse = { bounds.left + 10, bounds.top + 100 };
	send(input);
	click(bounds.left + 10, bounds.top + 100);
	click(700, 180);
	const i32 visible = PlacementPalette::Columns * ((bounds.bottom - bounds.top - PlacementPalette::HeaderHeight - PlacementPalette::TabHeight - PlacementPalette::SearchHeight - PlacementPalette::FooterHeight) / PlacementPalette::CellHeight);
	const i32 expectedTexture = editor.GetDocument().GetTileSet().Tiles.size() > static_cast<size_t>(visible) ? PlacementPalette::Columns : 0;
	Require(editor.GetDocument().GetLevel().GetObjects()[4].Kind() == static_cast<i32>(ObjectKind::Scenery) && editor.GetDocument().GetLevel().GetObjects()[4].TextureOrPermanent() == expectedTexture, "Scrolled scenery thumbnail selected the wrong texture ID.");
	editor.GetDocument().SetTimeLimit(444);
	const bytestream beforeGesture = editor.GetDocument().GetBytes();
	input = {};
	input.Click = input.MouseHeld = true;
	input.Mouse = { bounds.left + 10, bounds.top + 100 };
	send(input);
	input.Mouse = { 350, 10 };
	send(input);
	Require(editor.GetDocument().GetBytes() == beforeGesture, "Palette gesture clicked through to Undo.");
	input.Click = input.MouseHeld = false;
	send(input);
	chooseGroup(2);
	search("Penguin");
	if (editor.GetDocument().Supports(ObjectKind::Penguin))
	{
		click(620, 220);
		Require(editor.GetDocument().GetLevel().GetObjects()[5].Kind() == static_cast<i32>(ObjectKind::Penguin), "Shared XXX animal thumbnail was missing from a supported runtime.");
	}
	else
	{
		Require(editor.GetDocument().GetLevel().GetObjects()[5].Kind() == static_cast<i32>(ObjectKind::Inactive), "Archival Zwo palette exposed an unavailable actor.");
	}
	chooseGroup(2);
	search("Sheep");
	key('h');
	Require(!editor.IsPaletteVisible(), "Hidden HUD still displays the placement sidebar.");
	click(100, 140);
	Require(editor.GetDocument().GetLevel().GetCells()[LevelDocument::TileAt(100, 140)].Terrain == OccupancyCode::Sheep, "Hidden sidebar still intercepted placement into its former area.");
	key('h');
	Require(editor.IsPaletteVisible(), "Showing HUD did not restore the sidebar.");
	key('o');
	Require(!editor.IsPaletteVisible() && editor.IsObjectListVisible(), "Instance browser and placement sidebar were not kept distinct.");
	key('b');
	Require(editor.IsPaletteVisible() && !editor.IsObjectListVisible(), "B did not restore visual placement mode.");
	chooseGroup(static_cast<i32>(PlacementPalette::Group::Tiles));
	const i32 tile = LevelDocument::TileAt(740, 300);
	const RECT tilePalette = editor.GetPaletteBounds();
	Require(tilePalette.top == 26, "Removed placement bar still reserves canvas height.");
	const std::vector<SpriteDraw> tileDraws = editor.Draws();
	Require(std::none_of(tileDraws.begin(), tileDraws.end(), [](const SpriteDraw& draw) { return draw.Sprite->FrameWidth == 800 && draw.Sprite->FrameHeight == 26 && draw.Y == 26; }), "Old second toolbar is still drawn.");
	if (capture)
		engine.DrawFrame(tileDraws, ("editor-tile-sidebar-" + suffix + ".bmp").c_str());
	for (size_t brush = 0; brush < PlacementPalette::GetTileBrushes().size(); ++brush)
	{
		const bytestream beforeChoice = editor.GetDocument().GetBytes();
		click(tilePalette.left + static_cast<i32>(brush % PlacementPalette::Columns) * PlacementPalette::CellWidth + 10, tilePalette.top + PlacementPalette::HeaderHeight + PlacementPalette::TabHeight + PlacementPalette::SearchHeight + static_cast<i32>(brush / PlacementPalette::Columns) * PlacementPalette::CellHeight + 10);
		Require(editor.GetDocument().GetBytes() == beforeChoice, "Selecting a tile brush edited the level.");
		const PlacementPalette::TileBrush& definition = PlacementPalette::GetTileBrushes()[brush];
		const LevelCell previous = editor.GetDocument().GetLevel().GetCells()[tile];
		click(740, 300);
		const LevelCell painted = editor.GetDocument().GetLevel().GetCells()[tile];
		Require(definition.SpawnerOnly ? painted.Spawner == definition.Spawner && painted.Terrain == previous.Terrain : painted.Terrain == definition.Terrain && painted.Spawner == previous.Spawner, "Tile sidebar selected the wrong brush or modified another layer.");
		input = {};
		input.RightClick = true;
		input.Mouse = { 740, 300 };
		send(input);
		const LevelCell erased = editor.GetDocument().GetLevel().GetCells()[tile];
		Require(definition.SpawnerOnly ? erased.Spawner == SpawnerCode::None : erased.Terrain == OccupancyCode::Empty, "Sidebar tile right-click did not erase its own layer.");
	}
	chooseGroup(static_cast<i32>(PlacementPalette::Group::Properties));
	const std::vector<SpriteDraw> withProperties = editor.Draws();
	Require(std::any_of(withProperties.begin(), withProperties.end(), [](const SpriteDraw& draw) { return draw.Sprite->FrameWidth == 320; }), "Palette properties button failed.");
	key('i');
	key('b');
	key('v');
	click(740, 39);
	Require(editor.GetSelectedCell() == LevelDocument::TileAt(740, 39), "Former placement-row canvas is still blocked.");
	click(240, 10); // Main Tiles opens the sidebar's Tiles tab.
	Require(editor.IsPaletteVisible(), "Top-level Tiles button did not reopen the sidebar.");
	Log("PASS: sidebar-only tile/spawner brushes, terrain-layer preservation, erasing and reclaimed toolbar canvas.");
	const bytestream beforeHeader = editor.GetDocument().GetBytes();
	click(tilePalette.left + PlacementPalette::GridX + 2, tilePalette.top + 5);
	const std::vector<SpriteDraw> noGrid = editor.Draws();
	Require(std::none_of(noGrid.begin(), noGrid.end(), [](const SpriteDraw& draw) { return draw.Sprite->FrameWidth == 40 && draw.Sprite->FrameHeight == 40 && draw.Sprite->Pixels.front() == 0xFF809070; }), "Palette Grid button did not hide the grid.");
	click(tilePalette.left + PlacementPalette::GridX + 2, tilePalette.top + 5);
	Require(editor.GetDocument().GetBytes() == beforeHeader, "Palette header toggles modified authored data.");
	PlacementPalette specials;
	for (const CharacterDefinition& character : GetCharacterDefinitions())
	{
		if (!editor.GetDocument().Supports(character.Kind) || (character.Category != CharacterCategory::Mermaid && character.Category != CharacterCategory::Emitter))
			continue;
		specials.Show(PlacementPalette::Group::Actors);
		input = {};
		input.Click = true;
		input.Mouse = { tilePalette.left + 8, tilePalette.top + PlacementPalette::HeaderHeight + PlacementPalette::TabHeight + 4 };
		specials.Handle(input, editor.GetDocument(), tilePalette);
		input = {};
		input.Text = character.Name;
		specials.Handle(input, editor.GetDocument(), tilePalette);
		input = {};
		input.Enter = true;
		const PlacementPalette::Result result = specials.Handle(input, editor.GetDocument(), tilePalette);
		Require(result.Selected && result.Selected->Kind == character.Kind, "Special authored object is missing from the Actors tab.");
	}
	const bytestream beforeMove = editor.GetDocument().GetBytes();
	MoveSidebar(editor, 480, 50);
	chooseGroup(static_cast<i32>(PlacementPalette::Group::Properties));
	const RECT movedSidebar = editor.GetPaletteBounds(), embedded = editor.GetInspectorBounds();
	Require(movedSidebar.left == 480 && movedSidebar.top == 50 && embedded.left == movedSidebar.left && embedded.right == movedSidebar.right && embedded.top == movedSidebar.top + PlacementPalette::HeaderHeight + PlacementPalette::TabHeight && embedded.bottom == movedSidebar.bottom, "Properties tab did not share the moved sidebar's bounds.");
	if (capture)
		engine.DrawFrame(editor.Draws(), ("editor-movable-properties-" + suffix + ".bmp").c_str());
	key('t');
	click(100, 140);
	Require(editor.GetSelectedCell() == LevelDocument::TileAt(100, 140), "Moved sidebar retained its former canvas hit region.");
	MoveSidebar(editor, -1000, -1000);
	Require(editor.GetPaletteBounds().left == 0 && editor.GetPaletteBounds().top == 26, "Sidebar escaped the top/left screen boundary.");
	MoveSidebar(editor, 1000, 1000);
	Require(editor.GetPaletteBounds().right == 800 && editor.GetPaletteBounds().bottom == 574, "Sidebar escaped the bottom/right screen boundary.");
	chooseGroup(static_cast<i32>(PlacementPalette::Group::Actors));
	Require(editor.GetPaletteBounds().left == 480 && editor.GetDocument().GetBytes() == beforeMove, "Tab switching lost sidebar position or dragging changed authored data.");
	Log("PASS: embedded Properties tab, shared draggable sidebar, viewport clamps and released canvas hit regions.");
	const str editingMusic = editor.GetMusic();
	if (engine.GetData().HasEdition(GameEdition::XXX))
		Require(editingMusic == "XXX/SvenXXX/sfx/editor.wav", "Editor did not select the user-supplied WAV in a combined install.");
	else
		Require(editingMusic == editor.GetDocument().GetInfo().Music, "Zwo-only editor lost its fallback music.");
	input = {};
	input.Playtest = true;
	send(input);
	Require(editor.IsPlaytesting() && editor.GetMusic() == editor.GetDocument().GetInfo().Music, "Playtest did not select the level's music.");
	send(input);
	Require(!editor.IsPlaytesting() && editor.GetMusic() == editingMusic, "Returning from playtest did not restore editor music.");
	if (capture && pack.Edition == GameEdition::XXX)
	{
		for (i32 repeat = 0; repeat < 6; ++repeat)
		{
			Require(engine.PlayMusic(editingMusic), "Native backend could not play editor.wav.");
			engine.Tick();
			Require(engine.PlayMusic(level.Music), "Level music failed after WAV playback.");
		}
		engine.StopMusic();
		Log("PASS: native editor WAV playback and six level/editor music transitions.");
	}
	Log("PASS: visual palette thumbnails/keyed resampling, filtering/scrolling, actor/item/scenery placement, capture/hide behavior, shared/fallback availability and editor/playtest music selection.");
}

void CheckViewportAndLayers(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, bool capture)
{
	EditorViewport viewport;
	const POINT anchor{ 400, 300 };
	viewport.ZoomAt(anchor, 6);
	Require(std::abs(viewport.WorldAt(anchor).x - 400) <= 1 && std::abs(viewport.WorldAt(anchor).y - 300) <= 1, "Zoom moved the world point under the cursor.");
	for (i32 x = 0; x < 800; ++x)
	{
		const POINT world{ x, 123 }, sample = viewport.WorldAt(viewport.ScreenAt(world));
		Require(sample.x == world.x && sample.y == world.y, "Fractional zoom lost the pixel under a projected world coordinate.");
	}
	viewport.Reset();
	viewport.ZoomAt({ 0, 0 }, 100);
	Require(viewport.GetZoom() == 4, "Viewport exceeded maximum zoom.");
	viewport.Pan(100, 100);
	Sprite sheet{ { 0, 0, 0, 0, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFF00FF }, 2, 2, 2, 2, 1, true };
	std::vector<SpriteDraw> draws{ { &sheet, 0, 0, 1, true } };
	viewport.Transform(draws);
	Require(draws.size() == 1 && draws[0].Sprite->Pixels[100 * 800 + 100] == 0xFF00FF00 && draws[0].Sprite->Pixels[100 * 800 + 104] == 0xFFFF0000 && draws[0].Sprite->Pixels[104 * 800 + 100] == 0xFF303030 && draws[0].Sprite->Pixels[104 * 800 + 104] == 0xFF0000FF, "Viewport lost frame selection, mirroring or chroma transparency.");
	draws = { { &sheet, 0, 0, 1, true, 1 } };
	viewport.Transform(draws);
	Require(draws[0].Sprite->Pixels[104 * 800 + 104] == 0xFF303030, "Viewport ignored sprite clipping.");
	viewport.ZoomAt(anchor, -100);
	Require(viewport.GetZoom() == .25f, "Viewport exceeded minimum zoom.");

	LevelEditor editor(engine, pack, level, 1, true);
	auto send = [&](WindowInput::Result input) { editor.Handle(input, 0); };
	auto key = [&](i32 binding) { WindowInput::Result input; input.BindingKey = binding; send(input); };
	auto click = [&](POINT point) { WindowInput::Result input; input.Click = true; input.Mouse = point; send(input); };
	key('b');
	WindowInput::Result input;
	input.Control = true;
	input.Wheel = 6;
	input.Mouse = anchor;
	const bytestream beforeView = editor.GetDocument().GetBytes();
	send(input);
	Require(editor.GetZoom() > 2 && editor.GetDocument().GetBytes() == beforeView, "Editor Ctrl+wheel did not zoom without editing.");
	input = {};
	input.MiddleHeld = true;
	input.Mouse = anchor;
	send(input);
	const POINT beforePan = editor.WorldAt(anchor);
	input.Mouse = { 450, 330 };
	send(input);
	input.MiddleHeld = false;
	send(input);
	Require(editor.WorldAt(anchor).x < beforePan.x && editor.WorldAt(anchor).y < beforePan.y && editor.GetDocument().GetBytes() == beforeView, "Middle-drag pan mutated the document or used world deltas.");
	key('c');
	const POINT paint{ 520, 380 }, world = editor.WorldAt(paint);
	const i32 tile = LevelDocument::TileAt(world.x, world.y);
	click(paint);
	Require(editor.GetDocument().GetLevel().GetCells()[tile].Terrain == OccupancyCode::Wall, "Zoomed tile painting used screen coordinates.");
	input = {};
	input.Undo = true;
	send(input);
	Require(editor.GetDocument().GetBytes() == beforeView, "Zoomed paint undo changed viewport or authored data.");
	if (capture)
		engine.DrawFrame(editor.Draws(), (pack.Edition == GameEdition::XXX ? "editor-zoom-pan-xxx.bmp" : "editor-zoom-pan-zwo.bmp"));
	key('0');
	Require(editor.GetZoom() == 1 && editor.WorldAt(anchor).x == anchor.x && editor.WorldAt(anchor).y == anchor.y, "Reset did not restore native canvas coordinates.");
	const i32 sheep = editor.GetDocument().Place(ObjectKind::Sheep, LevelDocument::TileAt(620, 140));
	const bytestream beforeLayers = editor.GetDocument().GetBytes();
	key('l');
	const RECT view = editor.GetInspectorBounds();
	for (i32 layer = 0; layer < static_cast<i32>(LevelEditor::Layer::Count); ++layer)
	{
		const LevelEditor::Layer kind = static_cast<LevelEditor::Layer>(layer);
		const bool previous = editor.IsLayerVisible(kind);
		click({ view.left + 20, view.top + 28 + layer * 28 + 4 });
		Require(editor.IsLayerVisible(kind) != previous && editor.GetDocument().GetBytes() == beforeLayers, "Layer toggle edited data or affected another control.");
		click({ view.left + 20, view.top + 28 + layer * 28 + 4 });
	}
	click({ view.left + 20, view.top + 28 + static_cast<i32>(LevelEditor::Layer::Animals) * 28 + 4 });
	if (capture)
		engine.DrawFrame(editor.Draws(), (pack.Edition == GameEdition::XXX ? "editor-view-layers-xxx.bmp" : "editor-view-layers-zwo.bmp"));
	key('b');
	key('v');
	const Sprite* animal = editor.GetDocument().GetSprite(ObjectKind::Sheep);
	const LevelObjectRecord& record = editor.GetDocument().GetLevel().GetObjects()[sheep];
	POINT hit{};
	bool found = false;
	for (i32 y = 0; y < animal->FrameHeight && !found; ++y)
	{
		for (i32 x = 0; x < animal->FrameWidth && !found; ++x)
		{
			if ((animal->Pixels[y * animal->Width + x] & 0xFFFFFF) != 0xFF00FF)
			{
				hit = { static_cast<i32>(record.X()) + x, static_cast<i32>(record.Y()) + y };
				found = true;
			}
		}
	}
	Require(found, "Sheep layer fixture has no opaque pixel.");
	click(hit);
	Require(editor.GetSelectedObject() != sheep, "Hidden animal remained pickable.");
	key('l');
	click({ view.left + 20, view.top + 28 + static_cast<i32>(LevelEditor::Layer::Animals) * 28 + 4 });
	key('b');
	click(hit);
	Require(editor.GetSelectedObject() == sheep && editor.GetDocument().GetBytes() == beforeLayers, "Showing animal layer lost its object or selection behavior.");
	input = {};
	input.Control = true;
	input.Wheel = 4;
	input.Mouse = hit;
	send(input);
	input = {};
	input.Click = input.MouseHeld = true;
	input.Mouse = editor.ScreenAt(hit);
	send(input);
	Require(editor.GetSelectedObject() == sheep, "Zoomed opaque pixel did not select its actor.");
	input.Click = false;
	input.Mouse.x += static_cast<i32>(std::round(40 * editor.GetZoom()));
	send(input);
	input.MouseHeld = false;
	send(input);
	Require(editor.GetDocument().GetLevel().GetObjects()[sheep].Tile() == LevelDocument::TileAt(660, 140), "Zoomed actor drag did not move exactly one world cell.");
	input = {};
	input.Undo = true;
	send(input);
	Require(editor.GetDocument().GetBytes() == beforeLayers, "Zoomed drag undo lost authored bytes.");
	key('p');
	click(editor.ScreenAt({ 660, 180 }));
	Require(editor.GetDocument().GetLevel().GetCells()[LevelDocument::TileAt(660, 180)].Terrain == OccupancyCode::Sheep, "Zoomed actor placement used screen coordinates.");
	input = {};
	input.Undo = true;
	send(input);
	const f32 zoom = editor.GetZoom();
	input = {};
	input.Playtest = true;
	send(input);
	Require(editor.IsPlaytesting(), "Zoomed editor could not enter native playtest.");
	send(input);
	Require(!editor.IsPlaytesting() && editor.GetZoom() == zoom && editor.GetDocument().GetBytes() == beforeLayers, "Playtest changed camera/layers or authored data.");
	Log("PASS: anchored zoom/limits, mirrored/keyed/clipped scaling, middle pan, inverse-coordinate painting, independent visibility layers/picking and playtest isolation.");
}

void CheckPlacementOrder(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level)
{
	std::unique_ptr<LevelDocument> document = LevelDocument::Blank(engine, pack, level);
	const POINT distantPosition{ 600, 400 }, firstPosition{ 100, 100 }, secondPosition{ 100, 99 };
	const i32 tile = LevelDocument::TileAt(100, 100);
	const i32 hole = document->Place(ObjectKind::Scenery, tile, 0, &distantPosition);
	const i32 first = document->Place(ObjectKind::Scenery, tile, 0, &firstPosition);
	document->SetObjectProperty(first, LevelDocument::ObjectProperty::Depth, -80);
	document->Delete(hole);
	const bytestream before = document->GetBytes();
	const i32 second = document->Place(ObjectKind::Scenery, tile, 0, &secondPosition, LevelDocument::SceneryOrder::Stack);
	const LevelObjectRecord& earlier = document->GetLevel().GetObjects()[first];
	const LevelObjectRecord& later = document->GetLevel().GetObjects()[second];
	Require(second < first && later.Y() + later.Height() - later.DepthAdjustment() > earlier.Y() + earlier.Height() - earlier.DepthAdjustment(), "Later overlapping scenery sorted behind earlier scenery in a recycled slot.");
	const bytestream after = document->GetBytes();
	Require(document->Undo() && document->GetBytes() == before && document->Redo() && document->GetBytes() == after, "Scenery layering was not part of the single placement undo/redo.");
	const i32 separate = document->Place(ObjectKind::Scenery, tile, 0, &distantPosition);
	Require(document->GetLevel().GetObjects()[separate].DepthAdjustment() == 0, "Nonoverlapping scenery inherited unrelated depth.");
	const str map = pack.AssetPrefix + "_editor_order/level.lvl";
	const LevelInfo preview = document->WritePreview(map);
	LevelScene scene(engine, document->GetPack(), preview, 1);
	const Sprite* art = document->GetSprite(ObjectKind::Scenery, 0);
	const std::vector<SpriteDraw>& rendered = scene.Draws();
	const auto a = std::find_if(rendered.begin(), rendered.end(), [&](const SpriteDraw& draw) { return draw.Sprite == art && draw.X == firstPosition.x && draw.Y == firstPosition.y; });
	const auto b = std::find_if(rendered.begin(), rendered.end(), [&](const SpriteDraw& draw) { return draw.Sprite == art && draw.X == secondPosition.x && draw.Y == secondPosition.y; });
	Require(a != rendered.end() && b != rendered.end() && a < b, "Saved scenery placement order did not reach gameplay rendering.");
	Log("PASS: later opaque-overlapping scenery stays above earlier scenery, recycled slots, single-step undo/redo and saved native render order.");
}

void CheckNaturalSceneryDepth(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, bool capture)
{
	using SceneryOrder = LevelDocument::SceneryOrder;
	const str suffix = pack.Edition == GameEdition::XXX ? "xxx" : "zwo";
	std::unique_ptr<LevelDocument> document = LevelDocument::Blank(engine, pack, level);
	const std::vector<TileSetInfo>& themes = document->GetThemes();
	const auto theme = std::find_if(themes.begin(), themes.end(), [](const TileSetInfo& set) { return std::any_of(set.Tiles.begin(), set.Tiles.end(), [](const TileInfo& tile) { return _stricmp(std::filesystem::u8path(tile.File).filename().u8string().c_str(), "BG_DL_grass06water.bmp") == 0; }); });
	Require(theme != themes.end(), "Grass/river depth regression requires the installed Download theme.");
	document->SetTileSet(static_cast<size_t>(std::distance(themes.begin(), theme)));
	auto texture = [&](cstr name) { const auto found = std::find_if(document->GetTileSet().Tiles.begin(), document->GetTileSet().Tiles.end(), [&](const TileInfo& tile) { return _stricmp(std::filesystem::u8path(tile.File).filename().u8string().c_str(), name) == 0; }); Require(found != document->GetTileSet().Tiles.end(), "Missing depth fixture texture."); return static_cast<i32>(std::distance(document->GetTileSet().Tiles.begin(), found)); };
	const i32 riverTexture = texture("BG_DL_river03.bmp"), grassTexture = texture("BG_DL_grass06water.bmp");
	const POINT riverPosition{ 80, 80 }, grassPosition{ 100, 100 };
	const i32 tile = LevelDocument::TileAt(100, 100);
	const i32 river = document->Place(ObjectKind::Scenery, tile, riverTexture, &riverPosition);
	const i32 grass = document->Place(ObjectKind::Scenery, tile, grassTexture, &grassPosition);
	Require(document->GetLevel().GetObjects()[grass].DepthAdjustment() == 0, "Natural grass inherited the large overlapping river's ground depth.");
	const bytestream beforeRepeats = document->GetBytes();
	for (i32 repeat = 0; repeat < 8; ++repeat)
	{
		const i32 slot = document->Place(ObjectKind::Scenery, tile, grassTexture, &grassPosition);
		Require(document->GetLevel().GetObjects()[slot].DepthAdjustment() == 0, "Repeated natural placements accumulated depth drift.");
	}
	for (i32 repeat = 0; repeat < 8; ++repeat)
		Require(document->Undo(), "Repeated natural placement lost its undo entry.");
	Require(document->GetBytes() == beforeRepeats, "Natural placement undo changed unrelated fields.");
	const i32 farmerSlot = document->Place(ObjectKind::Farmer, LevelDocument::TileAt(100, 220));
	document->SetDrawAtBottom(river, true);
	const LevelInfo preview = document->WritePreview(pack.AssetPrefix + "_editor_natural/level.lvl");
	LevelScene scene(engine, document->GetPack(), preview, 1);
	WorldObject& farmer = scene.GetObjects()[scene.GetObjects().Find(ObjectKind::Farmer).at(0)];
	const LevelObjectRecord& grassRecord = document->GetLevel().GetObjects()[grass];
	const f32 ground = grassRecord.Y() + grassRecord.Height();
	const Sprite* grassArt = document->GetSprite(grassRecord);
	for (i32 side : { -1, 1 })
	{
		farmer.X = 100;
		farmer.Y = static_cast<f32>(ground + side - farmer.DepthOffset);
		farmer.Draw.X = 100;
		farmer.Draw.Y = static_cast<i32>(farmer.Y);
		const std::vector<SpriteDraw>& draws = scene.Draws();
		const auto vegetation = std::find_if(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == grassArt; });
		const auto actor = std::find_if(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == farmer.Draw.Sprite; });
		Require(vegetation != draws.end() && actor != draws.end() && (side > 0 ? vegetation < actor : actor < vegetation), "Natural scenery did not switch in front of/behind the farmer at its actual ground line.");
		if (capture)
			engine.DrawFrame(draws, ("editor-grass-farmer-" + str(side > 0 ? "front-" : "behind-") + suffix + ".bmp").c_str());
	}
	document->SetRawObjectWord(grass, 24, 0x89ABCDEF);
	document->SetObjectProperty(grass, LevelDocument::ObjectProperty::Depth, -314);
	document->SetObjectProperty(grass, LevelDocument::ObjectProperty::Pass, 1);
	const bytestream stacked = document->GetBytes();
	document->ResetSceneryDepth(grass);
	Require(document->GetLevel().GetObjects()[grass].DepthAdjustment() == 0 && document->GetLevel().GetObjects()[grass].RenderPass() == 0 && document->GetRawObject(grass)[24] == 0x89ABCDEF, "Natural reset did not repair legacy stacking/foreground or changed unrelated words.");
	const bytestream natural = document->GetBytes();
	Require(document->Undo() && document->GetBytes() == stacked && document->Redo() && document->GetBytes() == natural && Rejects([&]() { document->ResetSceneryDepth(farmerSlot); }), "Natural reset is not one atomic undo/redo or allowed an actor.");
	document->SetDrawAtBottom(river, true);
	const i32 floor = document->Place(ObjectKind::Scenery, tile, grassTexture, &grassPosition, SceneryOrder::Floor);
	Require(document->DrawsAtBottom(floor), "Floor placement did not serialize native floor depth.");
	document->ResetSceneryDepth(floor);
	Require(!document->DrawsAtBottom(floor) && document->GetLevel().GetObjects()[floor].DepthAdjustment() == 0, "Natural reset did not leave floor mode.");

	LevelEditor editor(engine, pack, level, 1, true);
	MoveSidebar(editor, 480, 26);
	WindowInput::Result input;
	auto click = [&](i32 x, i32 y, bool right = false) { input = {}; input.Click = !right; input.RightClick = right; input.Mouse = { x, y }; editor.Handle(input, 0); };
	const RECT sidebar = editor.GetPaletteBounds();
	click(sidebar.left + 84, sidebar.top + PlacementPalette::HeaderHeight + 4); // Scenery tab.
	const bytestream untouched = editor.GetDocument().GetBytes();
	click(sidebar.left + 10, sidebar.bottom - 16); // Natural -> Stack.
	click(sidebar.left + 10, sidebar.bottom - 16); // Stack -> Floor.
	Require(editor.GetDocument().GetBytes() == untouched, "Placement-depth preference edited existing level data.");
	click(sidebar.left + 10, sidebar.top + PlacementPalette::HeaderHeight + PlacementPalette::TabHeight + PlacementPalette::SearchHeight + 10);
	click(100, 100);
	const i32 placed = editor.GetSelectedObject();
	Require(placed >= 0 && editor.GetDocument().DrawsAtBottom(placed), "Sidebar depth choice did not reach scenery placement.");
	if (capture)
		engine.DrawFrame(editor.Draws(), ("editor-scenery-depth-palette-" + suffix + ".bmp").c_str());
	click(sidebar.left + 10, sidebar.bottom - 16, true); // Floor -> Stack.
	click(sidebar.left + 10, sidebar.bottom - 16, true); // Stack -> Natural.
	click(100, 140);
	Require(editor.GetDocument().GetLevel().GetObjects()[editor.GetSelectedObject()].DepthAdjustment() == 0, "Reverse cycling did not restore natural placement.");
	input = {};
	input.BindingKey = 'i';
	editor.Handle(input, 0);
	const RECT properties = editor.GetInspectorBounds();
	click(properties.left + 90, properties.top + 4);
	editor.GetDocument().SetObjectProperty(editor.GetSelectedObject(), LevelDocument::ObjectProperty::Depth, -300);
	click(properties.left + 160, properties.top + 24 + 11 * 28 + 4);
	Require(editor.GetDocument().GetLevel().GetObjects()[editor.GetSelectedObject()].DepthAdjustment() == 0, "Object Properties Natural depth action did not repair existing scenery.");
	if (capture)
		engine.DrawFrame(editor.Draws(), ("editor-scenery-depth-properties-" + suffix + ".bmp").c_str());
	Log("PASS: natural grass/river depth without placement drift, native farmer front/behind sorting, explicit stack/floor choices, atomic repair and palette/property routing.");
}

void CheckAnimalPlacementDefaults(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, bool capture)
{
	using AnimalPlacement = LevelDocument::AnimalPlacement;
	using SceneryOrder = LevelDocument::SceneryOrder;
	const str suffix = pack.Edition == GameEdition::XXX ? "xxx" : "zwo";
	std::unique_ptr<LevelDocument> document = LevelDocument::Blank(engine, pack, level);
	const bytestream baseline = document->GetBytes();
	const i32 tile = LevelDocument::TileAt(100, 140);
	size_t families = 0;
	for (const CharacterDefinition& character : GetCharacterDefinitions())
	{
		if (character.Category != CharacterCategory::Animal || !document->Supports(character.Kind))
			continue;
		++families;
		for (i32 facing = 0; facing < 4; ++facing)
		{
			const AnimalPlacement settings{ facing, facing % 2 ? 10 : 0 };
			const i32 slot = document->Place(character.Kind, tile, 0, nullptr, SceneryOrder::Natural, &settings);
			const LevelObjectRecord& record = document->GetLevel().GetObjects()[slot];
			Require(record.Facing() == facing && record.Frame() == facing && record.Impatience() == settings.Impatience && document->GetRawCell(tile)[2] == static_cast<u32>(slot), "Animal placement lost facing, standing frame, impatience or cell ownership.");
			const bytestream authored = document->GetBytes();
			Require(document->Undo() && document->GetBytes() == baseline && document->Redo() && document->GetBytes() == authored && document->Undo(), "Configured animal placement was not a single exact undo/redo step.");
		}
	}
	Require(families >= 4, "Animal placement fixture did not cover the available animal catalog.");
	for (const AnimalPlacement settings : { AnimalPlacement{ -1, 6 }, AnimalPlacement{ 4, 6 }, AnimalPlacement{ 0, -1 }, AnimalPlacement{ 0, 11 } })
		Require(Rejects([&]() { document->Place(ObjectKind::Sheep, tile, 0, nullptr, SceneryOrder::Natural, &settings); }) && document->GetBytes() == baseline, "Invalid animal defaults partially mutated a level.");
	const AnimalPlacement settings{ 3, 7 };
	Require(Rejects([&]() { document->Place(ObjectKind::Farmer, tile, 0, nullptr, SceneryOrder::Natural, &settings); }) && document->GetBytes() == baseline && document->Redo() && document->Undo(), "Animal defaults applied to a non-animal or rejected input cleared redo history.");
	const i32 ordinary = document->Place(ObjectKind::Sheep, tile);
	Require(document->GetLevel().GetObjects()[ordinary].Facing() == 0 && document->GetLevel().GetObjects()[ordinary].Impatience() == 6 && document->Undo(), "Unconfigured placement no longer uses the existing defaults.");
	const i32 authored = document->Place(ObjectKind::Goat, tile, 0, nullptr, SceneryOrder::Natural, &settings);
	const LevelInfo preview = document->WritePreview(pack.AssetPrefix + "_editor_animal_defaults/level.lvl");
	LevelDocument reloaded(engine, document->GetPack(), preview);
	Require(reloaded.GetBytes() == document->GetBytes() && reloaded.GetLevel().GetObjects()[authored].Impatience() == 7, "Animal defaults were not preserved on disk.");
	LevelScene scene(engine, document->GetPack(), preview, 1);
	const WorldObject& goat = scene.GetObjects()[scene.GetObjects().Find(ObjectKind::Goat).at(0)];
	Require(goat.Facing == 3 && goat.Impatience == 6, "Animal defaults did not reach native gameplay initialization, including odd-impatience rounding.");

	LevelEditor editor(engine, pack, level, 1, true);
	MoveSidebar(editor, 480, 26);
	auto send = [&](WindowInput::Result input) { editor.Handle(input, 0); };
	auto click = [&](i32 x, i32 y, bool right = false) { WindowInput::Result input; input.Click = !right; input.RightClick = right; input.Mouse = { x, y }; send(input); };
	const RECT sidebar = editor.GetPaletteBounds();
	click(sidebar.left + 164, sidebar.top + PlacementPalette::HeaderHeight + 4);
	click(sidebar.left + 10, sidebar.top + PlacementPalette::HeaderHeight + PlacementPalette::TabHeight + 4);
	WindowInput::Result input;
	input.Text = "Sheep";
	send(input);
	input = {};
	input.Enter = true;
	send(input);
	const bytestream untouched = editor.GetDocument().GetBytes();
	click(sidebar.left + 10, sidebar.bottom - 16, true); // Facing 0 -> 3.
	for (i32 count = 0; count < 4; ++count)
		click(sidebar.left + 180, sidebar.bottom - 16); // Impatience 6 -> 10.
	Require(editor.GetDocument().GetBytes() == untouched, "Animal placement controls edited existing level data.");
	input = {};
	input.Mouse = { 100, 140 };
	send(input);
	const std::vector<SpriteDraw> ghost = editor.Draws();
	const Sprite* sheepArt = editor.GetDocument().GetSprite(ObjectKind::Sheep);
	Require(std::any_of(ghost.begin(), ghost.end(), [&](const SpriteDraw& draw) { return draw.Sprite == sheepArt && draw.Frame == 3; }), "Animal placement ghost did not show the chosen direction.");
	if (capture)
		engine.DrawFrame(ghost, ("editor-animal-placement-" + suffix + ".bmp").c_str());
	click(100, 140);
	const i32 first = editor.GetSelectedObject();
	Require(first >= 0 && editor.GetDocument().GetLevel().GetObjects()[first].Facing() == 3 && editor.GetDocument().GetLevel().GetObjects()[first].Impatience() == 10, "Palette animal settings did not reach placement.");
	const bytestream placed = editor.GetDocument().GetBytes();
	click(sidebar.left + 10, sidebar.bottom - 16);  // Facing 3 -> 0.
	click(sidebar.left + 180, sidebar.bottom - 16); // Impatience 10 -> 0.
	Require(editor.GetDocument().GetBytes() == placed, "Changing animal defaults rewrote an existing animal.");
	input = {};
	input.BindingKey = 'p';
	send(input);
	click(140, 140);
	const LevelObjectRecord& second = editor.GetDocument().GetLevel().GetObjects()[editor.GetSelectedObject()];
	Require(second.Facing() == 0 && second.Impatience() == 0, "Repeated placement did not use wrapped animal defaults.");
	input = {};
	input.Undo = true;
	send(input);
	Require(editor.GetDocument().GetBytes() == placed, "Configured UI placement undo also changed an earlier animal.");
	Log("PASS: all available animal placement defaults, four directions and impatience bounds, atomic history/ownership, native reload/rounding, sidebar preferences and matching ghost.");
}

void CheckLinkAnnotations(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, bool capture)
{
	Sprite raster = EditorGraphics::Rectangle(8, 8, 0xFFFF00FF, false);
	EditorGraphics::Line(raster, { 0, 0 }, { 7, 7 }, 0xFF00DDDD);
	Require(raster.Pixels[0] == 0xFF00DDDD && raster.Pixels[2 * 8 + 2] == 0xFF00DDDD && raster.Pixels[3 * 8 + 3] == 0xFFFF00FF && raster.Pixels[7 * 8 + 7] == 0xFF00DDDD, "Dashed annotation line lost its endpoints or dash gaps.");
	EditorGraphics::Line(raster, { 20, 4 }, { -20, 4 }, 0xFFFFB000, false);
	EditorGraphics::Line(raster, { 4, -20 }, { 4, 20 }, 0xFF66FF66, false);
	Require(raster.Pixels[4 * 8] == 0xFFFFB000 && raster.Pixels[4 * 8 + 7] == 0xFFFFB000 && raster.Pixels[4] == 0xFF66FF66 && raster.Pixels[7 * 8 + 4] == 0xFF66FF66, "Annotation lines did not clip safely in both directions.");
	const str suffix = pack.Edition == GameEdition::XXX ? "xxx" : "zwo";
	LevelEditor editor(engine, pack, level, 1, true);
	MoveSidebar(editor, 480, 26);
	LevelDocument& document = editor.GetDocument();
	const i32 water = LevelDocument::TileAt(100, 100), a = LevelDocument::TileAt(300, 100), b = LevelDocument::TileAt(300, 300);
	document.PaintTerrain(water, OccupancyCode::Water);
	document.PaintTerrain(a, OccupancyCode::Puddle);
	document.PaintTerrain(b, OccupancyCode::Puddle);
	document.SetTeleporters({ b, a });
	const i32 animal = document.Place(ObjectKind::Sheep, LevelDocument::TileAt(100, 300));
	document.Place(ObjectKind::Bone, LevelDocument::TileAt(180, 300));
	const bytestream baseline = document.GetBytes();
	auto send = [&](WindowInput::Result input) { editor.Handle(input, 0); };
	auto key = [&](i32 binding) { WindowInput::Result input; input.BindingKey = binding; send(input); };
	auto select = [&](POINT point) { WindowInput::Result input; input.Click = input.Shift = true; input.Mouse = editor.ScreenAt(point); send(input); };
	auto toggle = [&](LevelEditor::Layer layer) { key('l'); const RECT bounds = editor.GetInspectorBounds(); WindowInput::Result input; input.Click = true; input.Mouse = { bounds.left + 20, bounds.top + 28 + static_cast<i32>(layer) * 28 + 4 }; send(input); };
	Require(!editor.IsLayerVisible(LevelEditor::Layer::Links), "Link overlay should be opt-in.");
	toggle(LevelEditor::Layer::Links);
	select({ 100, 100 });
	const std::vector<SpriteDraw>& draws = editor.Draws();
	const auto found = std::find_if(draws.begin(), draws.end(), [](const SpriteDraw& draw) { return draw.Sprite->Keyed && draw.Sprite->Width == 800 && draw.Sprite->FrameHeight == 600 && draw.Sprite->Pixels[100 * 800 + 300] == 0xFF00DDDD; });
	Require(found != draws.end() && editor.GetSelectedCell() == water, "Selecting water did not show its destination pool.");
	const Sprite* overlay = found->Sprite;
	auto pixel = [&](i32 x, i32 y) { editor.Draws(); return overlay->Pixels[y * 800 + x]; };
	Require(pixel(300, 300) == 0xFF00DDDD && pixel(100, 300) == 0xFF66FF66 && pixel(180, 300) == 0xFFFFB000 && document.GetBytes() == baseline, "Link overlay lost a teleport/actor/item endpoint or edited authored data.");
	BitmapFont font(engine, true, false, engine.GetData().GetThemeEdition());
	auto label = [&](cstr text) { std::vector<SpriteDraw> expected; font.Append(expected, text, 282, 82); const std::vector<SpriteDraw>& commands = editor.Draws(); return std::all_of(expected.begin(), expected.end(), [&](const SpriteDraw& glyph) { return std::any_of(commands.begin(), commands.end(), [&](const SpriteDraw& draw) { return draw.X == glyph.X && draw.Y == glyph.Y && draw.Frame == glyph.Frame && draw.Sprite->Width == glyph.Sprite->Width && draw.Sprite->FrameHeight == glyph.Sprite->FrameHeight && draw.Sprite->Pixels == glyph.Sprite->Pixels; }); }); };
	Require(label("P2"), "Puddle label does not match its destination-table position.");
	if (capture)
		engine.DrawFrame(editor.Draws(), ("editor-teleport-links-" + suffix + ".bmp").c_str());
	select({ 300, 100 });
	Require(pixel(297, 97) == 0xFFFF00FF && pixel(300, 300) == 0xFF00DDDD, "Multi-destination puddle incorrectly included itself as a target.");
	document.SetTeleporters({ a });
	Require(pixel(297, 97) == 0xFF00DDDD && label("P1"), "Single-destination self teleport or live table labels were not shown.");
	document.SetTeleporters({});
	editor.Draws();
	Require(std::count(overlay->Pixels.begin(), overlay->Pixels.end(), 0xFF00DDDD) == 0 && label("P-"), "Empty pool retained stale teleport lines or claimed a registered destination.");
	document.SetTeleporters({ b });
	Require(pixel(300, 300) == 0xFF00DDDD && label("P-"), "An unregistered puddle could not show the shared outgoing pool.");
	toggle(LevelEditor::Layer::Animals);
	Require(pixel(100, 300) == 0xFFFF00FF && pixel(180, 300) == 0xFFFFB000, "Hidden animals retained links or hid unrelated item links.");
	toggle(LevelEditor::Layer::Animals);
	const bytestream beforeMove = document.GetBytes();
	document.Move(animal, 40, 0);
	Require(pixel(100, 300) == 0xFFFF00FF && pixel(140, 300) == 0xFF66FF66 && document.Undo() && document.GetBytes() == beforeMove && pixel(100, 300) == 0xFF66FF66, "Actor links did not follow movement/undo.");
	select({ 100, 100 });
	toggle(LevelEditor::Layer::Tiles);
	Require(pixel(300, 300) == 0xFF00DDDD, "Hiding tile outlines also hid the independent link layer.");
	key('b');
	WindowInput::Result input;
	input.Control = true;
	input.Wheel = 2;
	input.Mouse = { 100, 100 };
	send(input);
	const POINT projected = editor.ScreenAt({ 300, 300 });
	const std::vector<SpriteDraw>& zoomed = editor.Draws();
	Require(zoomed.front().Sprite->Pixels[projected.y * 800 + projected.x] == 0xFF00DDDD, "Viewport zoom did not transform teleport link endpoints with the level.");
	if (capture)
		engine.DrawFrame(zoomed, ("editor-teleport-links-zoom-" + suffix + ".bmp").c_str());
	key('0');
	const bytestream beforeView = document.GetBytes();
	toggle(LevelEditor::Layer::Links);
	const std::vector<SpriteDraw>& hidden = editor.Draws();
	Require(std::none_of(hidden.begin(), hidden.end(), [&](const SpriteDraw& draw) { return draw.Sprite == overlay; }) && document.GetBytes() == beforeView, "Disabled link layer still drew or changed the document.");
	toggle(LevelEditor::Layer::Links);
	input = {};
	input.Playtest = true;
	send(input);
	const std::vector<SpriteDraw>& preview = editor.Draws();
	Require(editor.IsPlaytesting() && std::none_of(preview.begin(), preview.end(), [&](const SpriteDraw& draw) { return draw.Sprite == overlay; }), "Editor links leaked into gameplay playtest.");
	send(input);
	Require(!editor.IsPlaytesting() && document.GetBytes() == beforeView, "Link visualization/playtest changed the authored document.");
	Log("PASS: read-only teleport pool labels/possible targets, self and empty-pool rules, actor/item links, movement/undo, visibility, clipped dashed lines, zoom and playtest isolation.");
}

void CheckContinuousPainting(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, bool capture)
{
	LevelEditor editor(engine, pack, level, 1, true);
	auto send = [&](WindowInput::Result input) { editor.Handle(input, 0); };
	auto key = [&](i32 binding) { WindowInput::Result input; input.BindingKey = binding; send(input); };
	auto undo = [&]() { WindowInput::Result input; input.Undo = true; send(input); };
	auto redo = [&]() { WindowInput::Result input; input.Redo = true; send(input); };
	auto brush = [&](i32 index) {
		WindowInput::Result input;
		input.Click = true;
		input.Mouse = { 240, 10 };
		send(input);
		const RECT sidebar = editor.GetPaletteBounds();
		input.Mouse = { sidebar.left + index % PlacementPalette::Columns * PlacementPalette::CellWidth + 10, sidebar.top + PlacementPalette::HeaderHeight + PlacementPalette::TabHeight + PlacementPalette::SearchHeight + index / PlacementPalette::Columns * PlacementPalette::CellHeight + 10 };
		send(input);
		key('b');
	};
	auto drag = [&](POINT start, POINT end, bool erase = false) {
		WindowInput::Result input;
		input.Mouse = editor.ScreenAt(start);
		input.Click = input.MouseHeld = !erase;
		input.RightClick = input.RightHeld = erase;
		send(input);
		input.Click = input.RightClick = false;
		input.Mouse = editor.ScreenAt(end);
		send(input);
		input.MouseHeld = input.RightHeld = false;
		send(input);
	};
	LevelDocument& document = editor.GetDocument();
	document.Place(ObjectKind::Sheep, LevelDocument::TileAt(300, 100));
	document.Place(ObjectKind::Bone, LevelDocument::TileAt(380, 100));
	const bytestream before = document.GetBytes();
	brush(1);
	drag({ 100, 100 }, { 500, 100 });
	for (i32 x = 100; x <= 500; x += 40)
	{
		const OccupancyCode terrain = document.GetLevel().GetCells()[LevelDocument::TileAt(x, 100)].Terrain;
		Require(x == 300 ? terrain == OccupancyCode::Sheep : (x == 380 ? terrain == OccupancyCode::Empty : terrain == OccupancyCode::Wall), "Continuous stroke missed a gap or overwrote a linked actor/item.");
	}
	const bytestream painted = document.GetBytes();
	undo();
	Require(document.GetBytes() == before, "Undo did not remove the entire sparse-sample stroke.");
	redo();
	Require(document.GetBytes() == painted, "Redo did not restore the exact stroke bytes.");
	drag({ 100, 100 }, { 500, 100 }, true);
	Require(document.GetBytes() == before, "Right-drag did not erase only terrain across the whole stroke.");
	undo();
	Require(document.GetBytes() == painted, "Erase stroke was not a single undo entry.");
	undo();
	Require(document.GetBytes() == before, "Painting and erasing strokes were merged together.");
	brush(0);
	drag({ 620, 500 }, { 660, 500 });
	redo();
	Require(document.GetBytes() == painted, "A no-op stroke destroyed redo history.");
	undo();
	brush(3);
	drag({ 100, 180 }, { 300, 380 });
	for (i32 step = 0; step <= 5; ++step)
		Require(document.GetLevel().GetCells()[LevelDocument::TileAt(100 + step * 40, 180 + step * 40)].Terrain == OccupancyCode::Water, "Diagonal stroke left a gap.");
	undo();
	Require(document.GetBytes() == before, "Diagonal stroke undo changed unrelated cells.");

	brush(1);
	key('b'); // Show the sidebar while a canvas stroke owns its gesture.
	WindowInput::Result input;
	input.Click = input.MouseHeld = true;
	input.Mouse = { 500, 220 };
	send(input);
	input.Click = false;
	input.Mouse = { 100, 220 };
	send(input);
	input.Mouse = { 500, 380 };
	send(input);
	input.Mouse = { 350, 10 }; // Crossing Undo must not click it.
	send(input);
	input.MouseHeld = false;
	send(input);
	Require(document.GetLevel().GetCells()[LevelDocument::TileAt(500, 220)].Terrain == OccupancyCode::Wall && document.GetLevel().GetCells()[LevelDocument::TileAt(500, 380)].Terrain == OccupancyCode::Wall && document.GetLevel().GetCells()[LevelDocument::TileAt(500, 300)].Terrain == OccupancyCode::Empty && document.GetLevel().GetCells()[LevelDocument::TileAt(100, 220)].Terrain == OccupancyCode::Empty, "Stroke painted behind UI, bridged a suspended segment or activated Undo.");
	undo();
	Require(document.GetBytes() == before, "UI-suspended stroke split its history group.");
	key('b');
	input = {};
	input.Control = true;
	input.Wheel = 4;
	input.Mouse = { 400, 300 };
	send(input);
	input = {};
	input.MiddleHeld = true;
	input.Mouse = { 400, 300 };
	send(input);
	input.Mouse = { 450, 330 };
	send(input);
	input.MiddleHeld = false;
	send(input);
	drag({ 340, 220 }, { 500, 220 });
	for (i32 x = 340; x <= 500; x += 40)
		Require(document.GetLevel().GetCells()[LevelDocument::TileAt(x, 220)].Terrain == OccupancyCode::Wall, "Zoom/pan stroke painted in screen coordinates.");
	if (capture)
		engine.DrawFrame(editor.Draws(), pack.Edition == GameEdition::XXX ? "editor-continuous-stroke-xxx.bmp" : "editor-continuous-stroke-zwo.bmp");
	undo();
	Require(document.GetBytes() == before, "Zoomed stroke undo lost bytes.");
	key('0');
	for (i32 index : { 2, 4, 5, 6 })
	{
		brush(index);
		drag({ 20, 60 }, { 780, 60 });
		i32 count = 0;
		for (i32 x = 20; x < 800; x += 40)
		{
			const LevelCell& cell = document.GetLevel().GetCells()[LevelDocument::TileAt(x, 60)];
			count += index == 2 ? cell.Terrain == OccupancyCode::Puddle : cell.Spawner == PlacementPalette::GetTileBrushes()[index].Spawner;
		}
		Require(count == 16 && (index != 2 || document.GetLevel().GetPuddles().size() == 16), "Continuous brush broke the native 16-marker cap or puddle table.");
		undo();
		Require(document.GetBytes() == before, "Limited marker stroke could not be undone in one step.");
	}
	brush(1);
	input = {};
	input.Click = input.MouseHeld = true;
	input.Mouse = { 100, 460 };
	send(input);
	input.Click = false;
	input.Mouse = { 260, 460 };
	send(input);
	editor.FinishPaintStroke(); // GameApp uses this on focus loss.
	undo();
	Require(document.GetBytes() == before, "Focus-loss stroke boundary did not commit a single undo step.");
	input = {};
	input.Click = input.MouseHeld = true;
	input.Mouse = { 100, 420 };
	send(input);
	input.Click = false;
	input.Mouse = { 260, 420 };
	send(input);
	input.Playtest = true;
	send(input);
	const bytestream playtestBytes = document.GetBytes();
	Require(editor.IsPlaytesting(), "Held stroke did not finish before F5 playtest.");
	input = {};
	input.Playtest = true;
	send(input);
	Require(document.GetBytes() == playtestBytes, "Playtest rewrote the committed stroke.");
	undo();
	Require(document.GetBytes() == before, "Playtest split the stroke's undo group.");
	Log("PASS: continuous paint/right-erase, sparse and diagonal interpolation, grouped undo/redo, no-op redo retention, protected actors/items, UI suspension, zoom/pan, all marker limits and focus/playtest boundaries.");
}

void CheckSceneryWorkflow(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, bool capture)
{
	LevelEditor editor(engine, pack, level, 1, true);
	auto send = [&](WindowInput::Result input) { return editor.Handle(input, 0); };
	auto key = [&](i32 binding) { WindowInput::Result input; input.BindingKey = binding; return send(input); };
	auto click = [&](POINT point, bool right = false) { WindowInput::Result input; input.Mouse = point; input.Click = !right; input.RightClick = right; return send(input); };
	auto count = [&]() { return std::count_if(editor.GetDocument().GetLevel().GetObjects().begin(), editor.GetDocument().GetLevel().GetObjects().end(), [](const LevelObjectRecord& record) { return record.Kind() == static_cast<i32>(ObjectKind::Scenery); }); };
	WindowInput::Result input;
	input.Click = true;
	input.Mouse = { 140, 10 };
	send(input);
	const RECT sidebar = editor.GetPaletteBounds();
	click({ sidebar.left + PlacementPalette::Width / PlacementPalette::TabsPerRow + 4, sidebar.top + PlacementPalette::HeaderHeight + 4 });
	click({ sidebar.left + 10, sidebar.top + PlacementPalette::HeaderHeight + PlacementPalette::TabHeight + PlacementPalette::SearchHeight + 10 });
	key('b');
	key('n');
	key('a');
	click({ 100, 100 });
	const i32 first = editor.GetSelectedObject();
	click({ 400, 100 });
	const i32 second = editor.GetSelectedObject();
	click({ 650, 400 });
	Require(count() == 3 && first != second, "Scenery clicks did not place three independent objects.");
	LevelDocument& document = editor.GetDocument();
	auto opaquePoint = [&](i32 slot) {
		const LevelObjectRecord& record = document.GetLevel().GetObjects()[slot];
		const Sprite* art = document.GetSprite(record);
		const i32 sx = record.Frame() % art->Columns * art->FrameWidth, sy = record.Frame() / art->Columns * art->FrameHeight;
		for (i32 y = 0; y < art->FrameHeight; ++y)
		{
			for (i32 x = 0; x < art->FrameWidth; ++x)
			{
				const POINT point{ static_cast<i32>(record.X()) + x, static_cast<i32>(record.Y()) + y };
				if (point.x >= 0 && point.x < 800 && point.y >= 26 && point.y < 574 && (!art->Keyed || (art->Pixels[(sy + y) * art->Width + sx + x] & 0xFFFFFF) != 0xFF00FF))
					return point;
			}
		}
		throw std::runtime_error("No visible scenery hit pixel in workflow fixture.");
	};
	const bytestream placed = document.GetBytes();
	click(opaquePoint(first), true);
	Require(count() == 2 && document.GetLevel().GetObjects()[first].Kind() == static_cast<i32>(ObjectKind::Inactive), "Scenery right-click did not delete the opaque hit.");
	input = {};
	input.Undo = true;
	send(input);
	Require(document.GetBytes() == placed, "Right-click deletion was not independently undoable.");
	input = {};
	input.Back = true;
	Require(!send(input) && editor.GetSelectedObject() == -1 && document.GetBytes() == placed, "Escape did not leave placement without editing/closing.");
	click({ 740, 550 });
	Require(count() == 3, "Escape left the scenery placement tool active.");
	key('v');
	click(opaquePoint(first));
	Require(editor.GetSelectedObject() == first, "Selection mode did not pick scenery after Escape.");
	input = {};
	input.Back = true;
	Require(!send(input) && editor.GetSelectedObject() == -1, "Escape did not clear the selected object.");
	click(opaquePoint(first));
	key('i');
	RECT properties = editor.GetInspectorBounds();
	click({ properties.left + 90, properties.top + 4 });
	properties = editor.GetInspectorBounds();
	// Object subtab: five common fields, render pass, then Draw at bottom.
	click({ properties.left + 180, properties.top + 24 + 6 * 28 + 4 });
	Require(document.DrawsAtBottom(first) && document.GetLevel().GetObjects()[first].RenderPass() == 0, "Properties did not enable native floor depth.");
	const bytestream floor = document.GetBytes();
	Require(Rejects([&]() { document.SetDrawAtBottom(0, true); }) && Rejects([&]() { document.Reorder(first, true); }) && document.GetBytes() == floor, "Floor mode allowed an actor or silently changed its depth.");
	if (capture)
		engine.DrawFrame(editor.Draws(), pack.Edition == GameEdition::XXX ? "editor-floor-properties-xxx.bmp" : "editor-floor-properties-zwo.bmp");
	input = {};
	input.Undo = true;
	send(input);
	Require(document.GetBytes() == placed, "Floor checkbox was not one undo step.");
	input = {};
	input.Redo = true;
	send(input);
	Require(document.GetBytes() == floor && document.DrawsAtBottom(first), "Floor checkbox redo lost its serialized state.");
	document.Move(first, 0, 40, false);
	if (document.GetTileSet().Tiles.size() > 1)
		document.SetObjectProperty(first, LevelDocument::ObjectProperty::Texture, 1);
	Require(document.DrawsAtBottom(first), "Moving/resizing scenery lost floor mode.");
	const LevelInfo preview = document.WritePreview(pack.AssetPrefix + "_editor_floor/level.lvl");
	LevelDocument roundtrip(engine, document.GetPack(), preview);
	Require(roundtrip.DrawsAtBottom(first), "Reloaded level lost floor depth.");
	LevelScene scene(engine, document.GetPack(), preview, 1);
	for (i32 frame = 0; frame < 3; ++frame)
	{
		scene.Update(.1f, 0, frame - 1, false);
		const LevelObjectRecord& record = document.GetLevel().GetObjects()[first];
		const Sprite* art = document.GetSprite(record);
		const std::vector<SpriteDraw>& draws = scene.Draws();
		const auto ground = std::find_if(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == art && draw.X == static_cast<i32>(record.X()) && draw.Y == static_cast<i32>(record.Y()); });
		Require(ground != draws.end(), "Native game omitted floor scenery.");
		for (const WorldObject& actor : scene.GetObjects().CopyStates())
		{
			if (actor.Kind == ObjectKind::Scenery || !actor.Draw.Sprite)
				continue;
			const auto visible = std::find_if(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == actor.Draw.Sprite; });
			Require(visible != draws.end() && ground < visible, "Floor scenery rendered over an actor during gameplay.");
		}
	}
	if (capture)
		engine.DrawFrame(scene.Draws(), pack.Edition == GameEdition::XXX ? "editor-floor-playtest-xxx.bmp" : "editor-floor-playtest-zwo.bmp");
	document.SetDrawAtBottom(first, false);
	Require(!document.DrawsAtBottom(first) && document.GetLevel().GetObjects()[first].DepthAdjustment() == 0, "Disabling floor did not restore natural world depth.");
	document.SetObjectProperty(first, LevelDocument::ObjectProperty::Pass, 1);
	document.SetDrawAtBottom(first, true);
	Require(document.DrawsAtBottom(first) && document.GetLevel().GetObjects()[first].RenderPass() == 0, "Floor mode did not clear foreground.");
	document.SetObjectProperty(first, LevelDocument::ObjectProperty::Pass, 1);
	Require(!document.DrawsAtBottom(first) && document.GetLevel().GetObjects()[first].DepthAdjustment() == 0, "Changing render pass retained conflicting floor depth.");
	Log("PASS: persistent scenery clicks, opaque right-click deletion/undo, Escape selection behavior and saved native floor depth beneath actors, including movement/texture/pass changes.");
}

void CheckEditorMenu(MudGE& engine, bool capture)
{
	const bool xxx = engine.GetData().GetThemeEdition() == GameEdition::XXX;
	const fpath saves = engine.GetData().GetRoot().parent_path() / "editor-menu-saves";
	GameUi menu(engine, saves);
	WindowInput::Result input;
	input.Mouse = { xxx ? 730 : 680, 510 };
	input.MouseMoved = true;
	Require(menu.Handle(input) == UiAction::None, "Hovering Editor activated it.");
	const Sprite* buttons = engine.LoadSprite(engine.GetData().ThemeAsset("Sven2/gfx/mainmenu_buttons.bmp"), xxx ? 12 : 10, 1);
	const std::vector<SpriteDraw>& draws = menu.Draws(0);
	Require(std::any_of(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == buttons && draw.Frame == (xxx ? 10 : 8) && draw.Y == 500; }), "Editor button did not use its existing active asset frame.");
	if (capture)
		engine.DrawFrame(draws, xxx ? "editor-menu-xxx.bmp" : "editor-menu-zwo.bmp");
	input.Click = true;
	Require(menu.Handle(input) == UiAction::Editor && menu.GetPage() == GameUi::Page::Title, "Editor menu click did not request editor launch.");
	menu.Show(GameUi::Page::GetPacks);
	Require(menu.Handle(input) == UiAction::None && menu.GetPage() == GameUi::Page::GetPacks, "Editor button remained active on a submenu.");
	GameUi keyboard(engine, saves);
	input = {};
	input.Tab = true;
	for (i32 step = 0; step < (xxx ? 5 : 4); ++step)
		keyboard.Handle(input);
	input = {};
	input.Confirm = true;
	Require(keyboard.Handle(input) == UiAction::Editor, "Keyboard navigation could not activate Editor.");
	Log("PASS: title Editor asset frame, mouse/keyboard launch action and submenu isolation.");
}

void CheckRawInspection(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, bool capture)
{
	std::unique_ptr<LevelDocument> raw = LevelDocument::Blank(engine, pack, level);
	const bytestream original = raw->GetBytes();
	Require(raw->GetRawObject(255).size() == 25 && raw->GetRawCell(373).size() == 6, "Raw inspector omitted inactive/border records.");
	raw->SetRawObjectWord(255, 24, 0xDEADBEEF);
	Require(raw->GetRawObject(255)[24] == 0xDEADBEEF && raw->Undo() && raw->GetBytes() == original && raw->Redo(), "Raw object bits/undo/redo did not preserve exact bytes.");
	const bytestream edited = raw->GetBytes();
	RawInspector keys;
	keys.Sync(*raw, 0, 23);
	WindowInput::Result panelKey;
	panelKey.Mouse = { 20, 60 };
	panelKey.Right = true;
	Require(keys.Handle(panelKey, *raw, { 0, 0, 320, 428 }), "Raw paging key leaked to world movement.");
	panelKey.Right = false;
	panelKey.Delete = true;
	Require(keys.Handle(panelKey, *raw, { 0, 0, 320, 428 }) && raw->GetBytes() == edited, "Raw panel Delete leaked to the world selection.");

	Require(Rejects([&]() { raw->SetRawObjectWord(0, 11, static_cast<u32>(ObjectKind::Goat)); }) && Rejects([&]() { raw->SetRawObjectWord(0, 20, 23); }) && Rejects([&]() { raw->SetRawObjectWord(0, 0, 0x7FC00000); }) && Rejects([&]() { raw->SetRawObjectWord(0, 5, 0); }) && Rejects([&]() { raw->SetRawObjectWord(0, 23, 0x80000000); }) && raw->GetBytes() == edited, "Invalid raw object edits changed data.");
	const i32 oldTile = raw->GetLevel().GetObjects()[0].Tile();
	const f32 x = raw->GetLevel().GetObjects()[0].X() + 80;
	u32 bits;
	std::memcpy(&bits, &x, 4);
	raw->SetRawObjectWord(0, 0, bits);
	Require(raw->GetLevel().GetObjects()[0].Tile() == oldTile + 2 && raw->GetLevel().GetCells()[oldTile].Terrain == OccupancyCode::Empty && raw->GetLevel().GetCells()[oldTile + 2].Terrain == OccupancyCode::Player, "Raw actor movement did not repair tile ownership.");
	raw->Undo();
	Require(raw->GetBytes() == edited, "Raw actor movement was not atomic.");
	const size_t tile = LevelDocument::TileAt(100, 100);
	const u32 direction = raw->GetRawCell(tile)[1] & 255;
	raw->SetRawCellWord(tile, 1, 0xAABBCC00 | direction);
	Require(raw->GetRawCell(tile)[1] == (0xAABBCC00 | direction) && raw->GetLevel().GetCells()[tile].Direction == direction, "Raw packed cell bits changed the direction byte.");
	raw->SetRawCellWord(tile, 5, (7u << 16) | static_cast<u32>(SpawnerCode::Distraction));
	Require(raw->GetLevel().GetCells()[tile].Spawner == SpawnerCode::Distraction && raw->GetRawCell(tile)[5] >> 16 == 7, "Raw spawner/help write lost packed bytes.");
	const bytestream cells = raw->GetBytes();
	Require(Rejects([&]() { raw->SetRawCellWord(tile, 2, 20); }) && Rejects([&]() { raw->SetRawCellWord(tile, 4, 20); }) && Rejects([&]() { raw->SetRawCellWord(tile, 3, static_cast<u32>(ItemCode::Bone)); }) && Rejects([&]() { raw->SetRawCellWord(tile, 0, static_cast<u32>(OccupancyCode::Wall)); }) && raw->GetBytes() == cells, "Raw ownership/terrain validation corrupted a cell.");
	Require(Rejects([&]() { raw->GetRawCell(374); }) && Rejects([&]() { raw->GetRawObject(256); }) && Rejects([&]() { raw->SetRawCellWord(0, 6, 0); }) && Rejects([&]() { raw->SetRawObjectWord(0, 25, 0); }), "Raw record/word bounds were not checked.");
	for (i32 column = 0; column < 16; ++column)
		raw->PaintTerrain(LevelDocument::TileAt(column * 40 + 20, 60), OccupancyCode::Puddle);
	const bytestream capped = raw->GetBytes();
	Require(Rejects([&]() { raw->SetRawCellWord(LevelDocument::TileAt(660, 60), 0, static_cast<u32>(OccupancyCode::Puddle)); }) && raw->GetBytes() == capped, "Raw terrain bypassed the native puddle cap.");

	for (i32 column = 0; column < 15; ++column)
		raw->SetSpawner(LevelDocument::TileAt(column * 40 + 20, 140), SpawnerCode::Distraction);
	const bytestream spawners = raw->GetBytes();
	Require(Rejects([&]() { raw->SetRawCellWord(LevelDocument::TileAt(660, 140), 5, static_cast<u32>(SpawnerCode::Distraction)); }) && raw->GetBytes() == spawners, "Raw spawner word bypassed the native marker cap.");
	LevelEditor editor(engine, pack, level, 1, true);
	const i32 scenery = editor.GetDocument().Place(ObjectKind::Scenery, LevelDocument::TileAt(100, 100));
	const Sprite* sceneryArt = editor.GetDocument().GetSprite(ObjectKind::Scenery, 0);
	Require(Rejects([&]() { editor.GetDocument().SetRawObjectWord(scenery, 12, sceneryArt->Rows * sceneryArt->Columns); }) && Rejects([&]() { editor.GetDocument().SetRawObjectWord(scenery, 17, static_cast<u32>(editor.GetDocument().GetTileSet().Tiles.size())); }), "Raw scenery accepted an invalid sprite/frame.");
	auto send = [&](WindowInput::Result input) { return editor.Handle(input, 0); };
	auto click = [&](POINT point) { WindowInput::Result input; input.Click = true; input.Mouse = point; send(input); };
	auto text = [&](const str& value) { WindowInput::Result input; input.Text = value; send(input); input = {}; input.Enter = true; send(input); };
	WindowInput::Result placing;
	placing.BindingKey = 'p';
	send(placing);
	const RECT sidebar = editor.GetPaletteBounds();
	click({ sidebar.left + 3 * (PlacementPalette::Width / PlacementPalette::TabsPerRow) + 4, sidebar.top + PlacementPalette::HeaderHeight + PlacementPalette::TabRowHeight + 4 });
	RECT bounds = editor.GetInspectorBounds();
	click({ bounds.left + 40, bounds.top + 28 });
	text(std::to_string(scenery));
	WindowInput::Result input;
	input.Mouse = { bounds.left + 20, bounds.top + 60 };
	input.Wheel = -20;
	send(input);
	click({ bounds.left + 20, bounds.top + 56 + 9 * 24 + 4 });
	const bytestream beforeValue = editor.GetDocument().GetBytes();
	text("0x00000005");
	Require(editor.GetDocument().GetRawObject(scenery)[24] == 5, "Raw sidebar did not edit a paged object word from hex.");
	if (capture)
		engine.DrawFrame(editor.Draws(), pack.Edition == GameEdition::XXX ? "editor-raw-object-xxx.bmp" : "editor-raw-object-zwo.bmp");
	input = {};
	input.Undo = true;
	send(input);
	Require(editor.GetDocument().GetBytes() == beforeValue, "Raw UI edit did not use ordinary undo.");
	click({ bounds.left + 180, bounds.top + 4 });
	click({ bounds.left + 40, bounds.top + 28 });
	text("373");
	const u32 border = editor.GetDocument().GetRawCell(373)[1];
	click({ bounds.left + 20, bounds.top + 56 + 24 + 4 });
	text(std::to_string((border & 255) | 0x00120000));
	Require(editor.GetDocument().GetRawCell(373)[1] == ((border & 255) | 0x00120000), "Raw border cell inspection/edit did not use the requested index.");
	const bytestream valid = editor.GetDocument().GetBytes();
	click({ bounds.left + 20, bounds.bottom - 52 });
	text("0x100000000");
	Require(editor.GetDocument().GetBytes() == valid, "Raw UI accepted a value wider than 32 bits.");
	input = {};
	input.Back = true;
	Require(!send(input) && editor.GetDocument().GetBytes() == valid, "Raw invalid-edit Escape closed the editor or changed bytes.");
	MoveSidebar(editor, 480, 50);
	if (capture)
		engine.DrawFrame(editor.Draws(), pack.Edition == GameEdition::XXX ? "editor-raw-cell-xxx.bmp" : "editor-raw-cell-zwo.bmp");
	click({ 100, 100 });
	Require(editor.GetSelectedCell() == LevelDocument::TileAt(100, 100) && editor.GetSelectedObject() == -1 && editor.GetDocument().GetBytes() == valid, "Raw cell selection placed an actor or selected scenery over the cell.");
	const LevelInfo exported = editor.GetDocument().WritePreview(pack.AssetPrefix + "_editor_raw/level.lvl");
	LevelDocument reloaded(engine, editor.GetDocument().GetPack(), exported);
	Require(reloaded.GetRawCell(373) == editor.GetDocument().GetRawCell(373), "Raw unknown bits were lost on save/reload.");
	LevelEditor clean(engine, pack, level, 1);
	const RECT cleanSidebar = clean.GetPaletteBounds();
	input = {};
	input.Click = true;
	input.Mouse = { cleanSidebar.left + 3 * (PlacementPalette::Width / PlacementPalette::TabsPerRow) + 4, cleanSidebar.top + PlacementPalette::HeaderHeight + PlacementPalette::TabRowHeight + 4 };
	clean.Handle(input, 0);
	const RECT cleanRaw = clean.GetInspectorBounds();
	input.Mouse = { cleanRaw.left + 20, cleanRaw.top + 60 };
	clean.Handle(input, 0);
	input = {};
	input.Text = "0x3F800000";
	clean.Handle(input, 0);
	Require(!clean.GetDocument().IsDirty() && !clean.RequestWindowClose(), "Window close ignored a pending raw edit on an otherwise clean document.");
	input = {};
	input.Back = true;
	clean.Handle(input, 0);
	Require(!clean.GetDocument().IsDirty() && clean.RequestWindowClose(), "Cancelling a pending raw edit left a false dirty-close guard.");
	Log("PASS: raw 25-word objects/all 374 six-word cells, exact bit edits, atomic undo, linked movement, ownership/range/cap protection and sidebar index/hex/paging/focus/save behavior.");
}

void CheckStandaloneExports(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, bool capture)
{
	LevelDocument document(engine, pack, level);
	const bytestream source = engine.ReadAsset(level.Map);
	document.SetTimeLimit(321.25f);
	document.SetRawObjectWord(255, 24, 0x89ABCDEF);
	const u32 oldBorder = document.GetRawCell(373)[1];
	document.SetRawCellWord(373, 1, (oldBorder & 255) | 0x12340000);
	const bytestream expected = document.GetBytes();
	const fpath directory = engine.GetData().GetRoot().parent_path() / "standalone-exports" / (pack.Edition == GameEdition::XXX ? "xxx" : "zwo");
	std::filesystem::create_directories(directory);
	const fpath binary = directory / L"Renamed level \u00e4.LVL", objects = directory / L"Renamed objects.CSV", cells = directory / L"Renamed cells.csv";
	auto read = [&](const fpath& path) { std::ifstream file(path, std::ios::binary); Require(static_cast<bool>(file), "Export file is missing."); return bytestream(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()); };
	fpath oldTemporary = binary;
	oldTemporary += L".tmp";
	const bytestream canary{ 'k', 'e', 'e', 'p' };
	WriteFixture(oldTemporary, canary);
	document.ExportLevel(binary);
	Require(read(binary) == expected && read(oldTemporary) == canary && document.GetBytes() == expected && document.IsDirty(), "Renamed standalone export changed bytes/savepoint or clobbered an unrelated temporary file.");
	const LevelFile parsed(read(binary), binary.u8string());
	Require(parsed.GetVersion() == document.GetLevel().GetVersion() && parsed.GetTimeLimit() == 321.25f, "Standalone export changed version or timer.");
	LevelProject imported(engine, pack, level);
	imported.Import(binary);
	Require(imported.Current().GetBytes() == expected, "Standalone level did not roundtrip through project import.");
	for (LevelDocument::InventoryKind kind : { LevelDocument::InventoryKind::Objects, LevelDocument::InventoryKind::Cells })
	{
		const bool objectRows = kind == LevelDocument::InventoryKind::Objects;
		const fpath path = objectRows ? objects : cells;
		const str csv = document.InventoryCsv(kind);
		document.ExportInventory(path, kind);
		const bytestream written = read(path);
		Require(str(written.begin(), written.end()) == csv, "CSV file differs from its inventory data.");
		std::istringstream lines(csv);
		str line;
		Require(static_cast<bool>(std::getline(lines, line)), "CSV header is missing.");
		size_t count = 0, visible = 0;
		while (std::getline(lines, line))
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			std::vector<str> columns;
			std::istringstream row(line);
			str column;
			while (std::getline(row, column, ','))
				columns.push_back(column);
			const size_t prefix = objectRows ? 8 : 4;
			std::vector<u32> words;
			if (objectRows)
			{
				const std::array<u32, 25> raw = document.GetRawObject(count);
				words.assign(raw.begin(), raw.end());
			}
			else
			{
				const std::array<u32, 6> raw = document.GetRawCell(count);
				words.assign(raw.begin(), raw.end());
				visible += std::stoi(columns.at(3));
			}
			Require(columns.size() == prefix + words.size() && std::stoul(columns[0]) == count, "CSV column count or record ordering is wrong.");
			for (size_t word = 0; word < words.size(); ++word)
				Require(static_cast<u32>(std::stoll(columns[prefix + word])) == words[word], "CSV lost a raw word's signed bits.");
			++count;
		}
		Require(count == (objectRows ? LevelFile::ObjectCount : LevelFile::CellCount) && (objectRows || visible == 300), "CSV omitted inactive objects or border cells.");
	}
	Require(document.GetBytes() == expected && document.IsDirty() && document.Undo() && document.GetRawCell(373)[1] == oldBorder && document.Redo() && document.GetBytes() == expected, "Exports entered undo history or cleared the dirty state.");
	const fpath protectedLevel = engine.GetData().AssetPath(level.Map);
	wstr upper = protectedLevel.native();
	for (wchar_t& c : upper)
	{
		if (c >= L'a' && c <= L'z')
			c -= L'a' - L'A';
	}
	Require(Rejects([&]() { document.ExportLevel(protectedLevel); }) && Rejects([&]() { document.ExportLevel(fpath(upper)); }) && Rejects([&]() { document.ExportInventory(engine.GetData().GetRoot() / "blocked.csv", LevelDocument::InventoryKind::Cells); }) && engine.ReadAsset(level.Map) == source, "Export protection allowed installed assets to be replaced.");
	if (protectedLevel.native().size() >= 3 && protectedLevel.native()[1] == L':')
		Require(Rejects([&]() { document.ExportLevel(fpath(L"\\\\?\\" + protectedLevel.native())); }) && engine.ReadAsset(level.Map) == source, "Export bypassed installed assets through a Windows namespace alias.");
	Require(Rejects([&]() { document.ExportLevel(directory / "wrong.csv"); }) && Rejects([&]() { document.ExportInventory(directory / "wrong.lvl", LevelDocument::InventoryKind::Objects); }), "Export accepted the wrong file extension.");
	const DWORD attributes = GetFileAttributesW(binary.c_str());
	Require(SetFileAttributesW(binary.c_str(), attributes | FILE_ATTRIBUTE_READONLY) != 0, "Cannot protect standalone export for failed-write regression.");
	document.SetTimeLimit(99);
	const bool failed = Rejects([&]() { document.ExportLevel(binary); });
	SetFileAttributesW(binary.c_str(), attributes);
	Require(failed && read(binary) == expected && document.GetLevel().GetTimeLimit() == 99, "Failed replacement lost the previous file or dirty edits.");
	document.Undo();
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory))
		Require(entry.path().filename().native().find(binary.filename().native() + L".tmp-") != 0, "Export left its reserved temporary file behind.");
	std::error_code error;
	const fpath link = directory / "source-link";
	std::filesystem::create_directory_symlink(engine.GetData().GetRoot(), link, error);
	if (!error)
		Require(Rejects([&]() { document.ExportLevel(link / "blocked.lvl"); }), "Export followed a directory link into installed assets.");
	const fpath hardlink = directory / "source-alias.lvl";
	error.clear();
	std::filesystem::create_hard_link(protectedLevel, hardlink, error);
	if (!error)
	{
		document.ExportLevel(hardlink);
		Require(engine.ReadAsset(level.Map) == source && read(hardlink) == expected, "Atomic export through an alias modified the installed source file.");
	}
	LevelEditor editor(engine, pack, level, 1);
	WindowInput::Result input;
	input.BindingKey = 'i';
	editor.Handle(input, 0);
	const RECT bounds = editor.GetInspectorBounds();
	input = {};
	input.Click = true;
	input.Mouse = { bounds.left + 250, bounds.top + 4 };
	editor.Handle(input, 0);
	if (capture)
		engine.DrawFrame(editor.Draws(), pack.Edition == GameEdition::XXX ? "editor-export-project-xxx.bmp" : "editor-export-project-zwo.bmp");
	Log("PASS: renamed Unicode standalone level export/import, exact CSV object/cell inventories, source/case/link protection, atomic failed-write preservation, temporary cleanup and unchanged history/savepoints.");
}

} // namespace

void CheckIntegratedEditor(MudGE& engine, const LevelPackInfo& pack, LevelInfo level, bool capture)
{
	const str suffix = pack.Edition == GameEdition::XXX ? "xxx" : "zwo";
	level.Name = "Editor & \"format\" <test>";
	level.TileSet = pack.TileSets.front().Name;
	level.Music = pack.LevelSets.front().Levels.front().Music;
	bytestream source = engine.ReadAsset(level.Map);
	u32 nameLength;
	std::memcpy(&nameLength, source.data() + 12, 4);
	const size_t grid = 16 + nameLength;
	const i32 tile = LevelDocument::TileAt(100, 140);
	Put(source, 4, u32{ 0x12345678 });
	Put(source, grid + tile * 24 + 5, u8{ 0xA5 });
	Put(source, grid + tile * 24 + 13, u8{ 0xB6 });
	if (pack.Edition == GameEdition::XXX)
		Put(source, grid + tile * 24 + 20, u32{ 3u << 16 });
	WriteFixture(engine.GetData().AssetPath(level.Map), source);
	LevelProject project(engine, pack, level);
	LevelDocument& document = project.Current();
	CheckAuthoring(engine, pack, level, grid, capture);
	Require(!document.IsDirty() && document.GetBytes() == source, "Opening a level changed authored bytes.");
	document.ToggleCollision(tile);
	Require(document.GetLevel().GetCells()[tile].Terrain == OccupancyCode::Wall && document.GetBytes()[grid + tile * 24 + 5] == 0xA5, "Collision painting lost unrelated cell flags.");
	const bytestream collision = document.GetBytes();
	Require(Rejects([&]() { document.Place(ObjectKind::Sheep, tile); }) && document.GetBytes() == collision, "Rejected placement changed the level.");
	Require(document.Undo() && document.GetBytes() == source && !document.IsDirty(), "Undo did not restore exact input bytes.");
	Require(document.Redo() && document.GetBytes() == collision && document.Undo(), "Redo or savepoint tracking failed.");
	const i32 sheep = document.Place(ObjectKind::Sheep, tile);
	Require(document.GetLevel().GetObjects()[sheep].Tile() == tile && document.GetLevel().GetCells()[tile].Terrain == OccupancyCode::Sheep, "New animal lacks owned collision.");
	document.Move(sheep, 40, 0);
	Require(document.GetLevel().GetCells()[tile].Terrain == OccupancyCode::Empty && document.GetLevel().GetCells()[tile + 1].Terrain == OccupancyCode::Sheep, "Moving an animal left collision behind.");
	document.Rotate(sheep);
	Require(document.GetLevel().GetObjects()[sheep].Facing() == 1, "Actor facing did not rotate.");
	document.Delete(sheep);
	Require(document.GetLevel().GetCells()[tile + 1].Terrain == OccupancyCode::Empty && document.Undo() && document.GetLevel().GetObjects()[sheep].Kind() == static_cast<i32>(ObjectKind::Sheep), "Delete/undo failed to restore the linked actor.");
	Require(Rejects([&]() { document.Move(sheep, 40 * 22, 0); }), "Actor drag wrapped around to another grid row.");
	Require(Rejects([&]() { document.Delete(0); }), "Editor deleted the last player start.");
	LevelDocument items(engine, pack, level);
	const i32 item = items.Place(ObjectKind::Bone, tile);
	Require(items.GetLevel().GetCells()[tile].ItemCode == ItemCode::Bone && items.GetBytes()[grid + tile * 24 + 13] == 0xB6, "Item placement lost unrelated packed cell bytes.");
	items.Move(item, 40, 0);
	Require(items.GetLevel().GetCells()[tile].ItemCode == ItemCode::None && items.GetLevel().GetCells()[tile + 1].ItemObject == item, "Item move failed to transfer its pickup link.");
	items.Delete(item);
	Require(items.GetLevel().GetCells()[tile + 1].ItemCode == ItemCode::None, "Deleted item left a phantom pickup.");
	LevelDocument scenery(engine, pack, level);
	scenery.ToggleCollision(tile);
	const i32 decoration = scenery.Place(ObjectKind::Scenery, tile, 0);
	scenery.Move(decoration, 40, 0);
	scenery.Delete(decoration);
	Require(scenery.GetLevel().GetCells()[tile].Terrain == OccupancyCode::Wall, "Moving/deleting scenery silently changed authored collision.");
	const bytestream beforeInvalid = scenery.GetBytes();
	Require(Rejects([&]() { scenery.Place(ObjectKind::Scenery, tile, 999999); }) && scenery.GetBytes() == beforeInvalid, "Invalid scenery placement corrupted the document.");
	Require(Rejects([&]() { document.WritePreview("../editor-escape.lvl"); }) && Rejects([&]() { document.WritePreview(level.Map); }) && engine.ReadAsset(level.Map) == source, "Playtest export escaped or overwrote source data.");

	const str scratch = pack.AssetPrefix + "_editor_check_" + suffix + "/";
	std::vector<ObjectKind> kinds;
	for (const CharacterDefinition& character : GetCharacterDefinitions())
	{
		if (character.Category != CharacterCategory::Player && character.Category != CharacterCategory::Effect && (!character.XxxOnly || engine.GetData().GetGameplayEdition() == GameEdition::XXX))
			kinds.push_back(character.Kind);
	}
	for (const ItemDefinition& definition : GetItemDefinitions())
		kinds.push_back(definition.Kind);
	for (ObjectKind kind : kinds)
	{
		LevelDocument prototype(engine, pack, level);
		const i32 slot = prototype.Place(kind, tile);
		const LevelInfo test = prototype.WritePreview(scratch + std::to_string(static_cast<i32>(kind)) + ".lvl");
		LevelScene scene(engine, pack, test, 1);
		Require(!scene.GetObjects().Find(kind).empty(), "New authored object did not load into gameplay.");
		if (kind == ObjectKind::Farmer)
			Require(prototype.GetLevel().GetObjects()[slot].Height() == 160, "Farmer factory lost its native canvas dimensions.");
		for (i32 tick = 0; tick < 120; ++tick)
			scene.Update(1.0f / 60, 0, 0, false);
		for (const SpriteDraw& draw : scene.Draws())
			Require(draw.Sprite && draw.Frame >= 0 && draw.Frame < draw.Sprite->Rows * draw.Sprite->Columns, "Editor-created actor produced an invalid animation frame.");
	}

	LevelSetInfo episode;
	episode.Name = "Active catalog reload";
	episode.Levels = { level, level };
	GameSession active(engine, pack, episode);
	const fpath editionRoot = engine.GetData().GetRoot() / std::filesystem::u8path(pack.AssetPrefix);
	const size_t foldersBefore = PreviewFolders(editionRoot);
	{
		LevelEditor editor(engine, pack, level, 1);
		{
			WindowInput::Result hidePalette;
			hidePalette.BindingKey = 'b';
			editor.Handle(hidePalette, 0);
		}
		WindowInput::Result place;
		place.BindingKey = 'p';
		editor.Handle(place, 0);
		WindowInput::Result wheel;
		wheel.Wheel = -1;
		wheel.Mouse = { 100, 140 };
		editor.Handle(wheel, 0);
		const bytestream beforeGhost = editor.GetDocument().GetBytes();
		const Sprite* chicken = editor.GetDocument().GetSprite(ObjectKind::Chicken);
		const std::vector<SpriteDraw>& ghost = editor.Draws();
		Require(std::any_of(ghost.begin(), ghost.end(), [&](const SpriteDraw& draw) { return draw.Sprite == chicken; }) && editor.GetDocument().GetBytes() == beforeGhost, "Wheel/placement preview failed or mutated the document.");
		wheel.Wheel = 1;
		editor.Handle(wheel, 0);
		place = {};
		place.Click = true;
		place.Mouse = { 100, 140 };
		editor.Handle(place, 0);
		Require(editor.GetDocument().GetLevel().GetObjects()[2].Kind() == static_cast<i32>(ObjectKind::Sheep), "Place tool did not create a sheep.");
		WindowInput::Result select;
		select.BindingKey = 'v';
		editor.Handle(select, 0);
		const LevelObjectRecord& record = editor.GetDocument().GetLevel().GetObjects()[2];
		const Sprite* image = editor.GetDocument().GetSprite(record);
		POINT hit{};
		bool found = false;
		for (i32 y = 0; y < image->FrameHeight && !found; ++y)
		{
			for (i32 x = 0; x < image->FrameWidth && !found; ++x)
			{
				if ((image->Pixels[y * image->Width + x] & 0xFFFFFF) != 0xFF00FF)
				{
					hit = { static_cast<LONG>(record.X()) + x, static_cast<LONG>(record.Y()) + y };
					found = true;
				}
			}
		}
		Require(found, "Cannot find an opaque selection point.");
		WindowInput::Result drag;
		drag.Click = true;
		drag.MouseHeld = true;
		drag.Mouse = hit;
		editor.Handle(drag, 0);
		drag.Click = false;
		drag.MouseHeld = false;
		drag.Mouse.x += 40;
		editor.Handle(drag, 0);
		Require(editor.GetDocument().GetLevel().GetObjects()[2].Tile() == tile + 1, "Pixel selection/drag did not move the authored object.");
		WindowInput::Result erase;
		erase.Delete = true;
		editor.Handle(erase, 0);
		Require(editor.GetDocument().GetLevel().GetObjects()[2].Kind() == -1, "Delete command missed the selection.");
		erase = {};
		erase.Undo = true;
		editor.Handle(erase, 0);
		Require(editor.GetDocument().GetLevel().GetObjects()[2].Kind() == static_cast<i32>(ObjectKind::Sheep), "Editor undo did not restore the deletion.");
		const bytestream beforeTest = editor.GetDocument().GetBytes();
		WindowInput::Result test;
		test.Playtest = true;
		editor.Handle(test, 0);
		Require(editor.IsPlaytesting(), "F5 did not enter playtest.");
		test = {};
		editor.Handle(test, 1);
		if (capture)
			engine.DrawFrame(editor.Draws(), ("editor-playtest-" + suffix + ".bmp").c_str());
		test.Playtest = true;
		editor.Handle(test, 0);
		Require(!editor.IsPlaytesting() && editor.GetDocument().GetBytes() == beforeTest && engine.ReadAsset(level.Map) == source, "Playtest mutated the authoring document/source.");
		Require(!editor.RequestWindowClose(), "Window close discarded unsaved edits without confirmation.");
		WindowInput::Result save;
		save.SaveEditor = true;
		editor.Handle(save, 0);
		Require(editor.HasSaved() && !editor.GetDocument().IsDirty() && editor.RequestWindowClose(), "Save copy did not clear the close guard/savepoint.");
		if (capture)
			engine.DrawFrame(editor.Draws(), ("editor-canvas-" + suffix + ".bmp").c_str());
	}
	Require(PreviewFolders(editionRoot) == foldersBefore, "Disposable playtest folder was not cleaned up.");
	document.SetMetadata(level.Name + " saved", level.Music);
	project.SetName("Roundtrip " + suffix);
	document.SetTileSet(1);
	document.SetTimeLimit(123.5f);
	const str saved = project.Save();
	Require(project.Save() == saved && !document.IsDirty(), "Repeated save created a different project or lost the savepoint.");
	const size_t oldCount = engine.GetData().GetPacks().size();
	engine.ReloadPacks();
	Require(engine.GetData().GetPacks().size() == oldCount + 2, "Saved folder packs were not discovered without restarting.");
	bool loaded = false;
	for (const LevelPackInfo& exported : engine.GetData().GetPacks())
	{
		if (exported.Descriptor != saved)
			continue;
		const LevelInfo& edited = exported.LevelSets.front().Levels.front();
		Require(edited.Name == document.GetInfo().Name && engine.ReadAsset(edited.Map) == document.GetBytes(), "Saved XML metadata or level bytes did not round-trip.");
		LevelScene scene(engine, exported, edited, 1);
		Require(scene.GetInitialTime() == 123.5f, "Exported timer did not reach gameplay.");
		loaded = true;
	}
	Require(loaded && engine.ReadAsset(level.Map) == source, "Export did not load or modified original data.");
	active.Update(0.01f, 0, 0, false);
	active.DebugNextLevel();
	Require(active.LevelNumber() == 2 && active.GetPack().Id == pack.Id, "Refreshing packs invalidated the active game's metadata.");
	if (capture)
	{
		WindowInput guarded(engine.WindowHandle(), engine.ScaledVideo());
		SendMessageW(engine.WindowHandle(), WM_MOUSEWHEEL, MAKEWPARAM(0, 60), 0);
		Require(guarded.Poll().Wheel == 0, "Partial wheel delta was not retained.");
		SendMessageW(engine.WindowHandle(), WM_MOUSEWHEEL, MAKEWPARAM(0, 60), 0);
		Require(guarded.Poll().Wheel == 1, "Wheel deltas did not accumulate.");
		SendMessageW(engine.WindowHandle(), WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-240)), 0);
		Require(guarded.Poll().Wheel == -2, "Multiple reverse wheel notches were lost.");
		guarded.SetEditorCloseGuard(true);
		SendMessageW(engine.WindowHandle(), WM_CLOSE, 0, 0);
		Require(IsWindow(engine.WindowHandle()) && guarded.Poll().CloseRequested, "Native close guard did not intercept a window close.");
		guarded.SetEditorCloseGuard(false);
	}
	CheckProjectWorkflow(engine, pack, level, capture);
	CheckProjectManagement(engine, pack, level, capture);
	CheckEditorUsability(engine, pack, level, capture);
	CheckPaletteAndMusic(engine, pack, level, capture);
	CheckViewportAndLayers(engine, pack, level, capture);
	CheckPlacementOrder(engine, pack, level);
	CheckNaturalSceneryDepth(engine, pack, level, capture);
	CheckAnimalPlacementDefaults(engine, pack, level, capture);
	CheckLinkAnnotations(engine, pack, level, capture);
	CheckContinuousPainting(engine, pack, level, capture);
	CheckSceneryWorkflow(engine, pack, level, capture);
	CheckEditorMenu(engine, capture);
	CheckRawInspection(engine, pack, level, capture);
	CheckStandaloneExports(engine, pack, level, capture);
	Log("PASS: integrated editor " + suffix + " exact data preservation, placement/move/delete/rotate, collision, undo/redo, protected export, catalog reload, playtest isolation/cleanup and close guards.");
}
