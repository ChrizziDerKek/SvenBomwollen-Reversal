#include "Scene/Level/LevelFile.h"
#include "Game/DebugMenu.h"
#include "Scene/Objects/ObjectCatalog.h"
#include "Scene/Characters/Characters.h"
#include "Utility/Logger.h"
#include "Engine/MudGE.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include "Game/GameUi.h"
#include "Game/GameSession.h"
#include "FSR1/FsrPresenter.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <set>
#include <array>
#include <queue>
#include <cstring>
#include <functional>

namespace
{
void Require(bool value, cstr message)
{
	if (!value)
		throw std::runtime_error(message);
}
void CheckScalingCoordinates()
{

	auto r = FsrPresenter::ViewPort(1920, 1080);
	Require(r.left == 240 && r.right == 1680 && r.top == 0 && r.bottom == 1080, "Widescreen scaling must pillarbox to 4:3.");
	auto p = FsrPresenter::LogicalMouse({ 960, 540 }, 1920, 1080);
	Require(p.x == 400 && p.y == 300, "Scaled mouse center must map to the logical game center.");
	p = FsrPresenter::LogicalMouse({ 239, 100 }, 1920, 1080);
	Require(p.x < 0 && p.y < 0, "Black bars must not activate game UI.");
	p = FsrPresenter::LogicalMouse({ 1679, 1079 }, 1920, 1080);
	Require(p.x == 799 && p.y == 599, "Bottom-right scaled pixel must stay inside logical bounds.");
	r = FsrPresenter::ViewPort(800, 1000);
	Require(r.top == 200 && r.bottom == 800, "Tall windows must letterbox without distorting the game.");
	p = FsrPresenter::LogicalMouse({ 0, 0 }, 0, 0);
	Require(p.x < 0 && p.y < 0, "Minimized windows must not divide mouse coordinates by zero.");
}
std::vector<u32> ReadScaledCapture(cstr path, i32 width, i32 height)
{
	std::ifstream file(path, std::ios::binary);
	BITMAPFILEHEADER header{};
	BITMAPINFOHEADER info{};
	file.read(reinterpret_cast<char*>(&header), sizeof(header));
	file.read(reinterpret_cast<char*>(&info), sizeof(info));
	Require(file && info.biWidth == width && info.biHeight == -height && info.biBitCount == 32, "Scaled capture dimensions/format are incorrect.");
	file.seekg(header.bfOffBits);
	std::vector<u32> pixels(width * height);
	file.read(reinterpret_cast<char*>(pixels.data()), pixels.size() * 4);
	Require(static_cast<bool>(file), "Truncated GPU capture.");
	for (auto& pixel : pixels)
		pixel &= 0xffffff;
	return pixels;
}
void CheckUpscalerGpu()
{

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	FsrPresenter presenter(1280, 720, ScalingFilter::FSR1, 0.5f);
	std::vector<u32> source(800 * 600, 0x336699);
	presenter.Present(source.data(), "fsr-flat.bmp");
	auto flat = ReadScaledCapture("fsr-flat.bmp", 1280, 720);
	Require(flat[360 * 1280] == 0 && flat[360 * 1280 + 1279] == 0, "FSR black bars must be black.");
	for (i32 x : { 160, 640, 1119 })
	{
		const auto c = flat[360 * 1280 + x];
		for (i32 shift : { 0, 8, 16 })
			Require(std::abs(static_cast<i32>((c >> shift) & 255) - static_cast<i32>((0x336699 >> shift) & 255)) <= 1, "FSR must preserve constant colors, channels and edge clamping.");
	}
	for (i32 y = 0; y < 600; ++y)
	{
		for (i32 x = 0; x < 800; ++x)
			source[y * 800 + x] = x < 400 ? 0x202020 : (y > x - 400 ? 0xf0c080 : 0x4070c0);
	}
	presenter.SetFilter(ScalingFilter::Nearest);
	presenter.Present(source.data(), "nearest.bmp");
	auto nearest = ReadScaledCapture("nearest.bmp", 1280, 720);
	presenter.SetFilter(ScalingFilter::Bilinear);
	presenter.Present(source.data(), "bilinear.bmp");
	auto bilinear = ReadScaledCapture("bilinear.bmp", 1280, 720);
	presenter.SetFilter(ScalingFilter::FSR1);
	presenter.Present(source.data(), "fsr-edges.bmp");
	auto fsr = ReadScaledCapture("fsr-edges.bmp", 1280, 720);
	Require(nearest != bilinear && fsr != bilinear && fsr != nearest, "FSR, bilinear and nearest must execute distinct filters.");
	presenter.SetSharpness(2.0f);
	presenter.Present(source.data(), "fsr-soft.bmp");
	Require(fsr != ReadScaledCapture("fsr-soft.bmp", 1280, 720), "RCAS sharpening adjustment has no effect.");
	RECT rect{ 0, 0, 800, 600 };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	SetWindowPos(presenter.Window(), nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER);
	presenter.Present(source.data(), "native-sized.bmp");
	Require(ReadScaledCapture("native-sized.bmp", 800, 600) == source, "Native-size fallback must not sharpen or resample the source.");
	SendMessageW(presenter.Window(), WM_KEYDOWN, VK_F11, 0);
	presenter.Present(source.data());
	SendMessageW(presenter.Window(), WM_KEYDOWN, VK_F11, 0);
	presenter.Present(source.data(), "restored.bmp");
	Require(ReadScaledCapture("restored.bmp", 800, 600) == source, "Fullscreen restoration must recreate the original-sized targets.");
	SendMessageW(presenter.Window(), WM_CLOSE, 0, 0);
	Require(presenter.Closed(), "Closing the scaled window must request game exit.");
	Log("PASS: native FSR1 shaders, RCAS, comparison filters, color/edge preservation, letterboxing, resize/fullscreen and close.");
}
void CheckCapturePixel(cstr path, i32 x, i32 y, u32 expected)
{
	std::ifstream file(path, std::ios::binary);
	BITMAPFILEHEADER header{};
	BITMAPINFOHEADER info{};
	file.read(reinterpret_cast<char*>(&header), sizeof(header));
	file.read(reinterpret_cast<char*>(&info), sizeof(info));
	Require(file && header.bfType == 0x4d42 && info.biBitCount == 32 && info.biHeight < 0, "Unexpected native capture format.");
	file.seekg(header.bfOffBits + (static_cast<std::streamoff>(y) * info.biWidth + x) * 4);
	u32 observed = 0;
	file.read(reinterpret_cast<char*>(&observed), 4);
	Require(file && (observed & 0xffffff) == (expected & 0xffffff), "Native tab pixels covered the paper body.");
}
void Step(LevelScene& scene, i32 dx, i32 dy)
{
	scene.Move(0.1f, dx, dy);
	scene.Move(0.1f, dx, dy);
	scene.Move(0.1f, 0, 0);
}

// Use an isolated asset tree so authored regression levels never touch game data.
void LinkFixtureData(const fpath& source, const fpath& target)
{
	std::filesystem::create_directories(target);
	for (const auto& entry : std::filesystem::recursive_directory_iterator(source))
	{
		const auto destination = target / std::filesystem::relative(entry.path(), source);
		if (entry.is_directory())
			std::filesystem::create_directories(destination);
		else if (entry.is_regular_file())
		{
			std::error_code error;
			std::filesystem::create_hard_link(entry.path(), destination, error);
			if (error)
				std::filesystem::copy_file(entry.path(), destination);
		}
	}
}

struct FixtureActor
{
	i32 Kind, X, Y, Facing, Anger = 6, Temperament = 0;
	i32 State = 0;
	f32 Pulse = 1000;
	i32 AngerTicks = 0;
};
LevelInfo FixtureLevel(MudGE& engine, const str& name, std::initializer_list<FixtureActor> actors, i32 pickup = 0, GameEdition edition = GameEdition::Zwo)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack) { return pack.Name == "XS"; });
	auto level = xs->LevelSets.front().Levels.front();
	// v6 layout is documented in docs/VERIFIED_FORMAT.md and LevelScene's parser.
	bytestream data(16 + 9048 + 25600);
	auto integer = [&](size_t offset, i32 value) { std::memcpy(data.data() + offset, &value, 4); };
	auto real = [&](size_t offset, f32 value) { std::memcpy(data.data() + offset, &value, 4); };
	integer(0, edition == GameEdition::XXX ? 7 : 6);
	integer(8, 0); // Render requested UFOs and their shadows, as in visible-UFO levels.
	for (i32 tile = 0; tile < 22 * 17; ++tile)
	{
		const auto offset = 16 + tile * 24;
		integer(offset, tile % 22 == 0 || tile % 22 == 21 || tile / 22 == 0 || tile / 22 == 16 ? 1 : 0);
		integer(offset + 16, -1);
	}
	real(16 + 9044, 180);
	for (i32 slot = 0; slot < 256; ++slot)
		integer(16 + 9048 + slot * 100 + 44, -1);
	i32 slot = 0;
	for (const auto& actor : actors)
	{
		const i32 tile = ((actor.Y + 60) / 40 + 1) * 22 + (actor.X + 60) / 40 + 1;
		const auto offset = 16 + 9048 + slot * 100;
		// Inputs locate a 120px actor canvas. Preserve that logical center for
		// the farmer's 160px canvas and the distractions' 80px canvases.
		const i32 size = actor.Kind == 5 ? 160 : (actor.Kind >= 7 && actor.Kind <= 10 ? 80 : 120);
		const i32 x = actor.X + 60 - size / 2, y = actor.Y + 60 - size / 2;
		real(offset, static_cast<f32>(x));
		real(offset + 4, static_cast<f32>(y));
		real(offset + 8, static_cast<f32>(x));
		real(offset + 12, static_cast<f32>(y));
		integer(offset + 20, size);
		integer(offset + 24, size);
		integer(offset + 92, actor.Kind == 5 ? 40 : (actor.Kind == 0 ? 30 : (actor.Kind >= 7 && actor.Kind <= 10 ? 60 : 0)));
		integer(offset + 44, actor.Kind);
		real(offset + 52, 1000);
		real(offset + 56, 15);
		data[offset + 65] = static_cast<u8>(actor.Temperament);
		data[offset + 64] = static_cast<u8>(actor.AngerTicks);
		integer(offset + 72, actor.State);
		integer(offset + 76, actor.Facing);
		integer(offset + 80, tile);
		integer(offset + 84, actor.Anger);
		real(offset + 88, actor.Pulse);
		if (actor.Kind >= 7 && actor.Kind <= 10)
		{
			constexpr i32 itemCodes[] = { 130, 131, 129, 128 };
			integer(offset + 68, 1); // Permanent distraction fixture.
			data[16 + tile * 24 + 12] = static_cast<u8>(itemCodes[actor.Kind - 7]);
			integer(16 + tile * 24 + 16, slot);
		}
		else
		{
			const CharacterDefinition* character = FindCharacterDefinition(static_cast<ObjectKind>(actor.Kind));
			integer(16 + tile * 24, actor.Kind == 0 ? 100 : (character ? static_cast<i32>(character->OccupancyCode) : actor.Kind));
		}
		if (slot == 0 && pickup)
			data[16 + tile * 24 + 12] = static_cast<u8>(pickup);
		++slot;
	}
	if (edition == GameEdition::XXX)
	{
		std::array<std::array<i32, 4>, LevelFile::ObjectCount> layouts{};
		layouts.fill({ 0, 0, 400, 2 });
		const size_t offset = data.size();
		data.resize(offset + sizeof(layouts));
		std::memcpy(data.data() + offset, layouts.data(), sizeof(layouts));
	}
	level.Map = engine.GetData().EditionAsset(GameEdition::Zwo, "XS/levels/check-" + name + ".lvl");
	std::ofstream output(engine.GetData().GetRoot() / std::filesystem::u8path(level.Map), std::ios::binary);
	output.write(reinterpret_cast<cstr>(data.data()), static_cast<std::streamsize>(data.size()));
	if (!output)
		throw std::runtime_error("Cannot write isolated regression level.");
	return level;
}

void CheckAnimalMoodEffects(MudGE& engine)
{
	const auto& packs = engine.GetData().GetPacks();
	const auto xs = std::find_if(packs.begin(), packs.end(), [](const LevelPackInfo& pack) { return pack.Name == "XS"; });
	for (ItemCode code : { ItemCode::StinkMushroom, ItemCode::Flower })
	{
		const LevelInfo level = FixtureLevel(engine, "all-animal-mood-" + std::to_string(static_cast<i32>(code)), { { 0, 240, 200, 0 }, { 11, 280, 200, 0 }, { 12, 240, 240, 0 }, { 13, 200, 200, 0 }, { 2, 240, 160, 0 }, { 12, 600, 400, 0 } }, static_cast<i32>(code));
		LevelScene scene(engine, *xs, level, 1);
		scene.GetItems().PickupPlayerItem();
		scene.GetItems().AffectNearbyAnimals();
		for (size_t i = 1; i <= 4; ++i)
			Require(scene.GetObjects()[i].Impatience == (code == ItemCode::StinkMushroom ? 10 : 0), "Mushroom/flower did not update every nearby animal species.");
		Require(scene.GetObjects()[5].Impatience == 6, "Mood effect reached an animal outside the adjacent ring.");
	}
	for (i32 kind : { 11, 12, 13 })
	{
		const LevelInfo level = FixtureLevel(engine, "angry-animal-" + std::to_string(kind), { { 0, 240, 160, 4 }, { kind, 240, 200, 1, 10 } });
		LevelScene scene(engine, *xs, level, 1);
		scene.Update(0.01f, 0, 0, true);
		Require(scene.TakeTimeDelta() == 0, "An angry non-sheep animal kicked Sven.");
		const std::vector<str> sounds = scene.TakeSounds();
		Require(std::none_of(sounds.begin(), sounds.end(), [](const str& sound) { return sound.find("kickSven") != str::npos; }),
		        "A non-sheep animal played the sheep kick sound.");
		for (i32 frame = 0; frame < 1000; ++frame)
			scene.Update(0.02f, 0, 0, false);
		Require(!IsDemonState(scene.GetObjects()[1].State), "An angry non-sheep animal entered a demon state.");
	}
	Log("PASS: smell/flower affect all nearby animals; other animals never kick or become demons.");
}

void CheckDebugMenu(MudGE& engine, bool capture = false)
{
	const auto& packs = engine.GetData().GetPacks();
	const auto xs = std::find_if(packs.begin(), packs.end(), [](const LevelPackInfo& pack) { return pack.Name == "XS"; });
	const LevelInfo level = FixtureLevel(engine, "debug-tools", { { 0, 240, 200, 0 }, { 2, 640, 400, 0 } });
	LevelSetInfo set{ "Debug regression", { level, level } };
	GameSession session(engine, *xs, set);
	session.DebugChangeLives(-1);
	Require(session.GetLives() == 2, "Debug remove-life incorrectly applied native life insurance.");
	session.DebugChangeLives(1);
	Require(session.GetLives() == 3, "Debug add-life did not change the life count.");
	i32 remaining = session.GetRemainingTargets();
	for (ObjectKind kind : { ObjectKind::Sheep, ObjectKind::Chicken, ObjectKind::Goat, ObjectKind::Pig })
		Require(session.DebugSpawn(kind), "Debug animal spawn failed on an open level.");
	Require(session.GetRemainingTargets() == remaining + 4, "Spawned animals did not become completion targets.");
	for (const ItemDefinition& item : GetItemDefinitions())
		Require(session.DebugSpawn(item.Kind), "Debug item spawn failed on an open level.");
	Require(!session.DebugSpawn(ObjectKind::Player), "Debug spawner accepted a second player.");
	DebugMenu menu(engine);
	menu.Toggle();
	WindowInput::Result click;
	click.Click = true;
	click.Mouse = { 160, 200 };
	menu.Handle(click, &session);
	Require(session.GetLives() == 4, "Debug menu mouse action did not add a life.");
	WindowInput::Result select;
	select.Down = true;
	menu.Handle(select, &session);
	WindowInput::Result confirm;
	confirm.Confirm = true;
	menu.Handle(confirm, &session);
	Require(session.GetLives() == 3, "Debug menu keyboard action did not remove a life.");
	const size_t base = session.Draws().size();
	Require(menu.Draws(session.Draws()).size() > base, "Debug overlay did not draw its controls.");
	if (capture)
		engine.DrawFrame(menu.Draws(session.Draws()), "debug-menu.bmp");
	WindowInput::Result back;
	back.Back = true;
	menu.Handle(back, &session);
	Require(!menu.IsOpen(), "Escape did not close the debug menu.");
	session.DebugRestartLevel();
	Require(session.GetRemainingTargets() == remaining && session.GetLives() == 3, "Debug restart did not reset the level while retaining campaign lives.");
	session.DebugNextLevel();
	Require(session.LevelNumber() == 2 && !session.GetResults(), "Debug next-level did not load the next map.");
	session.DebugNextLevel();
	Require(session.Finished() && session.GetResults(), "Debug skip on the last level did not finish the pack.");
	LevelScene full(engine, *xs, level, 1);
	while (full.GetObjects().Size() < LevelFile::ObjectCount)
	{
		WorldObject object;
		full.GetObjects().Add(object);
	}
	Require(!full.SpawnNearPlayer(ObjectKind::Goat), "Debug spawning overran the native object pool.");
	Log("PASS: debug menu mouse/keyboard, item/animal spawning, pool limit, lives, restart and final-pack skip.");
}

void CheckDepthOrdering(MudGE& engine, bool capture = false)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& p) { return p.Name == "XS"; });
	auto addScenery = [&](LevelInfo level, i32 y, i32 texture = 0, i32 height = 80, i32 adjustment = 0, i32 x = 240) {
		auto bytes = engine.ReadAsset(level.Map);
		const size_t record = 16 + 9048 + 2 * 100;
		auto integer = [&](size_t offset, i32 value) { std::memcpy(bytes.data() + record + offset, &value, 4); };
		auto real = [&](size_t offset, f32 value) { std::memcpy(bytes.data() + record + offset, &value, 4); };
		real(0, static_cast<f32>(x));
		real(4, static_cast<f32>(y));
		real(8, static_cast<f32>(x));
		real(12, static_cast<f32>(y));
		integer(16, 0);
		integer(20, 80);
		integer(24, height);
		integer(44, 1);
		integer(48, 0);
		integer(68, texture);
		integer(92, adjustment);
		std::ofstream file(engine.GetData().GetRoot() / std::filesystem::u8path(level.Map), std::ios::binary | std::ios::trunc);
		file.write(reinterpret_cast<cstr>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		Require(static_cast<bool>(file), "Cannot write depth regression scenery.");
		return level;
	};
	auto index = [](const std::vector<SpriteDraw>& draws, const Sprite* sprite, i32 x, i32 y) {
		const auto found = std::find_if(draws.begin(), draws.end(), [&](const auto& d) { return d.Sprite == sprite && d.X == x && d.Y == y; });
		Require(found != draws.end(), "Depth regression sprite is missing.");
		return found - draws.begin();
	};
	cstr files[] = { "Chicken", "Goat", "Pig" };
	for (i32 kind = 11; kind <= 13; ++kind)
	{
		const auto* animalSprite = engine.LoadSprite(str("Sven2/gfx/Animals/") + files[kind - 11] + "_Stand.bmp", 4, 1);
		const auto north = FixtureLevel(engine, "depth-north-" + std::to_string(kind), { { 0, 240, 160, 4 }, { kind, 240, 200, 1 } });
		LevelScene approach(engine, *xs, north, 1);
		const auto& front = approach.Draws();
		Require(index(front, animalSprite, 240, 200) > index(front, approach.GetPlayerDraw().Sprite, 240, 160), "Sven approaching from above painted his legs over the animal.");
		if (capture)
			engine.DrawFrame(front, ("animal-north-" + std::to_string(kind) + ".bmp").c_str());
		const auto south = addScenery(FixtureLevel(engine, "depth-interaction-" + std::to_string(kind), { { 0, 240, 240, 0 }, { kind, 240, 200, 0 } }), 230);
		const auto& texture = engine.GetData().TileSet(*xs, south).Tiles.front();
		const auto* scenery = engine.LoadSprite(texture.File, texture.Frames, 1);
		LevelScene interacting(engine, *xs, south, 1);
		const auto& idle = interacting.Draws();
		Require(index(idle, animalSprite, 240, 200) < index(idle, scenery, 240, 230), "Occlusion fixture animal is not behind the scenery.");
		if (capture)
			engine.DrawFrame(idle, ("animal-behind-idle-" + std::to_string(kind) + ".bmp").c_str());
		interacting.Update(0.01f, 0, 0, true);
		Require(interacting.GetPlayerDraw().Sprite != animalSprite, "Animal interaction did not start.");
		for (i32 frame = 0; frame < 4; ++frame)
		{
			const auto& active = interacting.Draws();
			Require(index(active, interacting.GetPlayerDraw().Sprite, 240, 200) < index(active, scenery, 240, 230), "Animal interaction jumped in front of intervening scenery.");
			Require(std::none_of(active.begin(), active.end(), [&](const auto& d) { return d.Sprite == animalSprite; }),
			        "Animal stand sprite was drawn again over its combined interaction.");
			if (capture && frame == 0)
				engine.DrawFrame(active, ("animal-behind-interaction-" + std::to_string(kind) + ".bmp").c_str());
			interacting.Update(0.01f, 0, 0, false);
		}
	}
	const auto moving = addScenery(FixtureLevel(engine, "depth-mid-step", { { 0, 240, 160, 4 }, { 11, 480, 400, 0 } }), 175);
	const auto& texture = engine.GetData().TileSet(*xs, moving).Tiles.front();
	const auto* scenery = engine.LoadSprite(texture.File, texture.Frames, 1);
	LevelScene walk(engine, *xs, moving, 1);
	const auto& before = walk.Draws();
	Require(index(before, walk.GetPlayerDraw().Sprite, 240, 160) < index(before, scenery, 240, 175), "Mid-step depth fixture has the wrong initial order.");
	walk.Move(0.1f, 0, 1);
	Require(walk.GetPlayerDraw().Y > 160 && walk.GetPlayerDraw().Y < 200, "Depth fixture did not stop between grid cells.");
	const auto& after = walk.Draws();
	Require(index(after, walk.GetPlayerDraw().Sprite, walk.GetPlayerDraw().X, walk.GetPlayerDraw().Y) > index(after, scenery, 240, 175), "Moving Sven kept the old tile's depth until committing his step.");
	const auto xxl = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& p) { return p.Name == "XXL"; });
	auto heapLevel = FixtureLevel(engine, "depth-forest-heap", { { 0, 240, 280, 4 }, { 11, 480, 400, 0 } });
	heapLevel.TileSet = xxl->LevelSets.front().Levels.front().TileSet;
	heapLevel = addScenery(heapLevel, 379, 2, 19, 20, 284);
	const auto& heapImage = engine.GetData().TileSet(*xxl, heapLevel).Tiles.at(2);
	const auto* heap = engine.LoadSprite(heapImage.File, heapImage.Frames, 1);
	LevelScene crossing(engine, *xxl, heapLevel, 1);
	crossing.Move(0.1f, 0, 1);
	const auto& crossingDraws = crossing.Draws();
	Require(crossing.GetPlayerDraw().Y > 280 && crossing.GetPlayerDraw().Y < 320 && index(crossingDraws, crossing.GetPlayerDraw().Sprite, crossing.GetPlayerDraw().X, crossing.GetPlayerDraw().Y) > index(crossingDraws, heap, 284, 379), "Forest dirt heap covered Sven after his rendered depth crossed it mid-step.");
	if (capture)
		engine.DrawFrame(crossingDraws, "forest-heap-mid-step.bmp");
	Log("PASS: interpolated depth, north animal approaches and occluded chicken/goat/pig interactions.");
}

