#include "EditorChecks.h"
#include "Editor/LevelEditor.h"
#include "Scene/Characters/Characters.h"
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <limits>

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
	click(palette.left + PlacementPalette::Width / PlacementPalette::GroupCount + 2, palette.top + 28);
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
	Require(moved.left == 80 && moved.top == 100 && document.GetBytes() == beforePicking, "Sidebar drag did not move its embedded properties or edited data.");
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
	click(palette.left + PlacementPalette::Width / PlacementPalette::GroupCount + 2, palette.top + 28);
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
	Require(placement.GetDocument().GetLevel().GetObjects()[3].Kind() == static_cast<i32>(ObjectKind::Inactive), "Scenery placement was not one-shot like the standalone editor.");
	key('p');
	key('a');
	click(113, 157);
	Require(placement.GetDocument().GetLevel().GetObjects()[3].X() == 113 && placement.GetDocument().GetLevel().GetObjects()[3].Y() == 157, "Canvas-origin anchor no longer supports exact legacy placement.");
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
	input.Mouse = { sidebar.left + 2 * (PlacementPalette::Width / PlacementPalette::GroupCount) + 2, sidebar.top + 28 };
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
	Log("PASS: editor cursor hotspot, centered/origin anchors and exact preview/commit, one-shot scenery, persistent searchable object list, UI gesture ownership, hidden interface hit testing and playtest HUD toggle.");
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
	auto chooseGroup = [&](i32 group) { const RECT bounds = editor.GetPaletteBounds(); click(bounds.left + group * (PlacementPalette::Width / PlacementPalette::GroupCount) + 2, bounds.top + 28); };
	auto search = [&](const str& name) {
		const RECT bounds = editor.GetPaletteBounds();
		click(bounds.left + 12, bounds.top + 52);
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
	CheckEditorUsability(engine, pack, level, capture);
	CheckPaletteAndMusic(engine, pack, level, capture);
	Log("PASS: integrated editor " + suffix + " exact data preservation, placement/move/delete/rotate, collision, undo/redo, protected export, catalog reload, playtest isolation/cleanup and close guards.");
}