void CheckSerializedPowerups(MudGE& engine)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& p) { return p.Name == "XS"; });
	const auto* icons = engine.LoadSprite("Sven2/gfx/icons.bmp", 5, 1);
	cstr durations[] = { "FireworkTime", "GiftTime", "ImpotentTime", "SmellTime", "FlowerTime" };
	for (i32 kind = 14; kind <= 18; ++kind)
	{
		auto level = FixtureLevel(engine, "serialized-powerup-" + std::to_string(kind), { { 0, 240, 200, 2 }, { 7, 280, 200, 0 }, { 2, 640, 400, 0 } });
		auto bytes = engine.ReadAsset(level.Map);
		const size_t record = 16 + 9048 + 100;
		auto put = [&](size_t off, i32 value) { std::memcpy(bytes.data() + off, &value, 4); };
		i32 tile = 0;
		std::memcpy(&tile, bytes.data() + record + 80, 4);
		put(record + 44, kind);
		put(record + 48, kind - 14);
		bytes[16 + tile * 24 + 12] = static_cast<u8>(kind + 118);
		std::ofstream file(engine.GetData().GetRoot() / std::filesystem::u8path(level.Map), std::ios::binary | std::ios::trunc);
		file.write(reinterpret_cast<cstr>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		file.close();
		Require(static_cast<bool>(file), "Cannot write serialized powerup fixture.");
		LevelScene pickup(engine, *xs, level, 1), expiry(engine, *xs, level, 1);
		const auto& initial = pickup.Draws();
		Require(std::any_of(initial.begin(), initial.end(), [&](const auto& d) { return d.Sprite == icons && d.Frame == kind - 14; }),
		        "Serialized powerup used the wrong icon frame.");
		Step(pickup, 1, 0);
		const auto& collected = pickup.Draws();
		Require(std::none_of(collected.begin(), collected.end(), [&](const auto& d) { return d.Sprite == icons; }),
		        "Serialized powerup did not disappear on collection.");
		Require(!pickup.TakeSounds().empty(), "Serialized powerup did not activate on collection.");
		const f32 duration = static_cast<f32>(engine.GetData().NumericParam(durations[kind - 14], 10));
		expiry.Update(duration + 0.1f, 0, 0, false);
		const auto& expired = expiry.Draws();
		Require(std::none_of(expired.begin(), expired.end(), [&](const auto& d) { return d.Sprite == icons; }),
		        "Serialized powerup did not expire at its native Zwo duration.");
	}
	Log("PASS: all five serialized Zwo powerup kinds render, collect and expire.");
}

void DriveTutorial(MudGE& engine, const std::function<void(f32, i32, i32, bool)>& update, const std::function<const std::vector<SpriteDraw>&()>& draws, const std::function<i32()>& remaining)
{
	auto bytes = engine.ReadAsset("XS/levels/Tutorial01v01.lvl");
	u32 nameLength = 0;
	std::memcpy(&nameLength, bytes.data() + 12, 4);
	std::array<bool, 300> floor{};
	for (i32 cell = 0; cell < 300; ++cell)
	{
		i32 code = 0;
		const i32 raw = 23 + (cell / 20) * 22 + cell % 20;
		std::memcpy(&code, bytes.data() + 16 + nameLength + raw * 24, 4);
		floor[cell] = code == 0 || code == 2;
	}
	const auto* getPlayerDraw = engine.LoadSprite("Sven2/gfx/Sven/sven_stand.bmp", 8, 1);
	const auto* sheep = engine.LoadSprite("Sven2/gfx/Sheep/Sheep_Stand.bmp", 4, 1);
	constexpr i32 directions[][2] = { { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 } };
	auto cellAt = [](const SpriteDraw& draw) {
		const i32 x = (draw.X + 60) / 40, y = (draw.Y + 60) / 40;
		return x >= 0 && x < 20 && y >= 0 && y < 15 ? y * 20 + x : -1;
	};
	f32 elapsed = 0;
	auto tick = [&](f32 seconds, i32 dx, i32 dy, bool action) {
		update(seconds, dx, dy, action);
		elapsed += seconds;
	};
	while (remaining() > 0 && elapsed < 150)
	{
		const auto& frame = draws();
		auto hero = std::find_if(frame.begin(), frame.end(), [&](const auto& draw) { return draw.Sprite == getPlayerDraw; });
		if (hero == frame.end())
		{
			tick(0.1f, 0, 0, false);
			continue;
		}
		const i32 start = cellAt(*hero);
		Require(start >= 0, "Player left the tutorial grid.");
		auto passable = floor;
		std::vector<i32> targets;
		for (const auto& draw : frame)
		{
			if (draw.Sprite != sheep)
				continue;
			const i32 cell = cellAt(draw);
			if (cell >= 0)
			{
				targets.push_back(cell);
				passable[cell] = false;
			}
		}
		std::array<i32, 300> previous;
		previous.fill(-1);
		previous[start] = start;
		std::queue<i32> queue;
		queue.push(start);
		while (!queue.empty())
		{
			const i32 current = queue.front();
			queue.pop();
			for (const auto& direction : directions)
			{
				const i32 x = current % 20 + direction[0], y = current / 20 + direction[1];
				if (x < 0 || x >= 20 || y < 0 || y >= 15)
					continue;
				const i32 next = y * 20 + x;
				if (!passable[next] || previous[next] >= 0)
					continue;
				previous[next] = current;
				queue.push(next);
			}
		}
		i32 destination = -1, facing = -1, bestDistance = 301;
		for (i32 target : targets)
		{
			for (i32 direction = 0; direction < 8; ++direction)
			{
				const i32 x = target % 20 - directions[direction][0], y = target / 20 - directions[direction][1];
				if (x < 0 || x >= 20 || y < 0 || y >= 15)
					continue;
				const i32 candidate = y * 20 + x;
				if (previous[candidate] < 0)
					continue;
				i32 distance = 0;
				for (i32 cell = candidate; cell != start; cell = previous[cell])
					++distance;
				if (distance < bestDistance)
				{
					bestDistance = distance;
					destination = candidate;
					facing = direction;
				}
			}
		}
		if (destination < 0)
		{
			tick(0.1f, 0, 0, false);
			continue;
		}
		if (destination == start)
		{
			tick(0.01f, directions[facing][0], directions[facing][1], false);
			const i32 before = remaining();
			for (i32 i = 0; i < 2400 && remaining() == before && elapsed < 150; ++i)
				tick(1.0f / 60, 0, 0, i % 8 == 0);
		}
		else
		{
			i32 next = destination;
			while (previous[next] != start)
				next = previous[next];
			const i32 dx = next % 20 - start % 20, dy = next / 20 - start / 20;
			tick(0.1f, dx, dy, false);
			tick(0.1f, dx, dy, false);
			tick(0.2f, 0, 0, false);
		}
	}
	Require(remaining() == 0, "Public-input tutorial driver did not complete the round.");
}
void CheckMenus(MudGE& engine, const fpath& saves)
{
	using Page = GameUi::Page;
	GameUi ui(engine, saves);
	WindowInput::Result input;
	input.Confirm = true;
	Require(ui.Handle(input) == UiAction::None && ui.GetPage() == Page::GetPacks, "Start did not open episodes.");
	Require(ui.Handle(input) == UiAction::Start, "Flat episode selection did not start a session.");
	Require(!ui.GetPack().Name.empty() && !ui.GetLevelSet().Levels.empty(), "Catalog selection is empty.");
	ui.Show(Page::Help);
	for (i32 i = 0; i < 4; ++i)
	{
		Require(ui.Draws(0).size() > 40, "Original help text is absent.");
		WindowInput::Result next;
		next.Right = true;
		ui.Handle(next);
	}
	for (i32 i = 0; i < 3; ++i)
	{
		WindowInput::Result next;
		next.Right = true;
		ui.Handle(next);
	}
	const auto* smallFont = engine.LoadSprite("Sven2/gfx/font2.bmp", 88, 1);
	const auto& credits = ui.Draws(0);
	Require(std::any_of(credits.begin(), credits.end(), [&](const auto& draw) { return draw.Sprite == smallFont && draw.X == 120 && draw.Y == 220 && draw.Frame == 2; }),
	        "Credits shifted the Coloration row into subsequent text.");
	Require(std::any_of(credits.begin(), credits.end(), [&](const auto& draw) { return draw.Sprite == smallFont && draw.X == 120 && draw.Y == 380 && draw.Frame == 21; }),
	        "Credits did not retain the original two-column thanks row.");
	ui.Show(Page::Highscores);
	Require(ui.Draws(0).size() > 100, "Original highscore defaults are absent.");
	const auto firstEpisode = ui.GetPack().Name + "/" + ui.GetLevelSet().Name;
	WindowInput::Result nextEpisode;
	nextEpisode.Click = true;
	nextEpisode.Mouse = { 275, 100 };
	ui.Handle(nextEpisode);
	Require(ui.GetPack().Name + "/" + ui.GetLevelSet().Name != firstEpisode, "Highscore mouse arrow did not change episodes.");
	WindowInput::Result previousEpisode;
	previousEpisode.Left = true;
	ui.Handle(previousEpisode);
	Require(ui.GetPack().Name + "/" + ui.GetLevelSet().Name == firstEpisode, "Highscore keyboard traversal did not restore the episode.");
	WindowInput::Result cards;
	cards.Click = true;
	cards.Mouse = { 510, 40 };
	ui.Handle(cards);
	const auto& cardDraws = ui.Draws(0);
	Require(std::any_of(cardDraws.begin(), cardDraws.end(), [](const auto& draw) { return draw.X == 310 && draw.Y == 150; }),
	        "Card omitted its title.");
	Require(std::any_of(cardDraws.begin(), cardDraws.end(), [](const auto& draw) { return draw.X == 310 && draw.Y == 200; }),
	        "Card omitted its description.");
	const auto variants = ShagAnimation::Load(engine);
	for (i32 card = 0; card < 33; ++card)
	{
		const auto& draws = ui.Draws(0);
		Require(std::any_of(draws.begin(), draws.end(), [&](const auto& draw) { return draw.Sprite == variants[static_cast<size_t>(card) * 2].Sprite; }),
		        "An available original card was incorrectly locked.");
		WindowInput::Result nextCard;
		nextCard.Right = true;
		ui.Handle(nextCard);
	}
	WindowInput::Result playCard;
	playCard.Confirm = true;
	ui.Handle(playCard);
	const auto cardSounds = ui.TakeSounds();
	Require(cardSounds.size() == 1 && cardSounds.front() == variants.front().Sound, "Card playback omitted its original sound.");
	ui.Handle(playCard);
	Require(ui.TakeSounds().empty(), "Card input restarted an animation that was already playing.");
	const auto& playback = ui.Draws(0.11f);
	Require(std::any_of(playback.begin(), playback.end(), [&](const auto& draw) { return draw.Sprite == variants.front().Sprite && draw.Frame == variants.front().Frames[1]; }),
	        "Card browser did not use the original 15 fps animation clock.");
	const auto* arrows = engine.LoadSprite("Sven2/gfx/pijlen.bmp", 8, 1);
	WindowInput::Result arrowHover;
	arrowHover.MouseMoved = true;
	arrowHover.Mouse = { 245, 360 };
	ui.Handle(arrowHover);
	const auto& hoveredArrow = ui.Draws(0);
	Require(std::any_of(hoveredArrow.begin(), hoveredArrow.end(), [&](const auto& draw) { return draw.Sprite == arrows && draw.X == 238 && draw.Y == 350 && draw.Frame == 3; }),
	        "Card arrow did not change color on hover.");
	const auto* paper = engine.LoadSprite("Sven2/gfx/Help_BG.jpg", 1, 1);
	const auto* scoreTabs = engine.LoadSprite("Sven2/gfx/Highsc_file_cards.bmp", 10, 1);
	Require(std::all_of(hoveredArrow.begin(), hoveredArrow.end(), [&](const auto& draw) { return draw.Sprite != scoreTabs || draw.ClipBottom == 60; }),
	        "Highscore tabs extend over the paper body.");
	Require(std::any_of(hoveredArrow.begin(), hoveredArrow.end(), [&](const auto& draw) { return draw.Sprite == paper; }),
	        "Highscore paper backdrop is absent.");
	ui.Show(Page::Pause);
	WindowInput::Result escape;
	escape.Back = true;
	Require(ui.Handle(escape) == UiAction::Resume, "Pause escape did not resume.");
	WindowInput::Result chooseNo;
	chooseNo.Right = true;
	ui.Handle(chooseNo);
	Require(ui.Handle(input) == UiAction::Resume, "Pause No did not resume.");
	ui.ShowResults({ 1250, 1, 180, 3, 3, 0, 0, 0, true, false });
	ui.Handle(input);
	Require(ui.GetPage() == Page::Name, "A qualifying round did not open name entry.");
	WindowInput::Result typing;
	typing.Text = u8"J\u00f6rg";
	ui.Handle(typing);
	BitmapFont nameFont(engine);
	const auto* largeFont = engine.LoadSprite("Sven2/gfx/font1.bmp", 88, 1);
	const auto& nameDraws = ui.Draws(0);
	Require(std::any_of(nameDraws.begin(), nameDraws.end(), [&](const auto& draw) { return draw.Sprite == largeFont && draw.X == 150 + nameFont.Width(typing.Text) && draw.Y == 130 && draw.Frame == 8; }),
	        "Highscore name caret did not use the original I glyph in the ranked row.");
	ui.Handle(input);
	Require(ui.GetPage() == Page::Highscores, "Name entry did not return to the score screen.");
	Highscores saved(saves / L"XS-0.tsv");
	Require(saved.GetEntries().front().Score == 1250, "Results did not persist the submitted highscore.");
	GameUi hover(engine, saves / L"hover");
	hover.Show(Page::GetPacks);
	WindowInput::Result mouse;
	mouse.MouseMoved = true;
	mouse.Mouse = { 200, 170 };
	hover.Handle(mouse);
	const auto& hoverDraws = hover.Draws(0);
	const auto* green = engine.LoadSprite("Sven2/gfx/fontgreen1.bmp", 88, 1);
	Require(std::any_of(hoverDraws.begin(), hoverDraws.end(), [&](const auto& draw) { return draw.Sprite == green && draw.Y == 158; }),
	        "Hover did not make the episode text green.");
	mouse.Click = true;
	Require(hover.Handle(mouse) == UiAction::Start, "Clicking a hovered episode did not start it.");
	GameUi exitMenu(engine, saves / L"exit-menu");
	WindowInput::Result exitClick;
	exitClick.Click = true;
	exitClick.Mouse = { 650, 555 };
	exitMenu.Handle(exitClick);
	Require(exitMenu.GetPage() == Page::Quit, "Exit button did not open confirmation.");
	const auto* logo = engine.LoadSprite("Sven2/gfx/SvenLogo.bmp", 1, 1);
	const auto* logoDetail = engine.LoadSprite("Sven2/gfx/SvenLogo_O.bmp", 4, 1);
	// Model a retained completed session being supplied after returning to title.
	const std::vector<SpriteDraw> staleLevel = { { logo, -777, -777, 0 } };
	const auto& exitDraws = exitMenu.Draws(0, &staleLevel);
	Require(!exitMenu.UsesSceneBackground() && std::none_of(exitDraws.begin(), exitDraws.end(), [](const auto& draw) { return draw.X == -777; }),
	        "Title exit confirmation drew the retained level behind the menu.");
	Require(std::any_of(exitDraws.begin(), exitDraws.end(), [&](const auto& draw) { return draw.Sprite == logo && draw.X == 234 && draw.Y == 180; }) &&
	            std::any_of(exitDraws.begin(), exitDraws.end(), [&](const auto& draw) { return draw.Sprite == logoDetail && draw.X == 429 && draw.Y == 316; }),
	        "Exit confirmation removed the title artwork behind it.");
	Require(std::count_if(exitDraws.begin(), exitDraws.end(), [](const auto& draw) { return draw.Y == 545; }) == 4,
	        "Exit confirmation removed the four menu buttons.");
	exitMenu.Handle(chooseNo);
	Require(exitMenu.Handle(input) == UiAction::None && exitMenu.GetPage() == Page::Title, "Declining exit did not return to the title.");
	exitMenu.Show(Page::Pause);
	const auto& pauseDraws = exitMenu.Draws(0, &staleLevel);
	Require(exitMenu.UsesSceneBackground() && pauseDraws.front().X == -777, "Pause dialog lost its active level background.");
	RoundResult completed{ 1250, 1, 180, 3, 3, 0, 0, 0, false, false, 3, 2 };
	ui.ShowResults(completed);
	const auto firstDrawCount = ui.Draws(0.1f).size();
	ui.ShowResults(completed);
	Require(ui.Draws(0).size() == firstDrawCount, "Updating bonus values restarted the blinking prompt every frame.");
	const auto* scoreSprite = engine.LoadSprite("Sven2/gfx/score.bmp", 4, 1);
	const auto& bonusDraws = ui.Draws(0);
	Require(std::any_of(bonusDraws.begin(), bonusDraws.end(), [&](const auto& draw) { return draw.Sprite == scoreSprite && draw.X == 395 && draw.Y == 380 && draw.Frame == 2; }),
	        "Bonus screen lost the session's score-icon animation frame.");
	const auto* sheepSprite = engine.LoadSprite("Sven2/gfx/Sheep/Sheep_Stand.bmp", 4, 1);
	Require(std::any_of(bonusDraws.begin(), bonusDraws.end(), [&](const auto& draw) { return draw.Sprite == sheepSprite && draw.X == 310 - sheepSprite->FrameWidth / 2 && draw.Y == 240 - sheepSprite->FrameHeight / 2; }),
	        "Bonus sheep sprite was not centered at the original coordinates.");
	completed.EpisodeComplete = true;
	completed.TotalSheep = 17;
	ui.ShowResults(completed);
	const auto* packImage = engine.LoadSprite("Sven2/gfx/ScoreScreen_BG_levelpack.bmp", 1, 1);
	const auto& finishDraws = ui.Draws(0.1f);
	Require(std::any_of(finishDraws.begin(), finishDraws.end(), [&](const auto& draw) { return draw.Sprite == packImage && draw.X == 315 && draw.Y == 294; }),
	        "Pack completion screen omitted its original illustration.");
	Log("PASS: flat episode selection, hover, highscore traversal, pause and four help tabs.");
}
void CheckScores(const fpath& saves)
{
	const auto file = saves / L"persistence.tsv";
	Highscores scores(file, { { "A", 1000, 0 }, { "B", 900, 0 } });
	scores.Submit(u8"J\u00f6rg", 1050, 2, 3);
	const auto& rows = scores.GetEntries();
	Require(rows.front().Score == 1050 && rows.front().Level == 2, "Score was not ranked.");
	Highscores reload(file);
	Require(reload.GetEntries().front().Name == u8"J\u00f6rg", "UTF-8 score name did not survive reload.");
	Require(reload.GetEntries().front().Sheep == 3, "Highscore animal count did not survive reload.");
	const auto legacy = saves / L"legacy.tsv";
	{
		std::ofstream output(legacy);
		output << "800\tOld\t2\n";
	}
	Highscores old(legacy);
	Require(old.GetEntries().front().Score == 800 && old.GetEntries().front().Sheep == 0, "Earlier score tables stopped loading.");
	bool rejected = false;
	try
	{
		reload.Submit("", 0);
	}
	catch (const std::invalid_argument&)
	{
		rejected = true;
	}
	Require(rejected, "Empty score name was accepted.");
	Log("PASS: highscore ranking, Unicode, atomic persistence and validation.");
}
void CheckGlyphs(MudGE& engine)
{
	const auto* placeholder = engine.LoadSprite("Sven2/gfx/noanim.bmp", 1, 1);
	Require((placeholder->Pixels.front() & 0xffffff) == 0xff00ff, "Original RGB565-equivalent chroma key was not normalized.");
	const i32 expected[] = { 74, 61, 9, 33, 42, 32, 46, 37, 39, 60, 59, 66, 67, 73, 86, 43, 85 };
	for (bool smallFont : { false, true })
	{
		BitmapFont font(engine, smallFont);
		std::vector<SpriteDraw> glyphs;
		font.Append(glyphs, u8"1.Jendrik?!,';\u00dfo\u00f8", 0, 0);
		Require(glyphs.size() == std::size(expected), "Font dropped punctuation or Unicode glyphs.");
		for (size_t i = 0; i < glyphs.size(); ++i)
			Require(glyphs[i].Frame == expected[i], "Font disagrees with the original character map.");
	}
	Log("PASS: original font punctuation, normal o, crossed o and sharp s mapping.");
}
void CheckMovement(MudGE& engine)
{
	LevelScene wall(engine);
	Require(wall.GetInitialTime() == 180 && wall.GetRemainingTargets() == 3, "Original tutorial state was not loaded.");
	Step(wall, 1, 0);
	Step(wall, 1, 0);
	for (i32 i = 0; i < 25; ++i)
		wall.Move(0.1f, 0, -1);
	Require(wall.GetPlayerDraw().X == 440 && wall.GetPlayerDraw().Y == 40, "Authored wall did not stop Sven.");
	LevelScene bush(engine);
	Step(bush, 1, 0);
	for (i32 i = 0; i < 25; ++i)
		bush.Move(0.1f, 0, -1);
	Step(bush, 1, 0);
	for (i32 i = 0; i < 5; ++i)
		bush.Move(0.1f, 0, 1);
	Require(bush.GetPlayerDraw().X == 440 && bush.GetPlayerDraw().Y == -40, "North bush collision regressed.");
	LevelScene puddle(engine);
	Step(puddle, -1, 0);
	Step(puddle, -1, 0);
	for (i32 i = 0; i < 20 && !puddle.TakeSplashSound(); ++i)
		puddle.Move(0.1f, 0, -1);
	const auto landing = puddle.GetPlayerDraw();
	Require((landing.X == 80 && landing.Y == 80) || (landing.X == 0 && landing.Y == 440), "Puddle landed outside its authored ring.");
	for (i32 i = 0; i < 5; ++i)
		puddle.Move(0.1f, 0, 1);
	Require(puddle.GetPlayerDraw().X == landing.X && puddle.GetPlayerDraw().Y == landing.Y, "Sven moved during the splash state.");
	Log("PASS: original tutorial timer, wall, bush and splash lock.");
}

void CheckInteraction(MudGE& engine)
{
	LevelScene scene(engine);
	for (i32 i = 0; i < 3; ++i)
		Step(scene, -1, 0);
	for (i32 i = 0; i < 3; ++i)
		Step(scene, 0, -1);
	scene.Move(0.01f, 0, -1);
	Require(scene.GetPlayerDraw().X == 240 && scene.GetPlayerDraw().Y == 160, "Sheep approach route was blocked.");
	i32 score = 0;
	f32 bonusTime = 0;
	for (i32 i = 0; i < 2400 && scene.GetRemainingTargets() == 3; ++i)
	{
		scene.Update(1.0f / 60.0f, 0, 0, i % 8 == 0);
		score += scene.TakeScoreDelta();
		bonusTime += scene.TakeTimeDelta();
	}
	Require(scene.GetRemainingTargets() == 2 && scene.GetCompletedSheep() == 1, "Repeated action did not resolve the approached sheep.");
	Require(score > 0 && (bonusTime == 5 || bonusTime == 10), "Sheep score and time bonus did not stay separate.");
	Require(!scene.TakeSounds().empty(), "Interaction did not emit its original sound events.");
	DriveTutorial(engine, [&](f32 dt, i32 dx, i32 dy, bool action) { scene.Update(dt, dx, dy, action); }, [&]() -> const auto& { return scene.Draws(); }, [&] { return scene.GetRemainingTargets(); });
	Require(scene.Completed() && scene.GetCompletedSheep() == 3, "Tutorial did not reach completion after resolving its three targets.");
	Log("PASS: normal-entry sheep interaction, repeated input, target resolution, score and time rewards.");
}

void CheckReportedGameplay(MudGE& engine)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack) { return pack.Name == "XS"; });
	const auto walking = FixtureLevel(engine, "walking", { { 0, 240, 200, 0 }, { 2, 640, 400, 0 } });
	LevelScene east(engine, *xs, walking, 1), west(engine, *xs, walking, 1);
	east.Update(0.05f, 1, 0, false);
	west.Update(0.05f, -1, 0, false);
	Require(east.GetPlayerDraw().Mirror && !west.GetPlayerDraw().Mirror, "Sven's side-running sprite faces away from movement.");

	const auto angry = FixtureLevel(engine, "angry", { { 0, 240, 200, 0 }, { 2, 240, 160, 0, 10 }, { 2, 640, 400, 0 } });
	LevelScene kicked(engine, *xs, angry, 1);
	const auto variants = ShagAnimation::Load(engine);
	std::set<const Sprite*> usedVariants;
	i32 kickSounds = 0;
	for (i32 frame = 0; frame < 60 * 60 && kicked.GetRemainingTargets() == 2; ++frame)
	{
		kicked.Update(1.0f / 60, 0, 0, frame % 8 == 0);
		if (std::any_of(variants.begin(), variants.end(), [&](const auto& variant) { return variant.Sprite == kicked.GetPlayerDraw().Sprite; }))
			usedVariants.insert(kicked.GetPlayerDraw().Sprite);
		for (const auto& sound : kicked.TakeSounds())
			if (sound == "Sven2/sfx/Sheep_kickSven.ogg")
				++kickSounds;
	}
	Require(kickSounds == 1, "Angry sheep repeatedly kicked during one stationary approach.");
	Require(kicked.GetRemainingTargets() == 1, "Sheep could not be interacted with after its kick.");
	Require(usedVariants.size() == 1, "Repeated action switched animation during one sheep interaction.");

	const auto smelly = FixtureLevel(engine, "smelly", { { 0, 200, 240, 0 }, { 2, 240, 160, 0, 6 }, { 2, 640, 400, 0 } }, 135);
	LevelScene stink(engine, *xs, smelly, 1);
	stink.Update(0.01f, 0, 0, false);
	Step(stink, 0, -1);
	const auto sounds = stink.TakeSounds();
	Require(std::find(sounds.begin(), sounds.end(), "Sven2/sfx/item_stinky.ogg") != sounds.end(), "Stinky mushroom was not picked up.");
	Require(std::any_of(sounds.begin(), sounds.end(), [](const auto& sound) { return sound == "Sven2/sfx/Sheep_beamup.ogg" || sound == "Sven2/sfx/Sheep_angry2.ogg"; }),
	        "Smell did not emit the sheep's anger reaction.");
	const auto* mood = engine.LoadSprite("Sven2/gfx/symbols.bmp", 12, 1);
	const auto& stinkDraws = stink.Draws();
	Require(std::any_of(stinkDraws.begin(), stinkDraws.end(), [&](const auto& draw) { return draw.Sprite == mood && draw.Y == 160 && draw.Frame >= 10; }),
	        "Smell did not anger the neighboring sheep.");

	const auto cold = FixtureLevel(engine, "frigida", { { 0, 240, 200, 0 }, { 2, 240, 160, 0, 6 } });
	LevelScene frigida(engine, *xs, cold, 1);
	for (i32 frame = 0; frame < 90; ++frame)
		frigida.Update(1.0f / 60, 0, 0, frame % 8 == 0);
	const auto* kettle = engine.LoadSprite("Sven2/gfx/symbol_kettle.bmp", 12, 1);
	const auto& coldDraws = frigida.Draws();
	Require(std::any_of(coldDraws.begin(), coldDraws.end(), [&](const auto& draw) { return draw.Sprite == kettle && draw.Frame > 0; }),
	        "Frigida interaction hid its kettle indicator.");
	const auto kettlePosition = std::find_if(coldDraws.begin(), coldDraws.end(), [&](const auto& draw) { return draw.Sprite == kettle; });
	const auto interactionPosition = std::find_if(coldDraws.begin(), coldDraws.end(), [&](const auto& draw) { return draw.Sprite == frigida.GetPlayerDraw().Sprite; });
	Require(kettlePosition > interactionPosition, "Frigida interaction painted over its kettle indicator.");

	const auto enemy = FixtureLevel(engine, "fight", { { 0, 240, 200, 0 }, { 4, 280, 200, 6 }, { 2, 640, 400, 0 } });
	LevelScene fight(engine, *xs, enemy, 1);
	const auto* cloud = engine.LoadSprite("Sven2/gfx/anim_cloud.bmp", 10, 1);
	bool fought = false;
	for (i32 frame = 0; frame < 600 && !fought; ++frame)
	{
		fight.Update(1.0f / 60, 0, 0, false);
		const auto& draws = fight.Draws();
		fought = std::any_of(draws.begin(), draws.end(), [&](const auto& draw) { return draw.Sprite == cloud; });
	}
	Require(fought, "Enemy battle did not render the fight cloud.");
	const auto animals = FixtureLevel(engine, "animal-meters", { { 0, 240, 200, 0 }, { 12, 240, 160, 0 }, { 11, 400, 160, 0 }, { 13, 560, 160, 0 } });
	LevelScene otherAnimals(engine, *xs, animals, 1);
	const auto& animalDraws = otherAnimals.Draws();
	Require(std::count_if(animalDraws.begin(), animalDraws.end(), [&](const auto& draw) { return draw.Sprite == mood; }) == 3,
	        "Chicken, goat and pig did not all display their impatience meters.");
	otherAnimals.Update(0.1f, 0, 0, true);
	const auto& goatDraws = otherAnimals.Draws();
	const auto goatMeter = std::find_if(goatDraws.begin(), goatDraws.end(), [&](const auto& draw) { return draw.Sprite == mood && draw.Y == 150 && draw.X < 320; });
	const auto goatInteraction = std::find_if(goatDraws.begin(), goatDraws.end(), [&](const auto& draw) { return draw.Sprite == otherAnimals.GetPlayerDraw().Sprite; });
	Require(goatMeter != goatDraws.end() && goatMeter > goatInteraction, "Goat interaction covered or omitted its impatience meter.");
	Log("PASS: run direction, single kick recovery, stable interaction, smell, Frigida kettle and fight animation.");
}

void CheckEnemyAcquisition(MudGE& engine)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack) { return pack.Name == "XS"; });
	auto hasSound = [&](const std::vector<str>& sounds, cstr name) {
		const auto path = engine.GetData().SoundPath(name);
		return !path.empty() && std::find(sounds.begin(), sounds.end(), path) != sounds.end();
	};
	const auto outside = FixtureLevel(engine, "dog-boundary", { { 0, 240, 200, 0 }, { 4, 360, 200, 6 }, { 2, 640, 400, 0 } });
	const auto reservation = FixtureLevel(engine, "player-reservation", { { 0, 240, 200, 0 }, { 5, 240, 240, 0, 6, 0, 0, 0.15f }, { 2, 640, 400, 0 } });
	LevelScene moving(engine, *xs, reservation, 1);
	moving.Update(0.2f, 1, 0, false);
	Require(moving.GetPlayerDraw().X > 260 && moving.GetPlayerDraw().X < 280, "Reservation fixture did not cross the half-step point.");
	const auto* farmerNorth = engine.LoadSprite("Sven2/gfx/Guy/anim_guy_walk_back.bmp", 16, 1);
	const auto* farmerDiagonal = engine.LoadSprite("Sven2/gfx/Guy/anim_guy_walk_diagonal_back.bmp", 16, 1);
	const auto& movingDraws = moving.Draws();
	Require(std::none_of(movingDraws.begin(), movingDraws.end(), [&](const auto& draw) { return draw.Sprite == farmerNorth || (draw.Sprite == farmerDiagonal && draw.Mirror); }),
	        "Enemy entered Sven's committed source or reserved destination during a half-finished step.");
	LevelScene distant(engine, *xs, outside, 1);
	distant.Update(0.1f, 0, 0, false);
	Require(!hasSound(distant.TakeSounds(), "SfxDogFoundSven"), "Dog acquired Sven at the excluded D=3 boundary.");
	const auto inside = FixtureLevel(engine, "dog-notice", { { 0, 240, 200, 0 }, { 4, 320, 200, 6 }, { 2, 640, 400, 0 } });
	LevelScene noticed(engine, *xs, inside, 1);
	noticed.Update(0.01f, 0, 0, false);
	Require(hasSound(noticed.TakeSounds(), "SfxDogFoundSven"), "Dog did not bark on acquiring Sven.");
	noticed.Update(0.2f, 0, 0, false);
	Require(!hasSound(noticed.TakeSounds(), "SfxDogFoundSven"), "Dog repeated its acquisition bark while walking the same tile.");

	const auto boxed = FixtureLevel(engine, "dog-blocked", { { 0, 240, 200, 0 }, { 4, 320, 200, 6 }, { 2, 640, 400, 0 } });
	{
		std::fstream file(engine.GetData().GetRoot() / std::filesystem::u8path(boxed.Map), std::ios::in | std::ios::out | std::ios::binary);
		const i32 dogTile = ((200 + 60) / 40 + 1) * 22 + (320 + 60) / 40 + 1;
		for (i32 dy = -1; dy <= 1; ++dy)
		{
			for (i32 dx = -1; dx <= 1; ++dx)
			{
				if (!dx && !dy)
					continue;
				const i32 tile = dogTile + dy * 22 + dx;
				const i32 wall = 1;
				file.seekp(16 + tile * 24);
				file.write(reinterpret_cast<cstr>(&wall), 4);
			}
		}
		Require(static_cast<bool>(file), "Cannot author blocked-enemy fixture.");
	}
	LevelScene blocked(engine, *xs, boxed, 1);
	blocked.Update(0.01f, 0, 0, false);
	Require(hasSound(blocked.TakeSounds(), "SfxDogFoundSven"), "Wall incorrectly suppressed enemy detection.");
	blocked.Update(2.0f, 0, 0, false);
	Require(!hasSound(blocked.TakeSounds(), "SfxDogFoundSven"), "Blocked enemy ignored its retry delay and repeated notice.");

	const auto bone = FixtureLevel(engine, "dog-bone", { { 0, 240, 200, 0 }, { 4, 320, 200, 6 }, { 9, 320, 240, 0 }, { 2, 640, 400, 0 } });
	LevelScene distracted(engine, *xs, bone, 1);
	distracted.Update(0.01f, 0, 0, false);
	const auto notice = distracted.TakeSounds();
	Require(hasSound(notice, "SfxDogFoundBone") && !hasSound(notice, "SfxDogFoundSven"), "Dog did not prioritize a nearby bone over Sven.");
	distracted.Update(1.0f, 0, 0, false);
	Require(hasSound(distracted.TakeSounds(), "SfxDogDig"), "Dog failed to consume its bone and start digging.");

	const auto beer = FixtureLevel(engine, "farmer-beer", { { 0, 640, 400, 0 }, { 5, 240, 200, 2 }, { 7, 280, 200, 0 }, { 2, 640, 120, 0 } });
	LevelScene drinking(engine, *xs, beer, 1);
	bool foundBeer = false, drank = false;
	for (i32 frame = 0; frame < 60 * 15 && !drank; ++frame)
	{
		drinking.Update(1.0f / 60, 0, 0, false);
		const auto sounds = drinking.TakeSounds();
		foundBeer = foundBeer || hasSound(sounds, "SfxGuyFoundBeer");
		drank = drank || hasSound(sounds, "SfxLarsDrink");
	}
	Require(foundBeer && drank, "Farmer did not notice and drink the bottle with its original sounds.");
	drinking.Update(12.0f, 0, 0, false);
	const auto sleepSounds = drinking.TakeSounds();
	Require(hasSound(sleepSounds, "SfxLarsFall") && hasSound(sleepSounds, "SfxLarsSleep"), "Farmer's drinking-to-sleep transition omitted its fall or snore sound.");

	const auto idle = FixtureLevel(engine, "farmer-idle", { { 0, 240, 200, 0 }, { 5, 320, 200, 6 }, { 2, 640, 400, 0 } });
	LevelScene farmer(engine, *xs, idle, 1);
	farmer.Update(0.1f, 0, 0, false);
	Require(!hasSound(farmer.TakeSounds(), "SfxGuyFoundSven"), "Awake farmer used a dog-style proximity acquisition.");
	const auto active = FixtureLevel(engine, "farmer-activity", { { 0, 240, 200, 0 }, { 2, 240, 160, 0 }, { 2, 640, 400, 0 }, { 5, 640, 40, 4 } });
	LevelScene activity(engine, *xs, active, 1);
	activity.Update(0.01f, 0, 0, true);
	activity.Update(0.02f, 0, 0, false);
	Require(hasSound(activity.TakeSounds(), "SfxGuyFoundSven"), "Farmer ignored Sven's interaction outside proximity range.");
	for (i32 kind : { 3, 6 })
	{
		cstr sound = kind == 3 ? "SfxAlienFoundSven" : "SfxBroemseFoundSven";
		const auto nearby = FixtureLevel(engine, "notice-" + std::to_string(kind), { { 0, 240, 200, 0 }, { kind, 360, 200, 6 }, { 2, 640, 400, 0 } });
		LevelScene acquisition(engine, *xs, nearby, 1);
		acquisition.Update(0.01f, 0, 0, false);
		Require(hasSound(acquisition.TakeSounds(), sound), "Alien or girl did not notice Sven inside its original square range.");
		const auto excluded = FixtureLevel(engine, "boundary-" + std::to_string(kind), { { 0, 240, 200, 0 }, { kind, 400, 200, 6 }, { 2, 640, 400, 0 } });
		LevelScene boundary(engine, *xs, excluded, 1);
		boundary.Update(0.01f, 0, 0, false);
		Require(!hasSound(boundary.TakeSounds(), sound), "Alien or girl noticed Sven at the excluded D=4 boundary.");
	}
	Log("PASS: strict dog detection range, one-shot notice, bone priority/digging, farmer drink and activity detection.");
}

void CheckRound(MudGE& engine)
{
	const auto& getPacks = engine.GetData().GetPacks();
	auto pack = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& item) { return item.Name == "XS"; });
	Require(pack != getPacks.end(), "XS package missing.");
	GameSession game(engine, *pack, pack->LevelSets.front());
	DriveTutorial(engine, [&](f32 dt, i32 dx, i32 dy, bool action) { game.Update(dt, dx, dy, action); }, [&]() -> const auto& { return game.Draws(); }, [&] { return game.GetRemainingTargets(); });
	Require(!game.GetResults(), "Level results appeared before the original completion delay.");
	game.Update(1.51f, 0, 0, false);
	Require(game.GetResults() && !game.Ready() && !game.GameOver(), "Completed tutorial did not enter bonus counting.");
	const i32 expected = game.GetScore() + game.GetTimeBonus() + 30 + 75;
	game.SkipBonuses();
	Require(game.Ready() && game.GetScore() == expected, "Time, sheep and lives bonuses were not converted correctly.");
	Require(!game.TakeSounds().empty(), "Round did not emit audio events.");
	const i32 episodeScore = game.GetScore();
	game.Advance();
	Require(!game.GetResults() && game.LevelNumber() == 2 && game.GetScore() == episodeScore, "Level transition reset the episode score.");
	Log("PASS: complete tutorial round, result transition and exact time/sheep/lives bonus totals.");
	LevelSetInfo finalSet{ "Single tutorial", { pack->LevelSets.front().Levels.front() } };
	GameSession finalLevel(engine, *pack, finalSet);
	DriveTutorial(engine, [&](f32 dt, i32 dx, i32 dy, bool action) { finalLevel.Update(dt, dx, dy, action); }, [&]() -> const auto& { return finalLevel.Draws(); }, [&] { return finalLevel.GetRemainingTargets(); });
	finalLevel.Update(1.51f, 0, 0, false);
	Require(finalLevel.GetResults() && !finalLevel.GetResult().EpisodeComplete, "Final level skipped its bonus screen.");
	finalLevel.SkipBonuses();
	Require(finalLevel.Ready() && !finalLevel.Finished(), "Final bonus completion prematurely opened the pack screen.");
	finalLevel.TakeSounds();
	finalLevel.Advance();
	const auto sounds = finalLevel.TakeSounds();
	Require(finalLevel.Finished() && finalLevel.GetResult().EpisodeComplete && std::find(sounds.begin(), sounds.end(), engine.GetData().SoundPath("SfxFinishPack")) != sounds.end(), "Acknowledging the final bonus did not open pack completion with its sound.");
	Log("PASS: final level bonus, acknowledgement and separate pack-completion transition.");
}

void CheckLifeChanges(MudGE& engine, bool capture = false)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack) { return pack.Name == "XS"; });
	const auto arena = FixtureLevel(engine, "life-alien", { { 0, 240, 200, 0 }, { 3, 280, 200, 6 }, { 2, 640, 400, 0 } });
	const auto* icon = engine.LoadSprite("Sven2/gfx/Life_icon.bmp", 1, 1);
	const auto newLife = engine.GetData().SoundPath("SfxNewLife");
	const auto loseLife = engine.GetData().SoundPath("SfxLoseLife");
	LevelSetInfo freshSet{ "Zero score", { arena } };
	GameSession fresh(engine, *xs, freshSet);
	bool refunded = false;
	for (i32 frame = 0; frame < 60 * 60 && !refunded; ++frame)
	{
		fresh.Update(1.0f / 60, 0, 0, false);
		const auto sounds = fresh.TakeSounds();
		refunded = std::find(sounds.begin(), sounds.end(), newLife) != sounds.end();
	}
	Require(refunded && fresh.GetLives() == 3, "Original zero-score first-hit refund was not preserved.");
	fresh.Update(0.2f, 0, 0, false);
	const auto& graceDraws = fresh.Draws();
	Require(std::count_if(graceDraws.begin(), graceDraws.end(), [&](const auto& draw) { return draw.Sprite == icon && draw.Y == 565; }) == 2,
	        "Refund did not temporarily reduce the displayed life count.");
	Require(std::any_of(graceDraws.begin(), graceDraws.end(), [&](const auto& draw) { return draw.Sprite == icon && draw.Y < 500; }),
	        "Refund did not spawn the incoming life icon.");
	if (capture)
		engine.DrawFrame(graceDraws, "life-refund.bmp");

	auto setup = xs->LevelSets.front().Levels.front();
	auto bytes = engine.ReadAsset(setup.Map);
	u32 nameLength = 0;
	std::memcpy(&nameLength, bytes.data() + 12, 4);
	const f32 timer = 800;
	std::memcpy(bytes.data() + 16 + nameLength + 9044, &timer, 4);
	setup.Map = "XS/levels/check-life-score.lvl";
	{
		std::ofstream file(engine.GetData().GetRoot() / std::filesystem::u8path(setup.Map), std::ios::binary);
		file.write(reinterpret_cast<cstr>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		Require(static_cast<bool>(file), "Cannot create life-score setup.");
	}
	LevelSetInfo campaign{ "Lives", { setup, arena } };
	GameSession game(engine, *xs, campaign);
	DriveTutorial(engine, [&](f32 dt, i32 dx, i32 dy, bool action) { game.Update(dt, dx, dy, action); }, [&]() -> const auto& { return game.Draws(); }, [&] { return game.GetRemainingTargets(); });
	Require(game.GetRemainingTargets() == 0, "Life setup tutorial did not finish.");
	game.Update(1.51f, 0, 0, false);
	game.SkipBonuses();
	Require(game.Ready() && game.GetScore() > 750, "Life setup did not cross the original score threshold.");
	game.Advance();
	game.Update(1.0f / 60, 0, 0, false);
	Require(game.GetLives() == 4, "Campaign score did not grant the next level's extra life.");
	const auto& gainDraws = game.Draws();
	Require(std::count_if(gainDraws.begin(), gainDraws.end(), [&](const auto& draw) { return draw.Sprite == icon && draw.Y == 565; }) == 3,
	        "Extra life appeared in the HUD before its animation arrived.");
	Require(std::any_of(gainDraws.begin(), gainDraws.end(), [&](const auto& draw) { return draw.Sprite == icon && draw.X == 96 && draw.Y < 0; }),
	        "Extra life did not start falling from above the screen.");
	game.Update(0.35f, 0, 0, false);
	if (capture)
		engine.DrawFrame(game.Draws(), "life-gain.bmp");
	game.Update(2.0f, 0, 0, false);
	const auto& landed = game.Draws();
	Require(std::count_if(landed.begin(), landed.end(), [&](const auto& draw) { return draw.Sprite == icon && draw.Y == 565; }) == 4,
	        "Incoming life did not join the HUD on landing.");
	game.TakeSounds();
	bool lost = false;
	for (i32 frame = 0; frame < 60 * 60 && !lost; ++frame)
	{
		game.Update(1.0f / 60, 0, 0, false);
		const auto sounds = game.TakeSounds();
		lost = std::find(sounds.begin(), sounds.end(), loseLife) != sounds.end();
	}
	Require(lost && game.GetLives() == 3, "Alien attack failed to take a logical life after campaign score carried over.");
	game.Update(0.2f, 0, 0, false);
	const auto& lossDraws = game.Draws();
	Require(std::count_if(lossDraws.begin(), lossDraws.end(), [&](const auto& draw) { return draw.Sprite == icon && draw.Y == 565; }) == 3,
	        "Alien life loss did not reduce the stationary HUD icons.");
	if (capture)
		engine.DrawFrame(lossDraws, "life-loss.bmp");
	Log("PASS: zero-score refund animation, campaign extra-life drop/landing, alien logical loss and outgoing icon.");
}

void CheckPlayerItems(MudGE& engine, bool capture = false)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack) { return pack.Name == "XS"; });
	struct Powerup
	{
		i32 Code;
		cstr Effect;
		i32 Frames;
		cstr Sound;
		cstr Duration;
		i32 Distance;
	};
	const Powerup items[] = {
		{ 132, "symbol_power.bmp", 13, "SfxFirework", "FireworkDuration", 22 },
		{ 134, "symbol_bubbles_good.bmp", 6, "SfxImpotence", "ImpotentDuration", 9 },
		{ 135, "symbol_smell.bmp", 14, "SfxSmell", "SmellDuration", 14 },
		{ 136, "symbol_flowers.bmp", 8, "SfxRose", "FlowerDuration", 14 },
	};
	for (const auto& item : items)
	{
		const auto level = FixtureLevel(engine, "pickup-" + std::to_string(item.Code), { { 0, 240, 200, 0 }, { 2, 640, 400, 0 } }, item.Code);
		LevelScene scene(engine, *xs, level, 1);
		scene.Update(0.01f, 0, 0, false);
		const auto sounds = scene.TakeSounds();
		Require(std::any_of(sounds.begin(), sounds.end(), [&](const auto& sound) { return _stricmp(sound.c_str(), engine.GetData().SoundPath(item.Sound).c_str()) == 0; }),
		        "Player pickup omitted its original sound.");
		const auto* effect = engine.LoadSprite(str("Sven2/gfx/") + item.Effect, static_cast<i16>(item.Frames), 1);
		const auto& draws = scene.Draws();
		Require(std::any_of(draws.begin(), draws.end(), [&](const auto& draw) { return draw.Sprite == effect; }),
		        "Player pickup omitted its visual effect.");
		if (capture && item.Code == 134)
			engine.DrawFrame(draws, "slow-motion.bmp");
		scene.Update(0.1f, 1, 0, false);
		Require(scene.GetPlayerDraw().X >= 240 + item.Distance && scene.GetPlayerDraw().X <= 241 + item.Distance, "Player pickup did not apply the original movement-speed modifier.");
		scene.Update(static_cast<f32>(engine.GetData().NumericParam(item.Duration, 20)) + 1, 0, 0, false);
		const auto& expired = scene.Draws();
		Require(std::none_of(expired.begin(), expired.end(), [&](const auto& draw) { return draw.Sprite == effect; }),
		        "Player pickup effect outlived its configured duration.");
	}
	const auto gift = FixtureLevel(engine, "gift-pickup", { { 0, 240, 200, 0 }, { 2, 640, 400, 0 } }, 133);
	for (u32 seed = 1; seed <= 64; ++seed)
	{
		LevelScene scene(engine, *xs, gift, seed);
		scene.Update(0.01f, 0, 0, false);
		const i32 lives = scene.TakeLivesGained();
		const f32 time = scene.TakeTimeDelta();
		Require((lives == 1 && time == 0) || (lives == 0 && (time == 5 || time == 10 || time == 15 || time == 25)), "Gift pickup awarded a reward outside the original nested random branches.");
		Require(scene.TakeScoreDelta() == 0, "Gift pickup invented a score award.");
		scene.Update(0.1f, 0, 0, false);
		Require(scene.TakeLivesGained() == 0 && scene.TakeTimeDelta() == 0, "Gift was consumed more than once.");
	}
	Log("PASS: all four timed player powerups, speed/effect expiry, and 64 gift reward/consume scenarios.");
}

void CheckAttackReservations(MudGE& engine)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack) { return pack.Name == "XS"; });
	const auto level = FixtureLevel(engine, "attack-reservation", { { 0, 240, 120, 4 }, { 4, 240, 40, 4 }, { 2, 640, 400, 0 } }, 134);
	bool exercised = false;
	for (u32 seed = 1; seed <= 32 && !exercised; ++seed)
	{
		LevelScene scene(engine, *xs, level, seed);
		bool interrupted = false;
		for (i32 frame = 0; frame < 600; ++frame)
		{
			const auto previous = scene.GetPlayerDraw();
			scene.Update(1.0f / 60, 1, 0, false);
			for (const auto& sound : scene.TakeSounds())
				if (sound.find("fightdog.ogg") != str::npos && previous.X % 40 != 0 && previous.X < 640)
					interrupted = true;
			if (scene.TakeLifeHits() > 0)
			{
				if (interrupted)
				{
					const auto before = scene.GetPlayerDraw();
					scene.Update(0.5f, 1, 0, false);
					Require(scene.GetPlayerDraw().X > before.X + 20, "An interrupted step left an impassable player reservation after the attack.");
					exercised = true;
				}
				break;
			}
		}
	}
	Require(exercised, "Attack reservation scenario did not exercise a mid-step capture.");
	Log("PASS: mid-step enemy capture releases the cancelled player destination after recovery.");
}

void CheckEffectAndAnimationStates(MudGE& engine, bool capture = false)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack) { return pack.Name == "XS"; });
	auto contains = [](const std::vector<SpriteDraw>& draws, const Sprite* sprite) {
		return std::any_of(draws.begin(), draws.end(), [&](const auto& draw) { return draw.Sprite == sprite; });
	};
	const auto cold = FixtureLevel(engine, "frigida-continuation", { { 0, 240, 200, 0 }, { 2, 240, 160, 0 } });
	LevelScene frigida(engine, *xs, cold, 1);
	const auto* combined = engine.LoadSprite("Sven2/gfx/Frigida/anim_frigida_final_normal_back.bmp", 3, 1);
	frigida.Update(0, 0, 0, true);
	Require(frigida.GetPlayerDraw().Sprite == combined, "Frigida's initial frame exposed standing Sven.");
	for (i32 frame = 0; frame < 240 && !frigida.Completed(); ++frame)
	{
		frigida.Update(1.0f / 60, 0, 0, frame % 2 == 1);
		if (!frigida.Completed())
			Require(frigida.GetPlayerDraw().Sprite == combined, "Queued Frigida continuation flickered to standing Sven.");
	}

	const auto artifact = FixtureLevel(engine, "artifact-transform", { { 0, 240, 200, 0 }, { 2, 640, 400, 0 } }, 128);
	LevelScene alienPower(engine, *xs, artifact, 1);
	Require(!alienPower.UfoActive(), "Artifact UFO was active before pickup.");
	const auto* alienIcon = engine.LoadSprite("Sven2/gfx/icon_alien.bmp", 13, 1);
	bool ascended = false, hidden = false, landed = false;
	for (i32 frame = 0; frame < 60 * 60 && !landed; ++frame)
	{
		alienPower.Update(1.0f / 60, 0, 0, false);
		if (frame == 0)
			Require(alienPower.UfoActive(), "Artifact pickup did not begin the UFO audio lifecycle.");
		const auto hero = alienPower.GetPlayerDraw();
		ascended = ascended || (hero.Sprite && hero.Y < 200);
		hidden = hidden || !hero.Sprite;
		const auto& draws = alienPower.Draws();
		Require(std::all_of(draws.begin(), draws.end(), [](const auto& draw) { return draw.Sprite != nullptr; }),
		        "Hidden artifact phase published a null drawing sprite.");
		if (contains(draws, alienIcon))
		{
			Require(hero.X == 240 && hero.Y == 200, "Artifact beam moved Sven's ground position to another tile.");
			Require(std::any_of(draws.begin(), draws.end(), [&](const auto& draw) { return draw.Sprite == alienIcon && draw.X == 268 && draw.Y == 196; }),
			        "Alien powerup used the wrong icon geometry or following position.");
			landed = true;
			if (capture)
				engine.DrawFrame(draws, "artifact-landed.bmp");
		}
	}
	Require(ascended && hidden && landed && alienPower.TakeLifeHits() == 0, "Artifact ascent/wait/landing sequence was incomplete.");
	Require(alienPower.UfoActive(), "UFO lifecycle ended before departure.");
	alienPower.Update(16.0f, 0, 0, false);
	Require(!alienPower.UfoActive(), "UFO lifecycle stayed active after departure.");
	Require(!contains(alienPower.Draws(), alienIcon), "Alien powerup did not expire.");
	const auto movingArtifact = FixtureLevel(engine, "artifact-walking", { { 0, 240, 240, 0 }, { 2, 640, 400, 0 } });
	{
		std::fstream file(engine.GetData().GetRoot() / std::filesystem::u8path(movingArtifact.Map), std::ios::in | std::ios::out | std::ios::binary);
		const i32 tile = ((200 + 60) / 40 + 1) * 22 + (240 + 60) / 40 + 1;
		const char artifactCode = static_cast<char>(128);
		file.seekp(16 + tile * 24 + 12);
		file.write(&artifactCode, 1);
		Require(static_cast<bool>(file), "Cannot author moving artifact fixture.");
	}
	LevelScene movingPower(engine, *xs, movingArtifact, 1);
	Step(movingPower, 0, -1);
	Require(movingPower.GetPlayerDraw().Y == 200, "Moving artifact fixture did not reach the pickup tile.");
	bool movingLanded = false;
	for (i32 frame = 0; frame < 60 * 60 && !movingLanded; ++frame)
	{
		movingPower.Update(1.0f / 60, 0, 0, false);
		movingLanded = contains(movingPower.Draws(), alienIcon);
	}
	Require(movingLanded && movingPower.GetPlayerDraw().X == 240 && movingPower.GetPlayerDraw().Y == 200, "Artifact collected while walking returned Sven to the wrong committed tile.");

	const auto bone = FixtureLevel(engine, "dig-particles", { { 0, 640, 400, 0 }, { 4, 240, 200, 2 }, { 9, 280, 200, 0 }, { 2, 640, 120, 0 } });
	LevelScene dog(engine, *xs, bone, 1);
	const auto* dirt = engine.LoadSprite("Sven2/gfx/Digging.bmp", 18, 1);
	bool dug = false;
	for (i32 frame = 0; frame < 600 && !dug; ++frame)
	{
		dog.Update(1.0f / 60, 0, 0, false);
		dug = contains(dog.Draws(), dirt);
	}
	Require(dug, "Bone consumption omitted its digging particles.");
	if (capture)
		engine.DrawFrame(dog.Draws(), "dog-digging.bmp");
	dog.Update(10.0f, 0, 0, false);
	Require(contains(dog.Draws(), dirt), "Dig effect did not preserve its original hold/replay lifetime.");
	const auto* dogRest = engine.LoadSprite("Sven2/gfx/Dog/anim_dog_sleep_front.bmp", 4, 1);
	Require(contains(dog.Draws(), dogRest), "Dog stopped digging before the configured 15 seconds.");
	dog.Update(4.5f, 0, 0, false);
	Require(!contains(dog.Draws(), dirt), "Dig effect failed to finish its second pass.");
	dog.Update(0.7f, 0, 0, false);
	Require(!contains(dog.Draws(), dogRest), "Dog remained in the digging state beyond its configured timer.");

	const auto sleeping = FixtureLevel(engine, "farmer-zzz", { { 0, 640, 400, 0 }, { 5, 240, 200, 0, 6, 0, 10, 20 }, { 2, 640, 120, 0 } });
	LevelScene farmer(engine, *xs, sleeping, 1);
	const auto* zzz = engine.LoadSprite("Sven2/gfx/Guy/anim_guy_gfx_zzz.bmp", 13, 1);
	farmer.Update(0.1f, 0, 0, false);
	Require(contains(farmer.Draws(), zzz), "Sleeping farmer omitted the Zzz overlay.");
	if (capture)
		engine.DrawFrame(farmer.Draws(), "farmer-sleeping.bmp");
	const auto sleepingDog = FixtureLevel(engine, "dog-zzz", { { 0, 640, 400, 0 }, { 4, 240, 200, 0, 6, 0, 10, 20 }, { 2, 640, 120, 0 } });
	LevelScene dogSleep(engine, *xs, sleepingDog, 1);
	dogSleep.Update(0.1f, 0, 0, false);
	Require(contains(dogSleep.Draws(), zzz), "Sleeping dog omitted its Zzz overlay.");
	const auto wakingDog = FixtureLevel(engine, "dog-wake", { { 0, 240, 200, 0 }, { 4, 320, 200, 0, 6, 0, 10, 20 }, { 2, 640, 120, 0 } });
	LevelScene dogWake(engine, *xs, wakingDog, 1);
	dogWake.Update(0.1f, 0, 0, false);
	Require(!contains(dogWake.Draws(), zzz), "Dog did not wake when Sven entered its original wake range.");

	const auto girlLevel = FixtureLevel(engine, "girl-combined", { { 0, 240, 200, 0 }, { 6, 240, 120, 4 }, { 2, 640, 400, 0 } });
	LevelScene girl(engine, *xs, girlLevel, 1);
	const auto* girlFront = engine.LoadSprite("Sven2/gfx/Broemse/anim_broemse_beating_sven_front.bmp", 35, 1);
	const auto* svenStand = engine.LoadSprite("Sven2/gfx/Sven/sven_stand.bmp", 8, 1);
	bool played = false;
	i32 maximumFrame = 0;
	f32 time = 0;
	for (i32 frame = 0; frame < 900; ++frame)
	{
		girl.Update(1.0f / 60, 0, 0, false);
		time += girl.TakeTimeDelta();
		const auto& draws = girl.Draws();
		if (contains(draws, girlFront))
		{
			if (!played && capture)
				engine.DrawFrame(draws, "girl-playing.bmp");
			played = true;
			Require(!contains(draws, svenStand), "Girl interaction showed standing Sven alongside the combined animation.");
			for (const auto& draw : draws)
				if (draw.Sprite == girlFront)
				{
					Require(draw.Sprite->FrameHeight == 120, "Girl front animation was sliced into partial rows.");
					maximumFrame = std::max(maximumFrame, draw.Frame);
				}
		}
		else if (played)
			break;
	}
	Require(played && maximumFrame == 34 && time == -20 && girl.TakeLifeHits() == 0, "Girl interaction failed its complete 35-frame front sheet or time-only penalty.");
	Log("PASS: Frigida continuity, artifact ground/icon recovery, dig particles, sleep Zzz and complete girl interaction.");
}

void CheckEnemyHeadings(MudGE& engine, bool capture = false)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack) { return pack.Name == "XS"; });
	constexpr i32 directions[][2] = { { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 } };
	constexpr bool mirrors[4][8] = { { 0, 1, 1, 1, 0, 0, 0, 0 }, { 0, 1, 0, 0, 0, 1, 1, 0 }, { 0, 1, 0, 0, 0, 1, 1, 0 }, { 0, 1, 1, 1, 0, 0, 0, 0 } };
	cstr family[] = { "Alien", "Dog", "Guy", "Broemse" };
	cstr lower[] = { "alien", "dog", "guy", "broemse" };
	cstr headings[] = { "back", "diagonal_back", "side", "diagonal_front", "front", "diagonal_front", "side", "diagonal_back" };
	constexpr i16 rows[] = { 12, 16, 16, 8 };
	BitmapFont labels(engine, true);
	cstr directionNames[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
	for (i32 kind = 3; kind <= 6; ++kind)
	{
		std::array<const Sprite*, 8> sprites{};
		for (i32 d = 0; d < 8; ++d)
			sprites[d] = engine.LoadSprite(str("Sven2/gfx/") + family[kind - 3] + "/anim_" + lower[kind - 3] + "_walk_" + headings[d] + ".bmp", rows[kind - 3], 1);
		std::set<i32> covered;
		std::vector<SpriteDraw> montage;
		for (i32 facing = 0; facing < 8; ++facing)
		{
			for (u32 seed = 1; seed <= 4; ++seed)
			{
				const auto level = FixtureLevel(engine, "heading-" + std::to_string(kind) + "-" + std::to_string(facing),
				                                { { 0, 640, 400, 0 }, { kind, 240, 120, facing, 6, 0, 0, 0 }, { 2, 640, 480, 0 } });
				LevelScene scene(engine, *xs, level, seed);
				SpriteDraw before{};
				bool walking = false;
				for (i32 frame = 0; frame < 600 && !walking; ++frame)
				{
					scene.Update(1.0f / 60, 0, 0, false);
					for (const auto& draw : scene.Draws())
						if (std::find(sprites.begin(), sprites.end(), draw.Sprite) != sprites.end())
						{
							before = draw;
							walking = true;
							break;
						}
				}
				Require(walking, "Heading fixture never entered its first walking tile.");
				scene.Update(0.05f, 0, 0, false);
				const auto& draws = scene.Draws();
				const auto after = std::find_if(draws.begin(), draws.end(), [&](const auto& draw) { return std::find(sprites.begin(), sprites.end(), draw.Sprite) != sprites.end(); });
				Require(after != draws.end(), "Heading fixture left walking before its first short step.");
				const i32 dx = (after->X > before.X) - (after->X < before.X), dy = (after->Y > before.Y) - (after->Y < before.Y);
				i32 direction = 0;
				while (direction < 8 && (dx != directions[direction][0] || dy != directions[direction][1]))
					++direction;
				Require(direction < 8 && after->Sprite == sprites[direction] && after->Mirror == mirrors[kind - 3][direction], "Enemy walking sprite or mirroring disagreed with its actual movement direction.");
				if (capture && covered.find(direction) == covered.end())
				{
					if (montage.empty())
						montage.push_back(draws.front());
					auto posed = *after;
					posed.X = (direction % 4) * 200 + 40;
					posed.Y = (direction / 4) * 240 + 80;
					montage.push_back(posed);
					labels.Append(montage, directionNames[direction], (direction % 4) * 200 + 60, (direction / 4) * 240 + 40);
				}
				covered.insert(direction);
			}
		}
		Require(covered.size() == 8, "Heading regression did not cover all eight movement directions.");
		if (capture)
			engine.DrawFrame(montage, (str("directions-") + lower[kind - 3] + ".bmp").c_str());
	}
	Log("PASS: all eight authored walk directions and companion mirrors for all four enemy families.");
}

void CheckEnemyItems(MudGE& engine, bool capture = false)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack) { return pack.Name == "XS"; });
	auto heard = [&](const std::vector<str>& sounds, cstr name) {
		const auto path = engine.GetData().SoundPath(name);
		return std::any_of(sounds.begin(), sounds.end(), [&](const auto& sound) { return _stricmp(sound.c_str(), path.c_str()) == 0; });
	};
	const auto teddy = FixtureLevel(engine, "girl-teddy", { { 0, 640, 400, 0 }, { 6, 240, 200, 2 }, { 8, 280, 200, 0 }, { 2, 640, 120, 0 } });
	LevelScene girl(engine, *xs, teddy, 1);
	bool played = false;
	for (i32 frame = 0; frame < 1200 && !played; ++frame)
	{
		girl.Update(1.0f / 60, 0, 0, false);
		played = heard(girl.TakeSounds(), "SfxBroemsePlaysTeddy");
	}
	Require(played, "Girl did not consume her teddy and play its interaction sound.");
	girl.Update(20.0f, 0, 0, false);
	const auto* teddyFront = engine.LoadSprite("Sven2/gfx/Broemse/anim_broemse_beating_teddybear_diagonal_front.bmp", 14, 1);
	const auto* teddyBack = engine.LoadSprite("Sven2/gfx/Broemse/anim_broemse_beating_teddybear_diagonal_back.bmp", 9, 1);
	const auto& recovered = girl.Draws();
	Require(std::none_of(recovered.begin(), recovered.end(), [&](const auto& draw) { return draw.Sprite == teddyFront || draw.Sprite == teddyBack; }),
	        "Girl remained in her teddy interaction after the original sequence completed.");
	const auto playDog = FixtureLevel(engine, "girl-dog", { { 0, 640, 400, 0 }, { 6, 240, 120, 4 }, { 4, 240, 200, 0 }, { 2, 640, 480, 0 } });
	bool dogInteraction = false;
	for (u32 seed = 1; seed <= 16 && !dogInteraction; ++seed)
	{
		LevelScene pair(engine, *xs, playDog, seed);
		for (i32 frame = 0; frame < 1200 && !dogInteraction; ++frame)
		{
			pair.Update(1.0f / 60, 0, 0, false);
			dogInteraction = heard(pair.TakeSounds(), "SfxBroemsePlaysWotan");
			if (dogInteraction)
			{
				const auto* dogStand = engine.LoadSprite("Sven2/gfx/Dog/Dog_Stand.bmp", 8, 1);
				const auto& draws = pair.Draws();
				Require(std::none_of(draws.begin(), draws.end(), [&](const auto& draw) { return draw.Sprite == dogStand; }),
				        "Girl-dog interaction retained a separate standing dog.");
			}
		}
	}
	Require(dogInteraction, "Girl did not enter the dog interaction in the authored encounter.");
	const auto* artifactSprite = engine.LoadSprite("Sven2/gfx/Artifact.bmp", 1, 1);
	for (i32 kind = 3; kind <= 6; ++kind)
	{
		const auto level = FixtureLevel(engine, "artifact-enemy-" + std::to_string(kind),
		                                { { 0, 640, 400, 0 }, { kind, 240, 200, 2, 6, 0, 0, 0 }, { 10, 280, 200, 0 }, { 2, 640, 120, 0 } });
		bool beamed = false;
		for (u32 seed = 1; seed <= 16 && !beamed; ++seed)
		{
			LevelScene scene(engine, *xs, level, seed);
			for (i32 frame = 0; frame < 3600 && !beamed; ++frame)
			{
				scene.Update(1.0f / 60, 0, 0, false);
				beamed = heard(scene.TakeSounds(), "SfxBeam");
				if (beamed)
				{
					const auto& draws = scene.Draws();
					Require(std::none_of(draws.begin(), draws.end(), [&](const auto& draw) { return draw.Sprite == artifactSprite; }),
					        "Enemy artifact pickup left the collectible behind.");
					for (const auto* path : { "Sven2/gfx/ufo.bmp", "Sven2/gfx/ufo_shadow.bmp", "Sven2/gfx/anim_beam.bmp" })
					{
						const auto* image = engine.LoadSprite(path, std::strstr(path, "anim_beam") ? 44 : 13, 1);
						Require(std::any_of(draws.begin(), draws.end(), [&](const auto& draw) { return draw.Sprite == image; }),
						        "Enemy artifact sequence omitted visible UFO, shadow or beam.");
					}
					Require(scene.TakeLifeHits() == 0, "Enemy artifact pickup caused a player life hit.");
					if (capture)
						engine.DrawFrame(draws, ("artifact-ufo-" + std::to_string(kind) + ".bmp").c_str());
				}
			}
		}
		Require(beamed, "Enemy failed to consume an artifact and enter its UFO interaction.");
	}
	Log("PASS: girl teddy recovery, girl-dog combined interaction and artifact consumption by all four enemy families.");
}

void CheckDemonSheep(MudGE& engine, bool capture = false)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack) { return pack.Name == "XS"; });
	const auto* back = engine.LoadSprite("Sven2/gfx/Sheep/anim_sheep_demon_back.bmp", 23, 1);
	const auto* front = engine.LoadSprite("Sven2/gfx/Sheep/anim_sheep_demon_front.bmp", 23, 1);
	const auto* run = engine.LoadSprite("Sven2/gfx/Sheep/anim_sheep_demon_run_front.bmp", 8, 1);
	const auto* flash = engine.LoadSprite("Sven2/gfx/flash.bmp", 2, 1);
	const auto* mood = engine.LoadSprite("Sven2/gfx/symbols.bmp", 12, 1);
	const auto* stand = engine.LoadSprite("Sven2/gfx/Sheep/Sheep_Stand.bmp", 4, 1);
	const auto* power = engine.LoadSprite("Sven2/gfx/symbol_power.bmp", 13, 1);
	const auto devil = engine.GetData().SoundPath("SfxDevil");
	const auto electro = engine.GetData().SoundPath("SfxDevilElectro");
	auto contains = [](const auto& draws, const auto* sprite) {
		return std::any_of(draws.begin(), draws.end(), [&](const auto& draw) { return draw.Sprite == sprite; });
	};
	const auto natural = FixtureLevel(engine, "demon-natural", { { 0, 80, 240, 0 }, { 2, 480, 160, 0, 10 }, { 2, 480, 320, 0, 10 } });
	bool triggered = false;
	for (u32 seed = 1; seed <= 4 && !triggered; ++seed)
	{
		LevelScene scene(engine, *xs, natural, seed);
		for (i32 frame = 0; frame < 60 * 120 && !triggered; ++frame)
		{
			scene.Update(1.0f / 60, 0, 0, false);
			const auto sounds = scene.TakeSounds();
			triggered = std::find(sounds.begin(), sounds.end(), devil) != sounds.end();
			if (triggered)
			{
				Require(frame > 60 * 17, "Demon triggered before the 41-pulse impatience interval.");
				Require(contains(scene.Draws(), back) || contains(scene.Draws(), front), "Natural demon trigger omitted transformation artwork.");
				if (capture)
				{
					scene.Update(0.8f, 0, 0, false);
					engine.DrawFrame(scene.Draws(), "demon-transform.bmp");
				}
			}
		}
	}
	Require(triggered, "Maximum impatience never reached the original random demon branch.");
	const auto chance = FixtureLevel(engine, "demon-chance", { { 0, 80, 240, 0 }, { 2, 480, 160, 0, 10, 0, 0, 0, 40 }, { 2, 480, 320, 0, 10, 0, 0, 0, 40 } });
	bool succeeded = false, failed = false;
	for (u32 seed = 1; seed <= 20; ++seed)
	{
		LevelScene scene(engine, *xs, chance, seed);
		scene.Update(0.01f, 0, 0, false);
		const auto sounds = scene.TakeSounds();
		const bool devilSound = std::find(sounds.begin(), sounds.end(), devil) != sounds.end();
		succeeded = succeeded || devilSound;
		failed = failed || !devilSound;
		if (!devilSound)
		{
			const auto& draws = scene.Draws();
			Require(std::all_of(draws.begin(), draws.end(), [&](const auto& draw) { return draw.Sprite != mood || draw.Frame == 11; }),
			        "Failed demon attempt did not cap impatience back to 10.");
		}
	}
	Require(succeeded && failed, "Demon chance did not exercise both original random outcomes.");
	const auto protectedLevel = FixtureLevel(engine, "demon-frigida-protected", { { 0, 80, 240, 0 }, { 2, 480, 160, 0, 10, 2, 0, 0, 40 }, { 2, 480, 320, 0, 0 } });
	LevelScene protectedSheep(engine, *xs, protectedLevel, 1);
	protectedSheep.Update(30, 0, 0, false);
	const auto protectedSounds = protectedSheep.TakeSounds();
	Require(std::find(protectedSounds.begin(), protectedSounds.end(), devil) == protectedSounds.end(), "Frigida incorrectly entered the ordinary demon branch.");
	for (bool afterStep : { false, true })
	{
		const auto level = FixtureLevel(engine, afterStep ? "demon-moving-shock" : "demon-direct-shock",
		                                { { 0, 240, 200, 0 }, { 2, afterStep ? 160 : 200, afterStep ? 160 : 200, afterStep ? 1 : 0, 12, 0, 11 }, { 2, 640, 400, 0 } }, 132);
		LevelScene scene(engine, *xs, level, 1);
		bool shocked = false, sawRun = false;
		for (i32 frame = 0; frame < 600 && !shocked; ++frame)
		{
			scene.Update(1.0f / 60, 0, 0, false);
			sawRun = sawRun || contains(scene.Draws(), run);
			const auto sounds = scene.TakeSounds();
			shocked = std::find(sounds.begin(), sounds.end(), electro) != sounds.end();
		}
		Require(shocked, "Demon did not acquire Sven in its four-way facing arc.");
		Require(scene.TakeTimeDelta() == (afterStep ? -15 : 10), "Demon time change disagreed with its original direct/moving branch.");
		Require(scene.TakeLifeHits() == 0, "Demon shock invented a life penalty.");
		Require(!contains(scene.Draws(), power) && contains(scene.Draws(), flash), "Demon shock failed to clear the powerup or emit the flash.");
		if (afterStep)
			Require(sawRun, "Demon did not use the run animation before moving contact.");
		if (capture && afterStep)
			engine.DrawFrame(scene.Draws(), "demon-shock.bmp");
		const auto stopped = scene.GetPlayerDraw();
		scene.Update(0.2f, 1, 0, true);
		Require(scene.GetPlayerDraw().X == stopped.X && scene.GetPlayerDraw().Y == stopped.Y, "Sven moved during electric stun.");
		scene.Update(3.0f, 0, 0, false);
		const auto& recovered = scene.Draws();
		Require(!contains(recovered, front) && !contains(recovered, back) && !contains(recovered, flash), "Demon did not finish its shock and reverse transformation.");
		Require(contains(recovered, stand) && contains(recovered, mood), "Recovered sheep lost its ordinary body/meter.");
		Step(scene, 1, 0);
		Require(scene.GetPlayerDraw().X > stopped.X, "Sven did not regain movement after electric stun.");
	}
	const auto walk = FixtureLevel(engine, "demon-three-steps", { { 0, 80, 240, 0 }, { 2, 480, 160, 2, 12, 0, 11 }, { 2, 640, 400, 0 } });
	LevelScene walker(engine, *xs, walk, 1);
	bool moved = false, recovered = false;
	for (i32 frame = 0; frame < 900 && !recovered; ++frame)
	{
		walker.Update(1.0f / 60, 0, 0, false);
		const auto& draws = walker.Draws();
		moved = moved || contains(draws, run);
		recovered = moved && !contains(draws, back) && !contains(draws, front) && !contains(draws, run);
	}
	Require(moved && recovered, "Demon failed to move and return after its three-step budget.");
	const auto blocked = FixtureLevel(engine, "demon-blocked", { { 0, 80, 240, 0 }, { 2, 480, 200, 0, 12, 0, 11 }, { 1, 520, 160, 0 }, { 1, 520, 240, 0 }, { 1, 440, 240, 0 }, { 1, 440, 160, 0 } });
	LevelScene trapped(engine, *xs, blocked, 1);
	trapped.Update(4, 0, 0, false);
	const auto& trappedDraws = trapped.Draws();
	Require(std::any_of(trappedDraws.begin(), trappedDraws.end(), [&](const auto& draw) { return draw.Sprite == stand && draw.X == 480 && draw.Y == 200; }),
	        "Blocked demon failed to recover at its original position.");
	const auto timeout = FixtureLevel(engine, "demon-timeout", { { 0, 80, 240, 0 }, { 2, 480, 200, 0, 12, 0, 11 } });
	LevelScene expired(engine, *xs, timeout, 1);
	expired.TimeExpired();
	for (i32 frame = 0; frame < 60 * 120 && !expired.Completed(); ++frame)
		expired.Update(1.0f / 60, 0, 0, false);
	Require(expired.Completed() && expired.GetCompletedSheep() == 0 && expired.TakeLifeHits() == 0, "Timeout UFO failed to collect a sheep after its busy demon sequence.");
	Log("PASS: natural demon trigger, transform/run/recovery, direct and moving shock branches, flash, powerup cancellation and stun release.");
}

void CheckFarmerOccupancy(MudGE& engine)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack) { return pack.Name == "XS"; });
	constexpr i32 directions[8][2] = { { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 } };
	for (i32 state : { 0, 10 })
	{
		for (const auto& direction : directions)
		{
			const i32 x = 240 - 40 * direction[0], y = 200 - 40 * direction[1];
			const auto level = FixtureLevel(engine, "farmer-solid-" + std::to_string(state) + "-" + std::to_string(x) + "-" + std::to_string(y),
			                                { { 0, x, y, 0 }, { 5, 240, 200, 0, 6, 0, state, 1000 }, { 2, 640, 400, 0 } });
			LevelScene scene(engine, *xs, level, 1);
			scene.Update(0.3f, direction[0], direction[1], false);
			Require(scene.GetPlayerDraw().X == x && scene.GetPlayerDraw().Y == y, "Player entered the farmer's occupied cell.");
		}
	}
	Log("PASS: correctly centered 160px farmer blocks his logical cell from eight directions, awake and sleeping.");
}

void CheckGirlBeamReturn(MudGE& engine, bool capture = false)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack) { return pack.Name == "XS"; });
	const auto* stand = engine.LoadSprite("Sven2/gfx/Broemse/Broemse_stand.bmp", 8, 1);
	const auto* back = engine.LoadSprite("Sven2/gfx/Broemse/anim_broemse_beam_up_diagonal_back.bmp", 5, 1);
	const auto* front = engine.LoadSprite("Sven2/gfx/Broemse/anim_broemse_beam_up_diagonal_front.bmp", 5, 1);
	const auto fall = engine.GetData().SoundPath("SfxSvenFall");
	for (i32 quadrant = 0; quadrant < 4; ++quadrant)
	{
		const auto level = FixtureLevel(engine, "girl-beam-return-" + std::to_string(quadrant),
		                                { { 0, 640, 400, 0 }, { 6, 240, 200, quadrant * 2, 6, 0, 17 }, { 2, 640, 120, 0 } });
		LevelScene scene(engine, *xs, level, 1);
		const auto* beam = quadrant == 0 || quadrant == 3 ? back : front;
		scene.Update(0.5f, 0, 0, false);
		const auto& rising = scene.Draws();
		Require(std::any_of(rising.begin(), rising.end(), [&](const auto& draw) { return draw.Sprite == beam && draw.Mirror == (quadrant < 2) && draw.X == 240 && draw.Y == 190; }),
		        "Girl beam-up body used the wrong anchor or mirrored direction.");
		if (capture && quadrant == 0)
			engine.DrawFrame(rising, "girl-beam-up.bmp");
		scene.Update(1.0f, 0, 0, false);
		const auto& hidden = scene.Draws();
		Require(std::none_of(hidden.begin(), hidden.end(), [&](const auto& draw) { return draw.Sprite == stand || draw.Sprite == back || draw.Sprite == front; }),
		        "Girl remained visible during the artifact's hidden wait.");
		scene.Update(4.5f, 0, 0, false);
		const auto& falling = scene.Draws();
		Require(std::any_of(falling.begin(), falling.end(), [&](const auto& draw) { return draw.Sprite == stand && draw.X == 240 && draw.Y < 200; }),
		        "Girl did not descend using her original standing body.");
		if (capture && quadrant == 0)
			engine.DrawFrame(falling, "girl-beam-return.bmp");
		scene.Update(1.5f, 0, 0, false);
		const auto& landed = scene.Draws();
		Require(std::any_of(landed.begin(), landed.end(), [&](const auto& draw) { return draw.Sprite == stand && draw.X == 240 && draw.Y == 200; }),
		        "Girl did not return to her unchanged ground position.");
		const auto sounds = scene.TakeSounds();
		Require(std::find(sounds.begin(), sounds.end(), fall) != sounds.end(), "Girl landing omitted its fall sound.");
	}
	Log("PASS: all four girl beam directions, hidden wait, standing descent, ground recovery and landing sound.");
}

void CheckLabFixtures(MudGE& engine)
{
	for (const auto& pack : engine.GetData().GetPacks())
		if (pack.Name == "ParityLab")
		{
			const auto episode = std::find_if(pack.LevelSets.begin(), pack.LevelSets.end(), [](const auto& set) { return set.Name == "Items and teleports"; });
			Require(episode != pack.LevelSets.end(), "ParityLab items/teleports episode is missing.");
			const auto& level = episode->Levels.front();
			LevelScene puddles(engine, pack, level, 1);
			for (i32 cell = 0; cell < 3; ++cell)
				Step(puddles, 1, 0);
			Step(puddles, 0, 1);
			Step(puddles, 0, 1);
			Require(puddles.TakeSplashSound() && puddles.GetPlayerDraw().X == 560 && puddles.GetPlayerDraw().Y == 280, "Authored lab puddle did not teleport Sven to the other puddle's north exit.");
			LevelScene water(engine, pack, level, 1);
			for (i32 cell = 0; cell < 7; ++cell)
				Step(water, 1, 0);
			for (i32 cell = 0; cell < 5; ++cell)
				Step(water, 0, 1);
			Require(water.TakeSplashSound() && (water.GetPlayerDraw().X == 200 || water.GetPlayerDraw().X == 560) && water.GetPlayerDraw().Y == 280, "Authored lab water did not send Sven to a valid puddle exit.");
			LevelScene artifact(engine, pack, level, 1);
			for (i32 cell = 0; cell < 12; ++cell)
				Step(artifact, 1, 0);
			for (i32 cell = 0; cell < 3; ++cell)
				Step(artifact, 0, -1);
			const auto* sprite = engine.LoadSprite("Sven2/gfx/Artifact.bmp", 1, 1);
			const auto& consumed = artifact.Draws();
			Require(std::none_of(consumed.begin(), consumed.end(), [&](const auto& draw) { return draw.Sprite == sprite; }),
			        "Authored lab artifact failed to disappear when Sven entered its cell.");
			const auto* icon = engine.LoadSprite("Sven2/gfx/icon_alien.bmp", 13, 1);
			const auto* ship = engine.LoadSprite("Sven2/gfx/ufo.bmp", 13, 1);
			const auto* shadow = engine.LoadSprite("Sven2/gfx/ufo_shadow.bmp", 13, 1);
			bool powered = false, visibleUfo = false;
			for (i32 frame = 0; frame < 3600 && !powered; ++frame)
			{
				artifact.Update(1.0f / 60, 0, 0, false);
				const auto& draws = artifact.Draws();
				visibleUfo = visibleUfo || (std::any_of(draws.begin(), draws.end(), [&](const auto& draw) { return draw.Sprite == ship; }) &&
				                            std::any_of(draws.begin(), draws.end(), [&](const auto& draw) { return draw.Sprite == shadow; }));
				powered = std::any_of(draws.begin(), draws.end(), [&](const auto& draw) { return draw.Sprite == icon; });
			}
			Require(powered, "Authored lab artifact never completed Sven's UFO powerup sequence.");
			Require(visibleUfo, "Authored lab artifact sequence hid the UFO and its shadow.");
			Log("PASS: actual ParityLab puddle destination, water entry and artifact pickup/beam/powerup.");
		}
}

void CheckTimeout(MudGE& engine)
{
	LevelScene scene(engine);
	scene.TimeExpired();
	for (i32 frame = 0; frame < 60 * 120 && !scene.Completed(); ++frame)
		scene.Update(1.0f / 60, 0, 0, false);
	Require(scene.Completed() && scene.GetCompletedSheep() == 0, "Timeout UFO did not collect the remaining animals.");
	Require(scene.TakeScoreDelta() == 0 && scene.TakeTimeDelta() == 0 && scene.TakeLifeHits() == 0, "Timeout collection incorrectly awarded interaction rewards or took a life.");
	const auto sounds = scene.TakeSounds();
	Require(std::find(sounds.begin(), sounds.end(), "Sven2/sfx/beam.ogg") != sounds.end(), "UFO pickup did not emit its beam sound.");
	Log("PASS: timeout UFO collection, beam events, no interaction reward and no invented life penalty.");
}

void WriteBehaviorTrace(MudGE& engine, const fpath& path)
{
	std::ofstream output(path, std::ios::binary);
	Require(static_cast<bool>(output), "Cannot create behavior trace.");
	std::map<const Sprite*, u64> spriteHashes;
	for (const auto& pack : engine.GetData().GetPacks())
	{
		for (const auto& set : pack.LevelSets)
		{
			for (const auto& level : set.Levels)
			{
				for (u32 seed : { 1u, 73u })
				{
					LevelScene scene(engine, pack, level, seed);
					output << level.Map << ' ' << seed << '\n';
					for (i32 frame = 0; frame < 480; ++frame)
					{
						u64 hash = 14695981039346656037ull;
						auto mix = [&](const auto& value) {
							const auto* bytes = reinterpret_cast<const u8*>(&value);
							for (size_t i = 0; i < sizeof(value); ++i)
								hash = (hash ^ bytes[i]) * 1099511628211ull;
						};
						constexpr i32 direction[8][2] = { { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 } };
						constexpr f32 delta[] = { 1.0f / 60, 0.03f, 0.1f, 0.25f };
						const i32 heading = (frame / 30) % 8;
						scene.Update(delta[frame % 4], direction[heading][0], direction[heading][1], frame % 11 == 0);
						if (frame == 360)
							scene.TimeExpired();
						for (const auto& draw : scene.Draws())
						{
							auto entry = spriteHashes.find(draw.Sprite);
							if (entry == spriteHashes.end())
							{
								u64 pixels = 14695981039346656037ull;
								for (auto pixel : draw.Sprite->Pixels)
								{
									for (i32 byte = 0; byte < 4; ++byte)
										pixels = (pixels ^ ((pixel >> (8 * byte)) & 255)) * 1099511628211ull;
								}
								entry = spriteHashes.emplace(draw.Sprite, pixels).first;
							}
							mix(entry->second);
							mix(draw.Sprite->Width);
							mix(draw.Sprite->Rows);
							mix(draw.Sprite->Columns);
							mix(draw.X);
							mix(draw.Y);
							mix(draw.Frame);
							mix(draw.Mirror);
							mix(draw.ClipBottom);
						}
						mix(scene.TakeScoreDelta());
						mix(scene.TakeTimeDelta());
						mix(scene.TakeLifeHits());
						mix(scene.TakeLivesGained());
						mix(scene.GetRemainingTargets());
						mix(scene.GetCompletedSheep());
						for (str sound : scene.TakeSounds())
						{
							// Trace v1 used the standing-asset root's casing for the
							// UFO event. Playback now uses the XML resource path.
							if (sound == engine.GetData().SoundPath("SfxUFO"))
							{
								str core = FindCharacterDefinition(ObjectKind::Player)->SpriteAsset;
								sound = core.substr(0, core.find('/')) + sound.substr(sound.find('/'));
							}
							for (char byte : sound)
								mix(byte);
						}
						output << frame << ' ' << hash << '\n';
					}
				}
			}
		}
	}
	Require(static_cast<bool>(output), "Cannot finish behavior trace.");
	Log("PASS: deterministic behavior trace written.");
}

void CheckObjectOwnership(MudGE& engine)
{
	const LevelInfo level = FixtureLevel(engine, "object-ownership", { { 0, 80, 80, 0 }, { 2, 240, 240, 0 }, { 3, 360, 120, 0 }, { 4, 440, 240, 0 }, { 5, 520, 360, 0 }, { 6, 600, 200, 0 }, { 11, 120, 400, 0 }, { 12, 240, 400, 0 }, { 13, 360, 400, 0 } });
	const LevelPackInfo& pack = *std::find_if(engine.GetData().GetPacks().begin(), engine.GetData().GetPacks().end(), [](const LevelPackInfo& value) { return value.Name == "XS"; });
	LevelScene scene(engine, pack, level, 1);
	ObjectManager& objects = scene.GetObjects();
	Require(dynamic_cast<PlayerCharacter*>(&objects.Get(0)) != nullptr, "Player must be manager-owned.");
	Require(dynamic_cast<Sheep*>(&objects.Get(1)) != nullptr, "Sheep instance is missing.");
	Require(dynamic_cast<Alien*>(&objects.Get(2)) != nullptr, "Alien instance is missing.");
	Require(dynamic_cast<Dog*>(&objects.Get(3)) != nullptr, "Dog instance is missing.");
	Require(dynamic_cast<Farmer*>(&objects.Get(4)) != nullptr, "Farmer instance is missing.");
	Require(dynamic_cast<Girl*>(&objects.Get(5)) != nullptr, "Girl instance is missing.");
	Require(dynamic_cast<Chicken*>(&objects.Get(6)) != nullptr, "Chicken instance is missing.");
	Require(dynamic_cast<Goat*>(&objects.Get(7)) != nullptr, "Goat instance is missing.");
	Require(dynamic_cast<Pig*>(&objects.Get(8)) != nullptr, "Pig instance is missing.");
	BaseObject* originalSheep = &objects.Get(1);
	WorldObject effect = objects[1];
	effect.Kind = ObjectKind::CompletionEffect;
	objects.Add(effect);
	Require(&objects.Get(1) == originalSheep, "Adding an object invalidated an existing entity.");
	Require(objects.Find(ObjectKind::Dog).size() == 1, "Typed manager lookup lost an enemy.");
	bool rejected = false;
	try
	{
		objects.Replace(3, objects[6]);
	}
	catch (const std::runtime_error&)
	{
		rejected = true;
	}
	Require(rejected, "A live object slot must not be reused.");
	objects[3].Freed = true;
	objects.Replace(3, objects[6]);
	Require(dynamic_cast<Chicken*>(&objects.Get(3)) != nullptr && objects.Find(ObjectKind::Dog).empty(), "Reused slots must replace their actual object type.");
	Log("PASS: individual character ownership, typed factories, stable entities, lookup and slot reuse.");
}

void CheckMenuPresentation(MudGE& engine, const fpath& saves, bool capture)
{
	using Page = GameUi::Page;
	GameUi ui(engine, saves / L"menu-presentation");
	std::array<const Sprite*, 5> eyes{};
	for (size_t eye = 0; eye < eyes.size(); ++eye)
		eyes[eye] = engine.LoadSprite("XXX/SvenXXX/gfx/Help_BG_eye0" + std::to_string(eye + 1) + ".bmp", 6, 1);
	auto hasDraw = [](const std::vector<SpriteDraw>& draws, const Sprite* image, i32 x, i32 y, i32 frame) { return std::any_of(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == image && draw.X == x && draw.Y == y && draw.Frame == frame; }); };
	for (Page page : { Page::GetPacks, Page::Help, Page::Highscores })
	{
		ui.Show(page);
		WindowInput::Result cursor;
		cursor.Mouse = { 0, 0 };
		ui.Handle(cursor);
		const std::vector<SpriteDraw>& upper = ui.Draws(0);
		constexpr i32 upperX[] = { -130, -137, 680, 640, 680 }, upperY[] = { 100, 400, -120, 150, 425 };
		for (size_t eye = 0; eye < eyes.size(); ++eye)
			Require(hasDraw(upper, eyes[eye], upperX[eye], upperY[eye], 0) && hasDraw(upper, eyes[eye], upperX[eye], upperY[eye], 1), "Upper-left menu eye transform/base layering differs from native.");
		cursor.Mouse = { 799, 599 };
		ui.Handle(cursor);
		const std::vector<SpriteDraw>& lower = ui.Draws(0);
		constexpr i32 lowerX[] = { 0, -17, 693, 817, 812 }, lowerY[] = { 109, 459, 0, 161, 444 };
		for (size_t eye = 0; eye < eyes.size(); ++eye)
			Require(hasDraw(lower, eyes[eye], lowerX[eye], lowerY[eye], 0) && hasDraw(lower, eyes[eye], lowerX[eye], lowerY[eye], 5), "Lower-right menu eyes do not follow the cursor.");
		for (POINT outside : { POINT{ -1, -1 }, POINT{ -200, 300 }, POINT{ 800, 600 }, POINT{ 1200, 900 } })
		{
			cursor.Mouse = outside;
			cursor.Click = true;
			ui.Handle(cursor);
			const std::vector<SpriteDraw>& held = ui.Draws(0);
			for (size_t eye = 0; eye < eyes.size(); ++eye)
				Require(hasDraw(held, eyes[eye], lowerX[eye], lowerY[eye], 0) && hasDraw(held, eyes[eye], lowerX[eye], lowerY[eye], 5), "Leaving the viewport changed the menu eye pose.");
			Require(ui.GetPage() == page, "Out-of-window input activated a menu.");
		}
		cursor.Click = false;
		for (POINT point : { POINT{ 0, 0 }, POINT{ 200, 150 }, POINT{ 400, 300 }, POINT{ 600, 450 }, POINT{ 799, 599 } })
		{
			cursor.Mouse = point;
			ui.Handle(cursor);
			for (const SpriteDraw& draw : ui.Draws(0))
				Require((draw.Sprite != eyes[0] || draw.X <= 0) && (draw.Sprite != eyes[2] || draw.Y <= 0), "Menu tentacle detached from its screen edge.");
		}
		if (capture && page == Page::Help)
			engine.DrawFrame(ui.Draws(0), "menu-eyes-edge.bmp");
		cursor.Mouse = { 400, 300 };
		ui.Handle(cursor);
		if (capture && page == Page::Help)
			engine.DrawFrame(ui.Draws(0), "menu-eyes.bmp");
	}
	const Sprite* sven = engine.LoadSprite(engine.GetData().GetSpriteDefinition("SvenStand", GameEdition::XXX).File, 8, 1);
	BitmapFont titleFont(engine, false, false, GameEdition::XXX);
	for (i32 players : { 2, 1 })
	{
		ui.Show(Page::Title);
		WindowInput::Result choose;
		choose.Click = true;
		choose.Mouse = { players == 2 ? 210 : 40, 560 };
		ui.Handle(choose);
		Require(ui.GetPage() == Page::GetPacks, "Player mode button did not open episode selection.");
		const std::vector<SpriteDraw>& draws = ui.Draws(0);
		Require(hasDraw(draws, sven, 460, 65, 5), "Right-facing menu Sven is missing.");
		Require(hasDraw(draws, sven, 400, 65, 3) == (players == 2), "Menu Sven count differs from requested player mode.");
		std::vector<SpriteDraw> heading;
		titleFont.Append(heading, players == 2 ? "2 SPIELER" : "1 SPIELER", 150, 80);
		Require(std::any_of(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.X == 150 && draw.Y == 80 && draw.Sprite->Pixels == heading.front().Sprite->Pixels; }), "Player heading incorrectly follows the highlighted pack instead of the requested mode.");
		if (players == 2)
			for (cstr name : { "IconPlayer1", "IconPlayer2" })
			{
				const GameData::SpriteDefinition& definition = engine.GetData().GetSpriteDefinition(name, GameEdition::XXX);
				const Sprite* icon = engine.LoadSprite(definition.File, definition.Rows, definition.Columns);
				Require(hasDraw(draws, icon, name == str("IconPlayer1") ? 390 : 430, 10, 0), "Multiplayer menu identifier is missing.");
			}
		Require(ui.GetPlayerCount() == 1, "Zwo gameplay must remain single-player even under the two-player menu.");
		if (capture)
			engine.DrawFrame(draws, players == 2 ? "two-player-menu.bmp" : "one-player-menu.bmp");
	}
	WindowInput::Result two;
	two.Click = true;
	two.Mouse = { 210, 560 };
	ui.Handle(two);
	Require(ui.GetPage() == Page::GetPacks, "Switching menu mode incorrectly closes episode selection.");
	ui.Handle(two);
	Require(ui.GetPage() == Page::Title, "Repeated selection of the same player mode must close the menu.");
	ui.Show(Page::Help);
	WindowInput::Result enemies;
	enemies.Click = true;
	enemies.Mouse = { 220, 40 };
	ui.Handle(enemies);
	const Sprite* cloud = engine.LoadSprite("XXX/SvenXXX/gfx/anim_cloud.bmp", 10, 1);
	const std::array<const Sprite*, 4> catches = { cloud, cloud, engine.LoadSprite(engine.GetData().GetSpriteDefinition("BroemseSvenFront", GameEdition::XXX).File, 35, 1), engine.LoadSprite(engine.GetData().GetSpriteDefinition("AlienProbe2", GameEdition::XXX).File, 55, 1) };
	constexpr i32 frames[] = { 4, 3, 4, 24 };
	for (size_t enemy = 0; enemy < catches.size(); ++enemy)
	{
		const std::vector<SpriteDraw>& draws = ui.Draws(0);
		const Sprite* image = catches[enemy];
		Require(hasDraw(draws, image, 630 - image->FrameWidth / 2, 350 - image->FrameHeight / 2, frames[enemy]), "Enemy catch demonstration differs from native help.");
		Require(std::any_of(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == image && draw.X == 630 - image->FrameWidth / 2 && draw.Mirror == (enemy == 2); }), "Enemy help did not preserve its native mirrored binding.");
		if (capture)
			engine.DrawFrame(draws, ("enemy-help-" + std::to_string(enemy) + ".bmp").c_str());
		WindowInput::Result nextEnemy;
		nextEnemy.Click = true;
		nextEnemy.Mouse = { 320, 95 };
		ui.Handle(nextEnemy);
	}
	Log("PASS: cursor-driven menu eyes, requested 1/2-player headers/portraits/identifiers and all four native enemy catch previews.");
}

void CheckPresentationCorrections(MudGE& engine, const fpath& saves, bool capture)
{
	const GameData& data = engine.GetData();
	const std::vector<LevelPackInfo>& packs = data.GetPacks();
	const LevelPackInfo& xxx = *std::find_if(packs.begin(), packs.end(), [](const LevelPackInfo& pack) { return pack.Edition == GameEdition::XXX; });
	const LevelPackInfo& zwo = *std::find_if(packs.begin(), packs.end(), [](const LevelPackInfo& pack) { return pack.Name == "XS" && pack.Edition == GameEdition::Zwo; });
	BitmapFont smallFont(engine, true, false, GameEdition::XXX);
	Require(smallFont.Width("00") == 18 && smallFont.Width("11") == 12 && smallFont.Width(":") == 3, "Native FDD exclusive glyph bounds/advance regressed.");
	for (i32 key : { VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_RCONTROL, VK_LCONTROL, VK_RSHIFT, VK_LSHIFT, VK_SPACE, VK_RETURN, i32{ 'X' }, i32{ '0' }, VK_NUMPAD4 })
		Require(WindowInput::VirtualKeyFromNative(WindowInput::NativeKeyFromVirtual(key)) == key, "Native/Windows keyboard mapping is not reversible.");
	GameUi ui(engine, saves / L"keyboard");
	ui.Show(GameUi::Page::Help);
	WindowInput::Result next;
	next.Right = true;
	for (i32 tab = 0; tab < 3; ++tab)
		ui.Handle(next);
	if (capture)
	{
		engine.DrawFrame(ui.Draws(0), "controls.bmp");
		WindowInput windowInput(engine.WindowHandle());
		SendMessageW(engine.WindowHandle(), WM_KEYDOWN, VK_CONTROL, LPARAM{ 0x011d0001 });
		Require(windowInput.Poll().BindingKey == static_cast<i32>(NativeKey::RightControl), "Extended right Control was not captured in the native key domain.");
		SendMessageW(engine.WindowHandle(), WM_KEYUP, VK_CONTROL, static_cast<LPARAM>(0xc11d0001u));
		SendMessageW(engine.WindowHandle(), WM_KEYDOWN, 'X', LPARAM{ 0x002d0001 });
		Require(windowInput.Poll().BindingKey == 'x', "Keyboard rebinding did not capture a real window key message.");
		SendMessageW(engine.WindowHandle(), WM_KEYUP, 'X', static_cast<LPARAM>(0xc02d0001u));
	}
	WindowInput::Result click;
	click.Click = true;
	click.Mouse = { 250, 210 };
	ui.Handle(click);
	Require(ui.GetControlSelection() == 0, "Native control label click selected the wrong binding.");
	WindowInput::Result key;
	key.BindingKey = 'x';
	ui.Handle(key);
	Require(ui.GetControlSelection() == -1 && ui.GetControlBindings()[0][0] == 'x' && ui.HasCustomControls(), "Rebinding a control did not complete.");
	GameUi reloaded(engine, saves / L"keyboard");
	Require(reloaded.GetControlBindings()[0][0] == 'x', "Keyboard bindings were not persisted.");
	GameUi secondPlayer(engine, saves / L"keyboard-p2");
	secondPlayer.SetControlBinding(1, 0, 't');
	GameUi secondPlayerReloaded(engine, saves / L"keyboard-p2");
	Require(secondPlayerReloaded.GetControlBindings()[1][0] == 't' && !secondPlayerReloaded.HasCustomControls(), "Editing player 2 controls must not replace Zwo's legacy player 1 controls.");
	ui.Handle(click);
	WindowInput::Result cancel;
	cancel.Back = true;
	ui.Handle(cancel);
	Require(ui.GetPage() == GameUi::Page::Help && ui.GetControlSelection() == -1, "Escape must cancel rebinding without leaving the control screen.");
	// The user's marked collision: Garten 02 episode, first map garden10.lvl.
	const LevelSetInfo& garden = *std::find_if(xxx.LevelSets.begin(), xxx.LevelSets.end(), [](const LevelSetInfo& set) { return set.Name == "Garten 02"; });
	LevelScene gardenScene(engine, xxx, garden.Levels.front(), 1);
	Require(gardenScene.GetObjects().CanOccupy(279, gardenScene.GetPlayerIndex()), "Dormant mermaid leaves invisible collision in Garten 02.");
	Mermaid& mermaid = dynamic_cast<Mermaid&>(gardenScene.GetObjects().Get(gardenScene.GetObjects().Find(ObjectKind::Mermaid).front()));
	Require(mermaid.Reveal() && !gardenScene.GetObjects().CanOccupy(279, gardenScene.GetPlayerIndex()), "Revealed mermaid must occupy its cell.");
	if (capture)
		engine.DrawFrame(gardenScene.Draws(), "garden-mermaid-revealed.bmp");
	auto fixture = [&](const str& name, std::initializer_list<FixtureActor> actors) { return FixtureLevel(engine, name, actors, 0, GameEdition::XXX); };
	LevelPackInfo profile = zwo;
	profile.Edition = GameEdition::XXX;
	const Sprite* stand = engine.LoadSprite(data.GetSpriteDefinition("SvenStand", GameEdition::XXX).File, 8, 1);
	for (ObjectKind kind : { ObjectKind::Sheep, ObjectKind::Chicken, ObjectKind::Goat, ObjectKind::Pig, ObjectKind::Penguin, ObjectKind::Turtle, ObjectKind::Rabbit })
	{
		for (i32 facing = 0; facing < 4; ++facing)
		{
			const i32 playerFacing = 2 * facing;
			for (i32 requested = -1; requested < 8; ++requested)
			{
				const i32 dx = requested < 0 ? 0 : Directions[requested][0], dy = requested < 0 ? 0 : Directions[requested][1];
				const LevelInfo level = fixture("action-boundary-" + std::to_string(static_cast<i32>(kind)) + "-" + std::to_string(facing) + "-" + std::to_string(requested), { { 0, 240 - Directions[playerFacing][0] * 40, 200 - Directions[playerFacing][1] * 40, playerFacing }, { static_cast<i32>(kind), 240, 200, facing } });
				LevelScene scene(engine, profile, level, 1);
				// Level initialization randomly assigns Frigida; this fixture tests ordinary animals.
				scene.GetObjects()[1].Temperament = 0;
				scene.Update(0, 0, 0, true);
				Require(scene.GetPlayerDraw().Sprite == stand, "XXX accepted an action held across level entry.");
				scene.Update(0, Directions[playerFacing][0], Directions[playerFacing][1], false);
				scene.Update(0.001f, 0, 0, true);
				Require(scene.GetPlayerDraw().Sprite != stand, "Release/press did not start the XXX animal interaction.");
				const Sprite* interaction = scene.GetPlayerDraw().Sprite;
				scene.Update(0, dx, dy, false);
				scene.Update(0, dx, dy, true);
				bool finished = false;
				for (i32 tick = 0; tick < 2000 && !finished; ++tick)
				{
					scene.Update(0.01f, dx, dy, true);
					finished = scene.TakeScoreDelta() > 0;
				}
				if (!finished)
					throw std::runtime_error("XXX ordinary interaction never finished: " + level.Map + "; frame=" + std::to_string(scene.GetPlayerDraw().Frame) + "; rows=" + std::to_string(interaction->Rows) + "; same=" + std::to_string(scene.GetPlayerDraw().Sprite == interaction));
				const bool compatible = requested < 0 || (requested - 2 * facing + 8) % 8 <= 2;
				Require((scene.GetPlayerDraw().Sprite == interaction) == compatible, "XXX queued repeat ignored the requested direction at completion.");
			}
		}
	}
	const LevelInfo heldLevel = fixture("held-action-no-repeat", { { 0, 240, 240, 0 }, { 2, 240, 200, 0 } });
	LevelScene heldScene(engine, profile, heldLevel, 1);
	heldScene.GetObjects()[1].Temperament = 0;
	heldScene.Update(0, 0, 0, false);
	heldScene.Update(0.001f, 0, 0, true);
	i32 completed = 0;
	for (i32 tick = 0; tick < 1200; ++tick)
	{
		heldScene.Update(0.01f, 0, 0, true);
		completed += heldScene.TakeScoreDelta();
	}
	Require(completed == 1 && heldScene.GetPlayerDraw().Sprite == stand, "Holding action automatically repeated a XXX interaction.");
	for (i32 facing = 0; facing < 4; ++facing)
	{
		const i32 playerFacing = 2 * facing;
		for (i32 requested = -1; requested < 8; ++requested)
		{
			const i32 dx = requested < 0 ? 0 : Directions[requested][0], dy = requested < 0 ? 0 : Directions[requested][1];
			const LevelInfo level = fixture("frigida-repeat-" + std::to_string(facing) + "-" + std::to_string(requested), { { 0, 240 - Directions[playerFacing][0] * 40, 200 - Directions[playerFacing][1] * 40, playerFacing }, { 2, 240, 200, facing } });
			LevelScene scene(engine, profile, level, 1);
			scene.GetObjects()[1].Temperament = 1;
			scene.Update(0, Directions[playerFacing][0], Directions[playerFacing][1], false);
			scene.Update(0.001f, 0, 0, true);
			const Sprite* frigida = scene.GetPlayerDraw().Sprite;
			Require(frigida != stand && scene.GetObjects()[1].Temperament == 2, "Frigida repeat fixture did not start.");
			scene.Update(0, dx, dy, false);
			scene.Update(0, dx, dy, true);
			for (i32 tick = 0; tick < 200 && scene.GetObjects()[1].Impatience == 0; ++tick)
				scene.Update(0.01f, dx, dy, true);
			Require(scene.GetObjects()[1].Impatience == 1, "Frigida cycle failed to advance its impatience meter.");
			const bool compatible = requested < 0 || (requested - 2 * facing + 8) % 8 <= 2;
			Require((scene.GetPlayerDraw().Sprite == frigida) == compatible, "XXX Frigida repeat ignored the requested direction at completion.");
		}
	}
	Log("PASS: XXX release arming, held-key suppression and all nine repeat directions for all seven animal kinds/four facings, including Frigida.");
	const Sprite* mood = engine.LoadSprite("XXX/SvenXXX/gfx/symbols.bmp", 12, 1);
	constexpr i32 markerX[3][4] = { { 20, 20, 20, 20 }, { 40, 24, 30, 16 }, { 40, 24, 30, 16 } };
	for (i32 species = 0; species < 3; ++species)
	{
		for (i32 facing = 0; facing < 4; ++facing)
		{
			const LevelInfo level = fixture("marker-" + std::to_string(species) + "-" + std::to_string(facing), { { 0, 80, 80, 0 }, { 25 + species, 240, 200, facing } });
			LevelScene scene(engine, profile, level, 1);
			const std::vector<SpriteDraw>& draws = scene.Draws();
			Require(std::any_of(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == mood && draw.X == 240 + markerX[species][facing] && draw.Y == 199; }),
			        "New animal marker is not at its native facing offset.");
			if (capture && facing == 0)
				engine.DrawFrame(draws, ("animal-marker-" + std::to_string(species) + ".bmp").c_str());
		}
	}
	for (GameEdition edition : { GameEdition::Zwo, GameEdition::XXX })
	{
		for (i32 kind : { 2, 11, 12, 13 })
		{
			LevelInfo level = FixtureLevel(engine, "effect-anchor-" + std::to_string(static_cast<i32>(edition)) + "-" + std::to_string(kind), { { 0, 240, 200, 0 }, { kind, 240, 160, 0 } });
			if (edition == GameEdition::XXX)
				level = fixture("effect-anchor-xxx-" + std::to_string(kind), { { 0, 240, 200, 0 }, { kind, 240, 160, 0 } });
			LevelPackInfo pack = zwo;
			pack.Edition = edition;
			LevelScene scene(engine, pack, level, 1);
			scene.Update(0, 0, 0, false);
			scene.Update(0.001f, 0, 0, true);
			Require(scene.GetPlayerDraw().Y == 160, "Marker regression fixture failed to start a compound interaction.");
			for (ItemCode code : { ItemCode::StinkMushroom, ItemCode::Flower, ItemCode::AlienPower })
			{
				scene.GetItems().GetPowerup() = { code, 10, 0 };
				const std::vector<SpriteDraw>& draws = scene.Draws();
				Require(std::any_of(draws.begin(), draws.end(), [](const SpriteDraw& draw) { return draw.X == 268 && draw.Y == 156; }),
				        "Equipped effect detached from the compound interaction canvas.");
				if (capture && code == ItemCode::StinkMushroom && kind == 2)
					engine.DrawFrame(draws, (edition == GameEdition::XXX ? "xxx-interaction-effect.bmp" : "zwo-interaction-effect.bmp"));
			}
		}
	}
	for (const LevelPackInfo* pack : { &zwo, &xxx })
	{
		GameSession session(engine, *pack, pack->LevelSets.front());
		const std::vector<SpriteDraw>& draws = session.Draws();
		const bool modern = data.GetThemeEdition() == GameEdition::XXX;
		Require(std::any_of(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.X == (modern ? 745 : 733) && draw.Y == 19; }),
		        "Score HUD does not match the shared UI theme coordinates.");
		Require(std::any_of(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.X == (modern ? 26 : 18) && draw.Y == (modern ? 34 : 36); }), "Timer HUD does not match the shared UI theme coordinates.");
		if (capture)
			engine.DrawFrame(draws, pack->Edition == GameEdition::XXX ? "xxx-hud.bmp" : "zwo-hud.bmp");
	}
	RoundResult result;
	result.Time = result.TimeRemaining = 180;
	result.Sheep = result.SheepRemaining = 1;
	result.Lives = result.LivesRemaining = 3;
	ui.ShowResults(result);
	const std::vector<SpriteDraw>& bonus = ui.Draws(0);
	Require(std::any_of(bonus.begin(), bonus.end(), [](const SpriteDraw& draw) { return draw.X == 304 && draw.Y == 247 && draw.Sprite->FrameWidth == 6; }), "Bonus animal count lost native centered text alignment.");
	Require(std::any_of(bonus.begin(), bonus.end(), [](const SpriteDraw& draw) { return draw.X == 296 && draw.Y == 172; }), "Bonus timer lost native text alignment.");
	if (capture)
	{
		engine.DrawFrame(bonus, "bonus-layout.bmp");
		ui.Show(GameUi::Page::GetPacks);
		engine.DrawFrame(ui.Draws(0), "short-episode-labels.bmp");
		Log("Presentation correction captures: " + saves.string());
	}
	Log("PASS: native font advances/HUD, all new species/facings, compound effect anchors, dormant mermaid collision and keyboard binding capture/persistence.");
}

void CheckSharedPlayerRuntime(MudGE& engine)
{
	const GameData& data = engine.GetData();
	const Sprite* stand = engine.LoadSprite(data.GetSpriteDefinition("SvenStand", GameEdition::XXX).File, 8, 1);
	struct ActionCase
	{
		PlayerActionState State;
		i32 Facing;
		std::vector<i32> Frames;
		ItemCode Powerup = ItemCode::None;
	};
	std::vector<ActionCase> cases;
	for (i32 facing = 0; facing < 8; ++facing)
		cases.push_back({ PlayerActionState::Whistle, facing, facing % 2 ? std::vector<i32>{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 10, 11, 12, 13, 13, 14, 15 } : std::vector<i32>{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 } });
	for (i32 facing = 0; facing < 4; ++facing)
	{
		cases.push_back({ PlayerActionState::Kicked, 2 * facing, facing == 0 || facing == 3 ? std::vector<i32>{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 10, 10, 11, 11 } : std::vector<i32>{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 } });
		cases.push_back({ PlayerActionState::Frigida, 2 * facing, { 0, 0, 1, 2, 2, 0, 0 } });
	}
	for (ItemCode powerup : { ItemCode::Rocket, ItemCode::SlowMushroom, ItemCode::AlienPower })
		cases.push_back({ PlayerActionState::Frigida, 0, { 0, 0, 1, 2, 2, 0, 0 }, powerup });
	for (GameEdition edition : { GameEdition::Zwo, GameEdition::XXX })
	{
		LevelPackInfo pack = *std::find_if(data.GetPacks().begin(), data.GetPacks().end(), [](const LevelPackInfo& candidate) { return candidate.Name == "XS" && candidate.Edition == GameEdition::Zwo; });
		pack.Edition = edition;
		for (const ActionCase& test : cases)
		{
			const i32 animalFacing = test.State == PlayerActionState::Whistle ? (test.Facing / 2 + 2) % 4 : test.Facing / 2;
			const LevelInfo level = FixtureLevel(engine, "shared-action-" + std::to_string(static_cast<i32>(edition)) + "-" + std::to_string(static_cast<i32>(test.State)) + "-" + std::to_string(test.Facing), { { 0, 240, 240, test.Facing }, { 2, 240 + Directions[test.Facing][0] * 40, 240 + Directions[test.Facing][1] * 40, animalFacing, test.State == PlayerActionState::Kicked ? 10 : 6 } }, 0, edition);
			LevelScene scene(engine, pack, level, 1);
			scene.GetObjects()[1].Temperament = test.State == PlayerActionState::Frigida ? 1 : 0;
			if (test.Powerup != ItemCode::None)
				scene.GetItems().GetPowerup() = { test.Powerup, 30, 0 };
			scene.Update(0, Directions[test.Facing][0], Directions[test.Facing][1], true);
			Require(scene.GetPlayerDraw().Sprite == stand, "A source format bypassed shared XXX release arming.");
			scene.Update(0, Directions[test.Facing][0], Directions[test.Facing][1], false);
			scene.Update(0, 0, 0, true);
			const Sprite* animation = scene.GetPlayerDraw().Sprite;
			Require(animation != stand, "Shared action fixture did not start its animation.");
			const f32 rate = test.State == PlayerActionState::Frigida ? (test.Powerup == ItemCode::Rocket ? 30.0f : (test.Powerup == ItemCode::SlowMushroom ? 12.0f : (test.Powerup == ItemCode::AlienPower ? 40.0f : 20.0f))) : 15.0f;
			scene.Update(0.5f / rate, 0, 0, true);
			for (size_t frame = 0; frame < test.Frames.size(); ++frame)
			{
				if (frame)
					scene.Update(1 / rate, 0, 0, true);
				Require(scene.GetPlayerDraw().Sprite == animation && scene.GetPlayerDraw().Frame == test.Frames[frame], "Shared XXX action frame or duration differs from the native XML sequence.");
				Require(scene.GetPlayerDraw().Mirror == (test.State == PlayerActionState::Whistle ? SvenMirror[test.Facing] : SheepMirror[animalFacing]), "Shared XXX action lost its native facing/mirroring binding.");
				for (const SpriteDraw& draw : scene.Draws())
					Require(draw.Frame >= 0 && draw.Frame < draw.Sprite->Rows * draw.Sprite->Columns, "Shared action emitted an invalid sprite frame.");
			}
			scene.Update(1 / rate, 0, 0, true);
			Require(scene.GetPlayerDraw().Sprite == stand, "Shared XXX action outlived its descriptor or repeated on a held key.");
		}
		const LevelInfo level = FixtureLevel(engine, "shared-player-" + std::to_string(static_cast<i32>(edition)), { { 0, 240, 240, 0 }, { 2, 640, 400, 0 } }, 0, edition);
		LevelScene scene(engine, pack, level, 1);
		scene.Update(0.5f / 24, 1, 0, false);
		for (i32 frame = 0; frame < 14; ++frame)
		{
			if (frame)
				scene.Update(1.0f / 24, 1, 0, false);
			Require(scene.GetPlayerDraw().Frame == frame, "Shared walking sequence/rate depends on the source level format.");
		}
		scene.Update(1, 0, 0, false);
		scene.GetPlayer().Teleport(scene.GetObjects()[scene.GetPlayerIndex()].Tile);
		constexpr i32 blink[] = { 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1 };
		scene.Update(0.5f / 15, 0, 0, false);
		for (size_t frame = 0; frame < std::size(blink); ++frame)
		{
			if (frame)
				scene.Update(1.0f / 15, 0, 0, false);
			Require(scene.GetPlayerDraw().Frame == blink[frame] && scene.GetPlayerDraw().Sprite != stand, "Shared teleport blink sequence differs between source formats.");
		}
		scene.Update(1.0f / 15, 0, 0, false);
		Require(scene.GetPlayerDraw().Sprite == stand, "Shared teleport blink did not finish at its descriptor boundary.");
		const LevelInfo shockLevel = FixtureLevel(engine, "shared-shock-" + std::to_string(static_cast<i32>(edition)), { { 0, 240, 200, 0 }, { 2, 200, 200, 0, 12, 0, 11 }, { 2, 640, 400, 0 } }, 0, edition);
		LevelScene shock(engine, pack, shockLevel, 1);
		shock.GetObjects()[1].Temperament = 0;
		const GameData::SpriteDefinition& electric = data.GetSpriteDefinition("SvenElectricBack", GameEdition::XXX);
		const Sprite* electricSprite = engine.LoadSprite(electric.File, electric.Rows, electric.Columns);
		for (i32 tick = 0; tick < 3000 && shock.GetPlayerDraw().Sprite != electricSprite; ++tick)
			shock.Update(0.001f, 0, 0, false);
		Require(shock.GetPlayerDraw().Sprite == electricSprite, "Demon contact did not enter the shared electric-shock animation.");
		shock.Update(0.5f / 15, 0, 0, false);
		for (i32 frame = 0; frame < 20; ++frame)
		{
			if (frame)
				shock.Update(1.0f / 15, 0, 0, false);
			Require(shock.GetPlayerDraw().Sprite == electricSprite && shock.GetPlayerDraw().Frame == (frame % 4 < 2 ? 1 : 2), "Shared electric-shock sequence/rate differs from XXX.");
		}
		shock.Update(1.0f / 15, 0, 0, false);
		Require(shock.GetPlayerDraw().Sprite == stand, "Shared electric-shock animation did not release Sven at its descriptor boundary.");
	}
	Log("PASS: one XXX runtime for v6/v7 maps; complete whistle/kick/Frigida/walk/blink/shock sequences, mirrored directions and action boundaries.");
}

void CheckPlayerMovement(MudGE& engine)
{
	const GameData& data = engine.GetData();
	const LevelPackInfo& source = *std::find_if(data.GetPacks().begin(), data.GetPacks().end(), [](const LevelPackInfo& pack) { return pack.Name == "XS" && pack.Edition == GameEdition::Zwo; });
	// Literal transition results from XXX 0x4164C0, mapped to render facings.
	constexpr i32 transitions[8][8] = {
		{ 0, 0, 0, 4, 4, 4, 0, 0 },
		{ 1, 1, 1, 1, 5, 5, 5, 1 },
		{ 2, 2, 2, 2, 2, 6, 6, 6 },
		{ 7, 3, 3, 3, 3, 3, 7, 7 },
		{ 0, 0, 4, 4, 4, 4, 4, 0 },
		{ 1, 1, 1, 5, 5, 5, 5, 5 },
		{ 6, 2, 2, 2, 6, 6, 6, 6 },
		{ 7, 7, 3, 3, 3, 7, 7, 7 }
	};
	const str headings[] = { "Back", "02", "Side", "01", "Front", "01", "Side", "02" };
	auto cell = [&](const LevelInfo& level, i32 tile, OccupancyCode terrain, u8 direction) {
		bytestream bytes = data.ReadAsset(level.Map);
		const i32 code = static_cast<i32>(terrain);
		std::memcpy(bytes.data() + 16 + tile * 24, &code, sizeof(code));
		bytes[16 + tile * 24 + 4] = direction;
		std::ofstream output(data.GetRoot() / std::filesystem::u8path(level.Map), std::ios::binary | std::ios::trunc);
		output.write(reinterpret_cast<cstr>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	};
	for (GameEdition edition : { GameEdition::Zwo, GameEdition::XXX })
	{
		LevelPackInfo pack = source;
		pack.Edition = edition;
		for (i32 initial = 0; initial < 8; ++initial)
		{
			for (i32 requested = 0; requested < 8; ++requested)
			{
				const LevelInfo level = FixtureLevel(engine, "motion-" + std::to_string(static_cast<i32>(edition)) + "-" + std::to_string(initial) + "-" + std::to_string(requested), { { 0, 240, 240, 0 }, { 2, 640, 400, 0 } }, 0, edition);
				LevelScene scene(engine, pack, level, 1);
				const i32 base = scene.GetObjects()[scene.GetPlayerIndex()].Tile;
				const i32 destination = base + Directions[initial][0] + 22 * Directions[initial][1];
				scene.Update(0.04f, Directions[initial][0], Directions[initial][1], false);
				Require(scene.GetGrid().GetOccupancy(base) == OccupancyCode::Player && scene.GetGrid().GetOccupancy(destination) == OccupancyCode::PlayerReservation, "Movement failed to retain the base cell and reserve its destination.");
				Require(scene.GetGrid().GetReservationOwner(destination) == static_cast<i32>(scene.GetPlayerIndex()), "Step reservation has no player owner.");
				const f32 x = scene.GetPlayer().GetX(), y = scene.GetPlayer().GetY();
				scene.Update(0.02f, Directions[requested][0], Directions[requested][1], false);
				const i32 resolved = transitions[initial][requested];
				const f32 distance = 145 * 0.02f * (resolved % 2 ? 0.75f : 1);
				Require(std::abs(scene.GetPlayer().GetX() - x - Directions[resolved][0] * distance) < 0.001f && std::abs(scene.GetPlayer().GetY() - y - Directions[resolved][1] * distance) < 0.001f, "Mid-step transition differs from native 0x4164C0.");
				const GameData::SpriteDefinition& sprite = data.GetSpriteDefinition("SvenWalk" + headings[resolved], GameEdition::XXX);
				Require(scene.GetPlayerDraw().Sprite == engine.LoadSprite(sprite.File, sprite.Rows, sprite.Columns) && scene.GetPlayerDraw().Mirror == SvenMirror[resolved], "Walking sprite follows requested facing instead of actual motion.");
				scene.Update(1, 0, 0, false);
				const bool reversed = resolved != initial;
				Require(scene.GetPlayer().GetX() == 240 + (reversed ? 0 : Directions[initial][0] * 40) && scene.GetPlayer().GetY() == 240 + (reversed ? 0 : Directions[initial][1] * 40), "Key release failed to finish/reverse exactly one tile step.");
				Require(scene.GetGrid().GetOccupancy(reversed ? destination : base) == OccupancyCode::Empty && scene.GetGrid().GetReservationOwner(reversed ? destination : base) == -1, "A committed/reversed step left an invisible reservation behind.");
			}
		}
		const LevelInfo level = FixtureLevel(engine, "motion-boundaries-" + std::to_string(static_cast<i32>(edition)), { { 0, 240, 240, 0 }, { 2, 640, 400, 0 } }, 0, edition);
		LevelScene boundary(engine, pack, level, 1);
		boundary.GetPlayer().SetInput({ 1, 0, false });
		boundary.GetPlayer().AdvancePlayer(40.0f / 145, 1, 0);
		Require(boundary.GetPlayer().GetX() == 280 && boundary.GetObjects()[boundary.GetPlayerIndex()].X == 240, "Native strict outward boundary was prematurely committed.");
		boundary.GetPlayer().AdvancePlayer(0, 1, 0);
		Require(boundary.GetObjects()[boundary.GetPlayerIndex()].X == 240, "A zero-duration update committed the strict boundary.");
		boundary.GetPlayer().AdvancePlayer(0.001f, 1, 0);
		Require(boundary.GetObjects()[boundary.GetPlayerIndex()].X == 280, "Crossing the native outward boundary did not commit the step.");
		LevelScene corner(engine, pack, level, 1);
		corner.Update(0.04f, 1, 0, false);
		corner.Update(0.3f, 0, -1, false);
		corner.Update(1, 0, 0, false);
		Require(corner.GetPlayer().GetX() == 280 && corner.GetPlayer().GetY() == 200, "A held corner turn did not start at the next tile boundary.");
		LevelScene cancelled(engine, pack, level, 1);
		const i32 base = cancelled.GetObjects()[cancelled.GetPlayerIndex()].Tile;
		cancelled.Update(0.04f, 1, 0, false);
		cancelled.GetPlayer().CancelPlayerStep();
		Require(cancelled.GetPlayer().GetX() == 240 && cancelled.GetGrid().GetOccupancy(base + 1) == OccupancyCode::Empty && cancelled.GetGrid().GetReservationOwner(base + 1) == -1, "Interrupted movement retained its offset or destination collision.");
		cancelled.Update(0.28f, 0, -1, false);
		cancelled.Update(1, 0, 0, false);
		Require(cancelled.GetPlayer().GetX() == 240 && cancelled.GetPlayer().GetY() == 200, "Interrupted movement resumed its stale destination.");
		const LevelInfo directional = FixtureLevel(engine, "motion-directional-" + std::to_string(static_cast<i32>(edition)), { { 0, 240, 240, 0 }, { 2, 640, 400, 0 } }, 0, edition);
		cell(directional, base + 1, OccupancyCode::Wall, 1);
		LevelScene wall(engine, pack, directional, 1);
		wall.Update(0.28f, 1, 0, false);
		Require(wall.GetPlayer().GetX() == 280, "Matching directional-wall entry was blocked.");
		wall.Update(0.1f, 0, -1, false);
		Require(wall.GetPlayer().GetX() == 280 && wall.GetPlayer().GetY() == 240, "Invalid source direction escaped the directional tile.");
		wall.Update(0.28f, 1, 0, false);
		Require(wall.GetGrid().GetOccupancy(base + 1) == OccupancyCode::Wall && wall.GetGrid().GetReservationOwner(base + 1) == -1, "Leaving a directional tile lost its original wall.");
		constexpr u8 nativeDirections[] = { 2, 3, 1, 4, 2, 3, 1, 4 };
		for (u8 mask = 1; mask <= 4; ++mask)
		{
			for (i32 facing = 0; facing < 8; ++facing)
			{
				const LevelInfo masked = FixtureLevel(engine, "motion-mask-" + std::to_string(static_cast<i32>(edition)) + "-" + std::to_string(mask) + "-" + std::to_string(facing), { { 0, 240, 240, 0 }, { 2, 640, 400, 0 } }, 0, edition);
				cell(masked, base, OccupancyCode::Player, mask);
				LevelScene filtered(engine, pack, masked, 1);
				filtered.Update(0.04f, Directions[facing][0], Directions[facing][1], false);
				const bool moved = filtered.GetPlayer().GetX() != 240 || filtered.GetPlayer().GetY() != 240;
				Require(moved == (nativeDirections[facing] == mask), "Source direction filtering differs from native 0x417D10.");
			}
		}
		const LevelInfo blockedWall = FixtureLevel(engine, "motion-blocked-wall-" + std::to_string(static_cast<i32>(edition)), { { 0, 240, 240, 0 }, { 2, 640, 400, 0 } }, 0, edition);
		cell(blockedWall, base + 1, OccupancyCode::Wall, 0);
		cell(blockedWall, base - 22, OccupancyCode::Wall, 0);
		LevelScene blockedScene(engine, pack, blockedWall, 1);
		blockedScene.Update(0.1f, 1, 0, false);
		Require(blockedScene.GetPlayer().GetX() == 240 && blockedScene.GetGrid().GetOccupancy(base + 1) == OccupancyCode::Wall, "Blocked entry changed a scenery wall or moved Sven.");
		blockedScene.Update(0.28f, 1, -1, false);
		blockedScene.Update(1, 0, 0, false);
		Require(blockedScene.GetPlayer().GetX() == 280 && blockedScene.GetPlayer().GetY() == 200, "Diagonal movement added non-native adjacent-cell corner restrictions.");
		const LevelInfo blocked = FixtureLevel(engine, "motion-action-" + std::to_string(static_cast<i32>(edition)), { { 0, 240, 240, 0 }, { 2, 280, 240, 0 } }, 0, edition);
		LevelScene action(engine, pack, blocked, 1);
		action.GetObjects()[1].Temperament = 0;
		action.Update(0, 0, 0, false);
		action.Update(0.01f, 1, 0, true);
		Require(action.GetPlayer().GetX() == 240 && action.GetPlayerDraw().X == 280 && action.GetGrid().GetOccupancy(base + 1) == OccupancyCode::Sheep, "Blocked movement failed to start the animal action or overwrote animal collision.");
		const LevelInfo movingAction = FixtureLevel(engine, "moving-action-" + std::to_string(static_cast<i32>(edition)), { { 0, 240, 240, 0 }, { 2, 320, 240, 0 } }, 0, edition);
		LevelScene moving(engine, pack, movingAction, 1);
		moving.GetObjects()[1].Temperament = 0;
		moving.Update(0.1f, 1, 0, false);
		moving.Update(0.5f, 1, 0, true);
		Require(moving.GetPlayer().GetX() == 280 && moving.GetPlayerDraw().X == 280, "Action pressed while moving replayed after reaching an animal.");
		moving.Update(0, 1, 0, false);
		moving.Update(0.001f, 1, 0, true);
		Require(moving.GetPlayerDraw().X == 320, "A fresh action at the blocked destination was ignored.");
		cell(blocked, base, OccupancyCode::Player, 1);
		LevelScene filteredRepeat(engine, pack, blocked, 1);
		filteredRepeat.GetObjects()[1].Temperament = 0;
		filteredRepeat.Update(0, 0, 0, false);
		filteredRepeat.Update(0.001f, 1, 0, true);
		const Sprite* interaction = filteredRepeat.GetPlayerDraw().Sprite;
		filteredRepeat.Update(0, 0, 1, false);
		filteredRepeat.Update(0, 0, 1, true);
		bool completed = false;
		for (i32 tick = 0; tick < 2000 && !completed; ++tick)
		{
			filteredRepeat.Update(0.01f, 0, 1, true);
			completed = filteredRepeat.TakeScoreDelta() > 0;
		}
		Require(completed && filteredRepeat.GetPlayerDraw().Sprite == interaction, "Queued repeat used raw input rather than the direction accepted by the source cell.");
	}
	LevelPackInfo multiplayer = source;
	multiplayer.Edition = GameEdition::XXX;
	const LevelInfo shared = FixtureLevel(engine, "motion-reservation-owners", { { 0, 240, 240, 0 }, { 0, 400, 320, 0 }, { 2, 640, 400, 0 } }, 0, GameEdition::XXX);
	LevelScene owners(engine, multiplayer, shared, 1, nullptr, 2);
	const i32 otherBase = owners.GetObjects().GetPlayer(1).GetState().Tile;
	owners.Update(0.04f, std::array<PlayerInput, 2>{ PlayerInput{ 1, 0, false }, PlayerInput{ 0, -1, false } });
	owners.GetPlayer().CancelPlayerStep();
	Require(owners.GetGrid().GetOccupancy(otherBase - 22) == OccupancyCode::PlayerReservation && owners.GetGrid().GetReservationOwner(otherBase - 22) == static_cast<i32>(owners.GetObjects().GetPlayer(1).GetIndex()), "Cancelling one player released the other player's reservation.");
	Log("PASS: v6/v7 native 64-way movement transitions, motion-facing sprites, key release, strict boundaries, corner turns, cancellation, directional walls, blocked actions and reservation ownership.");
}

void CheckEditionFoundation(MudGE& engine, const fpath& saves)
{
	const GameData& data = engine.GetData();
	Require(data.HasEdition(GameEdition::Zwo) && data.HasEdition(GameEdition::XXX), "Both edition catalogs must be available.");
	Require(data.GetThemeEdition() == GameEdition::XXX, "Combined catalog must use the XXX theme.");
	Require(data.EditionAsset(GameEdition::Zwo, "Sven2/Sven2.xml") == "ZWO/Sven2/Sven2.xml", "Zwo namespace was lost.");
	Require(data.EditionAsset(GameEdition::XXX, "Sven2/Sven2.xml") == "XXX/SvenXXX/Sven2.xml", "XXX core namespace translation changed.");
	Require(data.AssetPath("Sven2/Sven2.xml") == data.AssetPath("ZWO/Sven2/Sven2.xml"), "Legacy Zwo paths do not resolve in the new asset layout.");
	for (GameEdition edition : { GameEdition::Zwo, GameEdition::XXX })
	{
		for (cstr sound : { "Tune01", "Tune02", "SfxUFO" })
			data.AssetPath(data.SoundPath(sound, edition));
	}
	std::set<str> ids;
	std::array<size_t, 2> counts{};
	size_t helpMarkers = 0;
	auto validateDraws = [&](const std::vector<SpriteDraw>& draws) {
		Require(!draws.empty(), "Scene or UI emitted no draw commands.");
		for (const SpriteDraw& draw : draws)
			Require(draw.Sprite && draw.Frame >= 0 && draw.Frame < draw.Sprite->Rows * draw.Sprite->Columns, "Initial scene/UI frame exceeds its sheet.");
	};
	for (const LevelPackInfo& pack : data.GetPacks())
	{
		Require(!pack.Id.empty() && ids.insert(pack.Id).second, "Edition pack save IDs collide.");
		for (const LevelSetInfo& set : pack.LevelSets)
		{
			for (const LevelInfo& level : set.Levels)
			{
				try
				{
					const bytestream bytes = data.ReadAsset(level.Map);
					const LevelFile file(bytes, level.Map);
					const bool xxx = pack.Edition == GameEdition::XXX;
					Require(file.GetVersion() == (xxx ? 7 : 6), "Catalog edition/version mismatch.");
					size_t players = 0, targets = 0;
					for (const LevelObjectRecord& record : file.GetObjects())
					{
						players += record.Kind() == static_cast<i32>(ObjectKind::Player);
						const CharacterDefinition* character = FindCharacterDefinition(static_cast<ObjectKind>(record.Kind()));
						targets += character && character->Category == CharacterCategory::Animal;
					}
					Require(players == (xxx ? size_t{ 2 } : size_t{ 1 }), "Unexpected shipped player spawn count.");
					for (const LevelCell& cell : file.GetCells())
					{
						helpMarkers += cell.HelpId != 0;
						Require(cell.Spawner == SpawnerCode::None || IsSpawner(cell.Spawner), "Help ID contaminated the spawner code.");
					}
					if (xxx)
					{
						std::array<std::array<i32, 4>, LevelFile::ObjectCount> layouts{};
						std::memcpy(layouts.data(), bytes.data() + bytes.size() - sizeof(layouts), sizeof(layouts));
						for (size_t index = 0; index < layouts.size(); ++index)
							Require(file.GetExtension(index) == layouts[index], "A v7 help layout word was lost.");
						bytestream truncated = bytes;
						truncated.pop_back();
						bool rejected = false;
						try
						{
							LevelFile invalid(truncated, level.Map);
						}
						catch (const std::runtime_error&)
						{
							rejected = true;
						}
						Require(rejected, "Truncated v7 extension was accepted.");
					}
					LevelScene scene(engine, pack, level, 1);
					Require(scene.GetGameplayEdition() == GameEdition::XXX, "Source pack selected legacy gameplay instead of the shared XXX runtime.");
					Require(scene.GetPlayerCount() == 1 && scene.GetObjects().Find(ObjectKind::Player).size() == 1, "Single-player load retained the second actor.");
					Require(scene.GetRemainingTargets() == static_cast<i32>(targets), "Optional actors changed completion target count.");
					validateDraws(scene.Draws());
					++counts[xxx ? 1 : 0];
				}
				catch (const std::exception& error)
				{
					throw std::runtime_error(level.Map + ": " + error.what());
				}
			}
		}
	}
	Require(counts[0] >= 105 && counts[1] == 129 && helpMarkers > 0, "Shipped catalog/help coverage is incomplete.");
	GameUi ui(engine, saves);
	for (GameUi::Page page : { GameUi::Page::Title, GameUi::Page::GetPacks, GameUi::Page::Pause, GameUi::Page::Quit })
	{
		ui.Show(page);
		validateDraws(ui.Draws(0.2f));
	}
	WindowInput::Result next;
	next.Right = true;
	ui.Show(GameUi::Page::Help);
	for (i32 tab = 0; tab < 5; ++tab)
	{
		validateDraws(ui.Draws(0.2f));
		ui.Handle(next);
	}
	ui.Show(GameUi::Page::Highscores);
	validateDraws(ui.Draws(0.2f));
	WindowInput::Result cards;
	cards.Click = true;
	cards.Mouse = { 510, 30 };
	ui.Handle(cards);
	Require(ui.CardCount() == 39, "XXX collection must include all 33 Zwo poses and six new poses without duplicates.");
	const std::vector<ShagAnimation> zwoCards = ShagAnimation::Load(engine);
	for (size_t card = 0; card < zwoCards.size() / 2; ++card)
	{
		const bool original = card < 13;
		const i32 number = static_cast<i32>(original ? card + 2 : card - 13 + 1);
		std::ostringstream name;
		name << (original ? "Shag" : "NF");
		if (number < 10)
			name << '0';
		name << number;
		const GameData::SpriteDefinition& definition = data.GetSpriteDefinition(name.str(), GameEdition::XXX);
		const Sprite* modern = engine.LoadSprite(definition.File, definition.Rows, definition.Columns);
		const Sprite* legacy = zwoCards[card * 2].Sprite;
		Require(modern->Width == legacy->Width && modern->FrameHeight == legacy->FrameHeight && modern->Pixels == legacy->Pixels, "A Zwo card pose is missing or changed in the XXX collection.");
	}
	for (i32 card = 0; card < ui.CardCount(); ++card)
	{
		WindowInput::Result play;
		play.Confirm = true;
		ui.Handle(play);
		for (i32 frame = 0; frame < 120; ++frame)
			validateDraws(ui.Draws(1.0f / 15));
		const std::vector<str> sounds = ui.TakeSounds();
		Require(sounds.size() == 1, "A collection card did not emit its own animation sound.");
		data.AssetPath(sounds.front());
		ui.Handle(next);
	}
	for (bool complete : { false, true })
	{
		RoundResult result;
		result.EpisodeComplete = complete;
		ui.ShowResults(result);
		validateDraws(ui.Draws(0.2f));
	}
	Log("PASS: edition foundation: " + std::to_string(counts[0]) + " Zwo + 129 XXX maps, separate pack IDs/assets, v7 layouts/help IDs, single-player spawn filtering and XXX UI/fonts/cards. Gameplay parity is not established by this smoke check.");
}

void CheckCatalog(MudGE& engine, size_t minimum = 105)
{
	size_t levels = 0;
	std::set<str> soundAssets;
	for (const auto& pack : engine.GetData().GetPacks())
	{
		for (const auto& set : pack.LevelSets)
		{
			for (const auto& level : set.Levels)
			{
				try
				{
					LevelScene scene(engine, pack, level);
					for (i32 frame = 0; frame < 1200; ++frame)
						scene.Update(1.0f / 60, 0, 0, false);
					scene.TimeExpired();
					for (i32 frame = 0; frame < 600; ++frame)
						scene.Update(1.0f / 60, 0, 0, false);
					for (const auto& draw : scene.Draws())
					{
						Require(draw.Sprite != nullptr, "Null actor sprite.");
						Require(draw.Frame >= 0 && draw.Frame < draw.Sprite->Rows * draw.Sprite->Columns, "Actor frame exceeds its sheet.");
					}
					for (const auto& sound : scene.TakeSounds())
						if (soundAssets.insert(sound).second)
							engine.GetData().AssetPath(sound);
					++levels;
				}
				catch (const std::exception& error)
				{
					throw std::runtime_error(level.Map + ": " + error.what());
				}
			}
		}
	}
	Require(levels >= minimum, "Catalog fixture count is below the required minimum.");
	Log("PASS: " + std::to_string(levels) + " folder levels load and simulate with valid sprite frames and sound references.");
}
} // namespace

i32 main(i32 argc, char** argv)
{
	try
	{
		const bool preview = argc == 3 && str(argv[1]) == "--catalog-preview";
		const bool catalogAudio = argc == 3 && str(argv[1]) == "--catalog-audio";
		const bool editionVisual = argc == 2 && str(argv[1]) == "--edition-visual";
		const bool editions = argc == 2 && (str(argv[1]) == "--editions" || editionVisual);
		const auto trace = argc == 3 && str(argv[1]) == "--trace" ? std::filesystem::absolute(std::filesystem::u8path(argv[2])) : fpath{};
		const auto catalog = argc == 3 && (str(argv[1]) == "--catalog" || preview || catalogAudio) ? std::filesystem::absolute(std::filesystem::u8path(argv[2])) : fpath{};
		wchar_t executable[32768]{};
		GetModuleFileNameW(nullptr, executable, 32768);
		const auto getRoot = fpath(executable).parent_path().parent_path().parent_path().parent_path().parent_path();
		const auto saves = getRoot / L"rebuild" / L"runtime" / L"checks" / (std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
		std::filesystem::create_directories(saves);
		std::filesystem::current_path(saves);
		CheckScalingCoordinates();
		if (argc == 2 && str(argv[1]) == "--upscaler")
		{
			CheckUpscalerGpu();
			Log("GPU captures: " + saves.string());
			return 0;
		}
		if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
			throw std::runtime_error("Cannot initialize COM.");
		struct ComLifetime
		{
			~ComLifetime()
			{
				CoUninitialize();
			}
		} apartment;
		const auto fixtureData = saves / L"data";
		if (catalog.empty())
			LinkFixtureData(editions ? getRoot / L"assets" : getRoot / L"assets" / L"ZWO", fixtureData);
		MudGE engine(getRoot / L"games" / L"sven2", catalog.empty() ? fixtureData : catalog);
		engine.Construct();
		engine.InitializePlugins();
		engine.VerifyData();
		if (editions)
		{
			CheckEditionFoundation(engine, saves);
			if (editionVisual)
				engine.StartVideo(false);
			CheckPresentationCorrections(engine, saves, editionVisual);
			CheckSharedPlayerRuntime(engine);
			CheckPlayerMovement(engine);
			CheckCatalog(engine, 234);
			CheckMenuPresentation(engine, saves, editionVisual);
			engine.Shutdown();
			return 0;
		}
		if (!trace.empty())
		{
			WriteBehaviorTrace(engine, trace);
			engine.Shutdown();
			return 0;
		}
		if (!catalog.empty())
		{
			if (catalogAudio)
			{
				engine.StartVideo(false);
				Require(engine.StartAudio(), "Catalog audio backend failed to initialize.");
				Require(engine.PlayMusic("Sven2/sfx/Sven2_music.ogg"), "Menu music failed.");
				for (i32 pass = 0; pass < 3; ++pass)
				{
					for (const auto& pack : engine.GetData().GetPacks())
					{
						for (const auto& set : pack.LevelSets)
						{
							for (const auto& level : set.Levels)
							{
								LevelScene scene(engine, pack, level, 1);
								Log("Testing ambient stream: " + level.Music);
								Require(engine.PlayMusic(level.Music), "Level ambient stream failed.");
								engine.DrawFrame(scene.Draws());
								const auto started = GetTickCount64();
								while (GetTickCount64() - started < 1200)
								{
									engine.Tick();
									scene.Update(1.0f / 60, 0, 0, false);
									Sleep(10);
								}
								engine.DrawFrame(scene.Draws());
							}
						}
					}
				}
				engine.StopMusic();
				engine.StopMusic(); // Repeated cleanup must be harmless.
				Require(engine.PlayMusic("Sven2/sfx/Sven2Highscore.ogg"), "Highscore music failed after campaign streams.");
				Require(engine.PlayMusic("Sven2/sfx/Sven2_music.ogg"), "Returning to menu music failed.");
				engine.StopMusic();
				Log("PASS: sequential catalog ambient streams.");
			}
			CheckLabFixtures(engine);
			CheckCatalog(engine, 1);
			if (preview)
			{
				engine.StartVideo(false);
				size_t index = 0;
				for (const auto& pack : engine.GetData().GetPacks())
				{
					for (const auto& set : pack.LevelSets)
					{
						for (const auto& level : set.Levels)
						{
							LevelScene scene(engine, pack, level, 1);
							engine.DrawFrame(scene.Draws(), ("pack-level-" + std::to_string(++index) + ".bmp").c_str());
						}
					}
				}
				Log("PASS: package previews in " + saves.string());
			}
			engine.Shutdown();
			Log("PASS: package catalog checks.");
			return 0;
		}
		Require(engine.GetData().GetPacks().size() >= 5, "Original package folders were not discovered.");
		Require(engine.GetData().Text("SelectPacks") == u8"W\u00c4HLE LEVELPACK", "Legacy XML encoding was not decoded.");
		CheckObjectOwnership(engine);
		CheckAnimalMoodEffects(engine);
		CheckDebugMenu(engine);
		CheckScores(saves);
		CheckGlyphs(engine);
		CheckMenus(engine, saves);
		CheckDepthOrdering(engine);
		CheckSerializedPowerups(engine);
		CheckMovement(engine);
		CheckInteraction(engine);
		CheckReportedGameplay(engine);
		CheckEnemyAcquisition(engine);
		CheckRound(engine);
		CheckLifeChanges(engine);
		CheckPlayerItems(engine);
		CheckAttackReservations(engine);
		CheckEffectAndAnimationStates(engine);
		CheckEnemyHeadings(engine);
		CheckEnemyItems(engine);
		CheckDemonSheep(engine);
		CheckFarmerOccupancy(engine);
		CheckGirlBeamReturn(engine);
		CheckTimeout(engine);
		CheckCatalog(engine);
		if (argc == 2 && str(argv[1]) == "--visual")
		{
			engine.StartVideo(false);
			CheckDebugMenu(engine, true);
			CheckDepthOrdering(engine, true);
			CheckEnemyItems(engine, true);
			CheckDemonSheep(engine, true);
			CheckGirlBeamReturn(engine, true);
			CheckPlayerItems(engine, true);
			CheckLifeChanges(engine, true);
			CheckEffectAndAnimationStates(engine, true);
			CheckEnemyHeadings(engine, true);
			GameUi ui(engine, saves / L"visual-scores");
			using Page = GameUi::Page;
			engine.Tick();
			engine.DrawFrame(ui.Draws(0), "menu.bmp");
			WindowInput::Result exitClick;
			exitClick.Click = true;
			exitClick.Mouse = { 650, 555 };
			ui.Handle(exitClick);
			engine.DrawFrame(ui.Draws(0), "menu-exit.bmp");
			ui.Show(Page::GetPacks);
			engine.Tick();
			engine.DrawFrame(ui.Draws(0), "packs.bmp");
			ui.Show(Page::Help);
			for (i32 tab = 0; tab < 4; ++tab)
			{
				const auto name = "help-" + std::to_string(tab) + ".bmp";
				engine.DrawFrame(ui.Draws(0), name.c_str());
				WindowInput::Result next;
				next.Right = true;
				ui.Handle(next);
			}
			ui.Show(Page::Highscores);
			engine.DrawFrame(ui.Draws(0), "highscores.bmp");
			const auto* paper = engine.LoadSprite("Sven2/gfx/Help_BG.jpg", 1, 1);
			CheckCapturePixel("highscores.bmp", 110, 65, paper->Pixels[65 * paper->Width + 110]);
			WindowInput::Result collectionTab;
			collectionTab.Click = true;
			collectionTab.Mouse = { 510, 40 };
			ui.Handle(collectionTab);
			engine.DrawFrame(ui.Draws(0), "cards-first.bmp");
			WindowInput::Result previousCard;
			previousCard.Left = true;
			ui.Handle(previousCard);
			engine.DrawFrame(ui.Draws(0), "cards-last.bmp");
			CheckCapturePixel("cards-last.bmp", 510, 65, paper->Pixels[65 * paper->Width + 510]);
			ui.ShowResults({ 1250, 1, 180, 3, 3, 0, 0, 0, false, false });
			engine.DrawFrame(ui.Draws(0), "results.bmp");
			ui.ShowResults({ 1250, 1, 180, 3, 3, 0, 0, 0, true, false, 17, 3 });
			engine.DrawFrame(ui.Draws(0.1f), "pack-complete.bmp");
			WindowInput::Result continueResult;
			continueResult.Confirm = true;
			ui.Handle(continueResult);
			WindowInput::Result enterName;
			enterName.Text = "Tester";
			ui.Handle(enterName);
			engine.DrawFrame(ui.Draws(0), "name-entry.bmp");
			const auto& getPacks = engine.GetData().GetPacks();
			const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack) { return pack.Name == "XS"; });
			GameSession session(engine, *xs, xs->LevelSets.front());
			engine.DrawFrame(session.Draws(), "level.bmp");
			ui.Show(Page::Pause);
			engine.DrawFrame(ui.Draws(0, &session.Draws()), "pause.bmp");
			ui.Show(Page::Title);
			ui.Handle(exitClick);
			engine.DrawFrame(ui.Draws(0, &session.Draws()), "menu-exit-after-level.bmp");
			const auto walking = FixtureLevel(engine, "visual-walk", { { 0, 240, 200, 0 }, { 2, 640, 400, 0 } });
			LevelScene east(engine, *xs, walking, 1), west(engine, *xs, walking, 1);
			east.Update(0.1f, 1, 0, false);
			west.Update(0.1f, -1, 0, false);
			engine.DrawFrame(east.Draws(), "running-east.bmp");
			engine.DrawFrame(west.Draws(), "running-west.bmp");
			const auto cold = FixtureLevel(engine, "visual-frigida", { { 0, 240, 200, 0 }, { 2, 240, 160, 0 } });
			LevelScene frigida(engine, *xs, cold, 1);
			for (i32 frame = 0; frame < 90; ++frame)
				frigida.Update(1.0f / 60, 0, 0, frame % 8 == 0);
			engine.DrawFrame(frigida.Draws(), "frigida.bmp");
			const auto enemy = FixtureLevel(engine, "visual-fight", { { 0, 240, 200, 0 }, { 4, 280, 200, 6 }, { 2, 640, 400, 0 } });
			LevelScene fight(engine, *xs, enemy, 1);
			const auto* cloud = engine.LoadSprite("Sven2/gfx/anim_cloud.bmp", 10, 1);
			for (i32 frame = 0; frame < 600; ++frame)
			{
				fight.Update(1.0f / 60, 0, 0, false);
				const auto& draws = fight.Draws();
				if (std::any_of(draws.begin(), draws.end(), [&](const auto& draw) { return draw.Sprite == cloud; }))
				{
					engine.DrawFrame(draws, "fight.bmp");
					break;
				}
			}
			const auto animals = FixtureLevel(engine, "visual-meters", { { 0, 240, 200, 0 }, { 12, 240, 160, 0 }, { 11, 400, 160, 0 }, { 13, 560, 160, 0 } });
			LevelScene meters(engine, *xs, animals, 1);
			engine.DrawFrame(meters.Draws(), "animal-meters.bmp");
			meters.Update(0.2f, 0, 0, true);
			engine.DrawFrame(meters.Draws(), "goat-interaction.bmp");
			Require(engine.StartAudio(), "Native regression audio backend failed to initialize.");
			for (cstr name : { "SfxDogFoundSven", "SfxGuyFoundBeer", "SfxLarsDrink", "SfxLarsSleep" })
				Require(engine.PlaySample(engine.GetData().SoundPath(name)), "Native enemy sample playback request failed.");
			Log("PASS: native enemy notice, drink and sleep sample playback requests.");
			Require(engine.PlaySample(engine.GetData().SoundPath("SfxUFO"), true), "Native UFO loop failed to start.");
			const auto loopStarted = GetTickCount64();
			while (GetTickCount64() - loopStarted < 6200)
			{
				engine.Tick();
				Sleep(10);
			}
			Require(engine.LoopedSoundPlaying(), "UFO sound stopped after the original five-second recording instead of looping.");
			engine.StopLoopedSound();
			Require(!engine.LoopedSoundPlaying(), "UFO channel did not stop on lifecycle cleanup.");
			Require(engine.PlaySample(engine.GetData().SoundPath("SfxUFO"), true), "Stopped UFO loop failed to restart.");
			engine.StopLoopedSound();
			Log("PASS: native UFO sample loops beyond 6 seconds, stops and restarts.");
			Log("PASS: native UI captures in " + saves.string());
		}
		engine.Shutdown();
		Log("PASS: all remake checks.");
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "FAIL: " << error.what() << std::endl;
		return 1;
	}
}
