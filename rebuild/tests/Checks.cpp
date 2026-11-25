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
LevelInfo FixtureLevel(MudGE& engine, const str& name, std::initializer_list<FixtureActor> actors, i32 pickup = 0, GameEdition edition = GameEdition::Zwo, u32 flags = 0)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack) { return pack.Name == "XS"; });
	auto level = xs->LevelSets.front().Levels.front();
	// v6 layout is documented in docs/VERIFIED_FORMAT.md and LevelScene's parser.
	bytestream data(16 + 9048 + 25600);
	auto integer = [&](size_t offset, i32 value) { std::memcpy(data.data() + offset, &value, 4); };
	auto real = [&](size_t offset, f32 value) { std::memcpy(data.data() + offset, &value, 4); };
	integer(0, edition == GameEdition::XXX ? 7 : 6);
	integer(8, static_cast<i32>(flags | (edition == GameEdition::XXX ? 1u : 0u))); // UFO visibility has opposite flag polarity in v6 and v7.
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
	const Sprite* clock = engine.LoadSprite("Sven2/gfx/timer.bmp", 1, 1);
	auto hudLives = [&](const std::vector<SpriteDraw>& draws) {
		// Animated icons precede the HUD and can round to its exact Y coordinate.
		const auto hud = std::find_if(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == clock; });
		Require(hud != draws.end(), "Life display is missing its HUD layer.");
		return std::count_if(hud, draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == icon && draw.Y == 565; });
	};
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
	Require(hudLives(graceDraws) == 2,
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
	Require(hudLives(gainDraws) == 3,
	        "Extra life appeared in the HUD before its animation arrived.");
	Require(std::any_of(gainDraws.begin(), gainDraws.end(), [&](const auto& draw) { return draw.Sprite == icon && draw.X == 96 && draw.Y < 0; }),
	        "Extra life did not start falling from above the screen.");
	game.Update(0.35f, 0, 0, false);
	if (capture)
		engine.DrawFrame(game.Draws(), "life-gain.bmp");
	game.Update(2.0f, 0, 0, false);
	const auto& landed = game.Draws();
	Require(hudLives(landed) == 4,
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
	Require(hudLives(lossDraws) == 3,
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
		Require(ui.GetPlayerCount() == players, "Selected player mode was lost while browsing a Zwo episode.");
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
	Require(completed == 2 && heldScene.GetPlayerDraw().Sprite == stand, "Holding action automatically repeated a XXX interaction.");
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

void CheckInteractionOwnership(MudGE& engine)
{
	const GameData& data = engine.GetData();
	const LevelPackInfo& source = *std::find_if(data.GetPacks().begin(), data.GetPacks().end(), [](const LevelPackInfo& pack) { return pack.Name == "XS" && pack.Edition == GameEdition::Zwo; });
	for (GameEdition edition : { GameEdition::Zwo, GameEdition::XXX })
	{
		LevelPackInfo pack = source;
		pack.Edition = edition;
		const LevelInfo level = FixtureLevel(engine, "interaction-phase-" + std::to_string(static_cast<i32>(edition)), { { 0, 240, 240, 0 }, { 12, 280, 240, 0 } }, 0, edition);
		LevelScene entry(engine, pack, level, 1);
		entry.GetPlayer().SetInput({ 1, 0, false });
		entry.GetPlayer().AdvancePlayer(0, 1, 0);
		entry.GetPlayer().SetInput({ 1, 0, true });
		entry.GetPlayer().AdvancePlayer(0.1f, 1, 0);
		Require(entry.GetPlayerDraw().Frame == 2 && entry.TakeScoreDelta() == 0, "A newly accepted action did not advance in the same native animation phase.");
		LevelScene scene(engine, pack, level, 1);
		PlayerCharacter& player = scene.GetPlayer();
		player.SetInput({ 1, 0, false });
		player.AdvancePlayer(0, 1, 0);
		player.SetInput({ 1, 0, true });
		player.AdvancePlayer(0, 1, 0);
		Require(scene.GetObjects()[1].InteractionPlayer == 0, "Animal interaction did not claim its target.");
		scene.GetItems().GetPowerup() = { ItemCode::Rocket, 10, 0 };
		player.AdvancePlayer(0.3f, 0, 0);
		Require(scene.TakeScoreDelta() == 0 && scene.GetObjects()[1].InteractionPlayer == 0, "A mid-animation powerup changed the latched frame rate.");
		scene.GetItems().GetPowerup() = {};
		player.AdvancePlayer(0.26f, 0, 0);
		Require(scene.TakeScoreDelta() == 0 && scene.GetObjects()[1].InteractionPlayer == 0, "Completion was applied in the animation phase instead of the next controller phase.");
		player.AdvancePlayer(0, 0, 0);
		Require(scene.TakeScoreDelta() == 2 && scene.GetObjects()[1].Impatience == 4 && scene.GetObjects()[1].InteractionPlayer == -1, "Native ordinary completion score/mood/release differs from XXX.");
		player.SetInput({ 1, 0, false });
		player.AdvancePlayer(0, 1, 0);
		player.SetInput({ 1, 0, true });
		player.AdvancePlayer(1, 1, 0);
		player.InterruptInteraction();
		player.AdvancePlayer(0, 0, 0);
		Require(scene.TakeScoreDelta() == 0 && scene.GetObjects()[1].InteractionPlayer == -1, "Interrupting a completed pending animation left a lock or awarded stale score.");
		const LevelInfo turning = FixtureLevel(engine, "interaction-turn-abort-" + std::to_string(static_cast<i32>(edition)), { { 0, 240, 240, 0 }, { 12, 280, 240, 2 } }, 0, edition);
		LevelScene whistle(engine, pack, turning, 1);
		whistle.Update(0, 1, 0, false);
		whistle.Update(0.001f, 1, 0, true);
		Require(whistle.GetObjects()[1].State == ActorState::Turning, "Whistle interruption fixture did not start its turn.");
		whistle.GetPlayer().InterruptInteraction();
		Require(whistle.GetObjects()[1].State == ActorState::Idle && whistle.GetObjects()[1].TurnTo == -1 && whistle.GetObjects()[1].Idle == 250, "Interrupted whistle retained its target's turn animation.");
	}
	LevelPackInfo pack = source;
	pack.Edition = GameEdition::XXX;
	const LevelInfo level = FixtureLevel(engine, "interaction-contention", { { 0, 240, 240, 0 }, { 0, 280, 280, 0 }, { 12, 280, 240, 0 } }, 0, GameEdition::XXX);
	LevelScene scene(engine, pack, level, 1, nullptr, 2);
	std::array<PlayerInput, 2> input = { PlayerInput{ 1, 0, false }, PlayerInput{ 0, -1, false } };
	scene.Update(0, input);
	input[0].Action = input[1].Action = true;
	scene.Update(0.001f, input);
	Require(scene.GetObjects()[2].InteractionPlayer == 0, "Two players claimed one animal simultaneously.");
	scene.GetPlayer().InterruptInteraction();
	input[0] = {};
	input[1].Action = false;
	scene.Update(0, input);
	input[1].Action = true;
	scene.Update(0.001f, input);
	Require(scene.GetObjects()[2].InteractionPlayer == 1, "Released animal could not be claimed by the other player.");
	Log("PASS: animal ownership/contention, latched interaction rate, native completion phase/score and interrupted pending completion.");
}

void CheckSharedItems(MudGE& engine, bool capture)
{
	const GameData& data = engine.GetData();
	const LevelPackInfo& source = *std::find_if(data.GetPacks().begin(), data.GetPacks().end(), [](const LevelPackInfo& pack) { return pack.Name == "XS" && pack.Edition == GameEdition::Zwo; });
	for (GameEdition edition : { GameEdition::Zwo, GameEdition::XXX })
	{
		LevelPackInfo pack = source;
		pack.Edition = edition;
		const LevelInfo level = FixtureLevel(engine, "shared-items-" + std::to_string(static_cast<i32>(edition)), { { 0, 240, 240, 0 }, { 12, 640, 400, 0 }, { 2, 560, 400, 0 }, { 2, 480, 400, 0 } }, 0, edition);
		LevelScene scene(engine, pack, level, 1);
		const i32 tile = scene.GetObjects()[scene.GetPlayerIndex()].Tile;
		auto pickup = [&](ObjectKind kind) { scene.GetItems().SpawnItem(kind, tile); scene.Update(0, 0, 0, false); };
		constexpr i32 bonuses[] = { 5, 10, 15, 20, 25, 5 };
		constexpr i32 pitches[] = { 0, 47100, 50100, 53100, 56100, 0 };
		for (size_t gift = 0; gift < std::size(bonuses); ++gift)
		{
			pickup(ObjectKind::Gift);
			Require(scene.TakeTimeDelta() == bonuses[gift] && scene.TakeLivesGained() == 0 && scene.TakeScoreDelta() == 0, "XXX gift chain used the legacy random rewards.");
			const std::vector<SoundRequest> audio = scene.TakeAudioEvents();
			Require(std::any_of(audio.begin(), audio.end(), [&](const SoundRequest& event) { return event.Asset == data.SoundPath("SfxPositive", GameEdition::XXX) && event.Frequency == pitches[gift]; }), "Gift-chain playback rate did not follow native XXX pitches.");
		}
		pickup(ObjectKind::Rocket);
		pickup(ObjectKind::Gift);
		Require(scene.TakeTimeDelta() == 5 && scene.GetItems().GetPowerup().Code == ItemCode::Rocket, "Accepted non-gift did not reset the chain, or gift replaced the equipped effect.");
		pickup(ObjectKind::Rocket);
		Require(scene.GetItems().GetInteractionMultiplier() == 2 && scene.GetItems().GetPowerup().Code == ItemCode::Rocket && scene.GetItems().GetSharedRocketRemaining() == 10, "Second rocket failed to set the shared multiplier/private duration.");
		Require(!scene.GetObjects().Find(ObjectKind::RocketBurst).empty(), "Second rocket omitted its native launch effect.");
		const Sprite* rocket = engine.LoadSprite(data.EditionAsset(GameEdition::XXX, "Sven2/gfx/icons.bmp"), 5, 1);
		for (size_t burst : scene.GetObjects().Find(ObjectKind::RocketBurst))
			Require(scene.GetObjects()[burst].Draw.Sprite == rocket && scene.GetObjects()[burst].Draw.Frame == 0, "Second rocket launched a speed marker instead of the rocket.");
		if (capture)
			engine.DrawFrame(scene.Draws(), edition == GameEdition::Zwo ? "rocket-launch-zwo.bmp" : "rocket-launch-xxx.bmp");
		scene.GetItems().GetPowerup() = {};
		Require(scene.GetItems().GetInteractionMultiplier() == 2, "Replacing a personal effect cancelled the shared rocket multiplier.");
		scene.GetItems().UpdateItems(10);
		Require(scene.GetItems().GetInteractionMultiplier() == 2, "Shared rocket expired at zero instead of below zero.");
		scene.GetItems().UpdateItems(0.001f);
		Require(scene.GetItems().GetInteractionMultiplier() == 1, "Shared rocket multiplier did not expire.");
		for (size_t target = 1; target < 4; ++target)
		{
			scene.GetObjects()[target].Impatience = 3;
			scene.GetObjects()[target].Temperament = target == 3 ? 1 : 0;
		}
		pickup(ObjectKind::Flower);
		pickup(ObjectKind::Flower);
		Require(scene.GetObjects()[1].Impatience == 0 && scene.GetObjects()[2].Impatience == 0 && scene.GetObjects()[3].Impatience == 3, "Double flower missed distant animals or changed Frigida.");
		Require(scene.GetObjects().Find(ObjectKind::FlowerBurst).size() == 2, "Double flower omitted its attached mood effects.");
		pickup(ObjectKind::StinkMushroom);
		pickup(ObjectKind::StinkMushroom);
		Require(scene.GetObjects()[1].Impatience == 10 && scene.GetObjects()[2].Impatience == 10 && scene.GetObjects()[3].Impatience == 3, "Double stink missed distant animals or changed Frigida.");
		pickup(ObjectKind::Gift);
		Require(scene.TakeTimeDelta() == 5, "Repeated non-gift did not reset the shared gift chain.");
		scene.GetItems().GetPowerup() = { ItemCode::AlienPower, 15, 0 };
		scene.GetItems().SpawnItem(ObjectKind::Rocket, tile);
		scene.Update(0, 0, 0, false);
		Require(scene.GetItems().GetPowerup().Code == ItemCode::AlienPower && scene.GetObjects().Find(ObjectKind::Rocket).size() == 1, "Alien power consumed/replaced an incompatible pickup.");
		scene.GetItems().RemoveItem(tile);
		pickup(ObjectKind::Gift);
		Require(scene.TakeTimeDelta() == 10, "Rejected pickup reset the gift chain protected by alien power.");
		scene.GetItems().ResetGiftChain();
		pickup(ObjectKind::Gift);
		Require(scene.TakeTimeDelta() == 5, "Damage-chain reset did not restart gifts at five seconds.");
		scene.Update(4, 0, 0, false);
		Require(scene.GetObjects().Find(ObjectKind::RocketBurst).empty() && scene.GetObjects().Find(ObjectKind::FlowerBurst).empty() && scene.GetObjects().Find(ObjectKind::SmellBurst).empty(), "Repeated-pickup particles outlived their native lifetime.");
		const LevelInfo enemies = FixtureLevel(engine, "shared-slow-" + std::to_string(static_cast<i32>(edition)), { { 0, 80, 80, 0 }, { 4, 320, 320, 2 }, { 5, 400, 320, 2 }, { 6, 480, 320, 2 }, { 3, 560, 320, 2 }, { 12, 640, 400, 0 } }, 0, edition);
		LevelScene slow(engine, pack, enemies, 1), normal(engine, pack, enemies, 1);
		const i32 playerTile = slow.GetObjects()[slow.GetPlayerIndex()].Tile;
		for (i32 pickupCount = 0; pickupCount < 2; ++pickupCount)
		{
			slow.GetItems().SpawnItem(ObjectKind::SlowMushroom, playerTile);
			slow.Update(0, 0, 0, false);
		}
		Require(slow.GetItems().GetPowerup().Code == ItemCode::None && slow.GetItems().GetEnemyMotionScale() == 0.5f && slow.GetItems().GetSharedSlowRemaining() == 30, "Second slow mushroom did not consume its private effect and start shared slowdown.");
		Require(slow.GetObjects().Find(ObjectKind::SlowNotice).size() == 4 && !slow.GetObjects().Find(ObjectKind::SlowBubble).empty(), "Shared slowdown omitted enemy notices/bubbles.");
		std::array<f32, 4> positions{};
		for (size_t target = 1; target <= 4; ++target)
		{
			for (LevelScene* test : { &slow, &normal })
			{
				WorldObject& actor = test->GetObjects()[target];
				actor.State = ActorState::Walking;
				actor.TargetX = actor.X + 40;
				actor.TargetY = actor.Y;
				actor.Hunting = false;
				actor.Age = 0;
			}
			positions[target - 1] = slow.GetObjects()[target].X;
		}
		slow.Update(0.01f, 1, 0, false);
		normal.Update(0.01f, 1, 0, false);
		for (size_t target = 1; target <= 4; ++target)
		{
			Require(std::abs((slow.GetObjects()[target].X - positions[target - 1]) * 2 - (normal.GetObjects()[target].X - positions[target - 1])) < 0.001f, "Shared slowdown did not halve an enemy's movement.");
			Require(std::abs(slow.GetObjects()[target].Age * 2 - normal.GetObjects()[target].Age) < 0.001f, "Shared slowdown did not halve an enemy's walk animation.");
		}
		Require(slow.GetPlayer().GetX() == normal.GetPlayer().GetX(), "Shared enemy slowdown incorrectly slowed Sven.");
		slow.GetItems().UpdateItems(30);
		Require(slow.GetItems().GetEnemyMotionScale() == 1, "Shared enemy slowdown did not expire.");
	}
	Log("PASS: v6/v7 XXX gift rewards/pitches/reset/immunity, shared repeat pickups, distant animal moods/particles, all enemy slowdown rates and independent expiry.");
}

void CheckArtifactAndMultiplier(MudGE& engine, bool capture)
{
	const GameData& data = engine.GetData();
	const LevelPackInfo& source = *std::find_if(data.GetPacks().begin(), data.GetPacks().end(), [](const LevelPackInfo& pack) { return pack.Name == "XS" && pack.Edition == GameEdition::Zwo; });
	for (GameEdition edition : { GameEdition::Zwo, GameEdition::XXX })
	{
		LevelPackInfo pack = source;
		pack.Edition = edition;
		for (bool frigida : { false, true })
		{
			const LevelInfo level = FixtureLevel(engine, "shared-multiplier-" + std::to_string(static_cast<i32>(edition)) + "-" + std::to_string(frigida), { { 0, 240, 240, 0 }, { frigida ? 2 : 12, 280, 240, 0 } }, 0, edition);
			LevelScene scene(engine, pack, level, 1);
			const i32 tile = scene.GetObjects()[scene.GetPlayerIndex()].Tile;
			for (i32 pickup = 0; pickup < 2; ++pickup)
			{
				scene.GetItems().SpawnItem(ObjectKind::Rocket, tile);
				scene.Update(0, 0, 0, false);
			}
			scene.GetItems().GetPowerup() = {};
			scene.GetObjects()[1].Temperament = frigida ? 2 : 0;
			scene.GetObjects()[1].Impatience = frigida ? 10 : 6;
			scene.Update(0.001f, 1, 0, true);
			i32 score = 0;
			for (i32 tick = 0; tick < 500 && !score; ++tick)
			{
				scene.Update(0.01f, 0, 0, false);
				score += scene.TakeScoreDelta();
			}
			if (frigida)
				Require(scene.GetObjects()[1].Freed && score == 30, "Shared rocket skipped Frigida's >=11 completion or native first-attempt reward.");
			else
				Require(score == 4 && scene.GetObjects()[1].Impatience == 2, "Shared rocket did not multiply ordinary mood drain and unequipped interaction score.");
		}
		const LevelInfo artifact = FixtureLevel(engine, "artifact-native-" + std::to_string(static_cast<i32>(edition)), { { 0, 240, 240, 0 }, { 12, 640, 400, 0 } }, static_cast<i32>(ItemCode::Artifact), edition);
		for (u32 flags : { 0u, 1u })
		{
			bytestream bytes = data.ReadAsset(artifact.Map);
			std::memcpy(bytes.data() + 8, &flags, sizeof(flags));
			Require(LevelFile(bytes, artifact.Map).GetHidesUfo() == (edition == GameEdition::XXX ? flags == 0 : flags != 0), "Level UFO visibility used the wrong source-format flag polarity.");
		}
		LevelScene scene(engine, pack, artifact, 1);
		bool returned = false, sawUp = false, sawDown = false, sawShip = false, sawShadow = false;
		const Sprite* ship = engine.LoadSprite(data.EditionAsset(GameEdition::XXX, "Sven2/gfx/ufo.bmp"), 13, 1);
		const Sprite* shadow = engine.LoadSprite(data.EditionAsset(GameEdition::XXX, "Sven2/gfx/ufo_shadow.bmp"), 13, 1);
		const GameData::SpriteDefinition& definition = data.GetSpriteDefinition("SvenBeamUpBack", GameEdition::XXX);
		const Sprite* beam = engine.LoadSprite(definition.File, definition.Rows, definition.Columns);
		for (i32 tick = 0; tick < 3000 && (!returned || scene.UfoActive()); ++tick)
		{
			scene.Update(0.01f, 0, 0, false);
			returned = returned || scene.GetItems().GetPowerup().Code == ItemCode::AlienPower;
			const SpriteDraw& player = scene.GetPlayerDraw();
			if (player.Sprite == beam)
			{
				sawUp = sawUp || player.Frame < 14;
				sawDown = sawDown || player.Frame == 21;
			}
			for (const SpriteDraw& draw : scene.Draws())
			{
				Require(draw.Sprite && draw.Frame >= 0 && draw.Frame < draw.Sprite->Rows * draw.Sprite->Columns, "Artifact/beam lifecycle emitted an invalid frame.");
				sawShip = sawShip || draw.Sprite == ship;
				sawShadow = sawShadow || draw.Sprite == shadow;
			}
			if (capture && tick == 850)
				engine.DrawFrame(scene.Draws(), edition == GameEdition::Zwo ? "artifact-shared-zwo.bmp" : "artifact-shared-xxx.bmp");
		}
		Require(returned && sawUp && sawDown && !scene.UfoActive(), "Shared XXX artifact did not complete its UFO/up/down/return lifecycle.");
		Require(sawShip && sawShadow, "Artifact beamed Sven without drawing the UFO and its shadow.");
	}
	LevelPackInfo pack = source;
	pack.Edition = GameEdition::XXX;
	const LevelInfo artifact = FixtureLevel(engine, "artifact-owner-two", { { 0, 80, 80, 0 }, { 0, 400, 240, 0 }, { 12, 640, 400, 0 } }, 0, GameEdition::XXX);
	LevelScene owners(engine, pack, artifact, 1, nullptr, 2);
	owners.GetItems().SpawnItem(ObjectKind::Artifact, owners.GetObjects().GetPlayer(1).GetState().Tile);
	bool returned = false;
	for (i32 tick = 0; tick < 3000 && (!returned || owners.UfoActive()); ++tick)
	{
		owners.Update(0.01f, 0, 0, false);
		LevelScene::PlayerScope owner(owners, 1);
		returned = returned || owners.GetItems().GetPowerup().Code == ItemCode::AlienPower;
	}
	Require(returned && !owners.UfoActive() && owners.GetItems().GetPowerup().Code == ItemCode::None, "Player-two artifact returned its effect to player one or got stuck.");
	if (capture)
	{
		Require(engine.StartAudio(), "Cannot validate the native pitched-sound channel.");
		Require(engine.PlaySample(data.SoundPath("SfxPositive", GameEdition::XXX), false, 56100), "Native pitched gift sample failed to play.");
	}
	Log("PASS: shared rocket scores/moods and Frigida threshold; full v6/v7 artifact frame lifecycle and second-player UFO ownership.");
}

void CheckMultiplayerFoundations(MudGE& engine, bool capture)
{
	const GameData& data = engine.GetData();
	LevelPackInfo pack = *std::find_if(data.GetPacks().begin(), data.GetPacks().end(), [](const LevelPackInfo& candidate) { return candidate.Name == "XS" && candidate.Edition == GameEdition::Zwo; });
	pack.Edition = GameEdition::XXX;
	const LevelInfo level = FixtureLevel(engine, "multiplayer-foundations", { { 0, 240, 240, 0 }, { 0, 400, 240, 0 }, { 12, 280, 240, 0 }, { 12, 400, 200, 0 }, { 4, 320, 320, 0 } }, 0, GameEdition::XXX);
	LevelScene scene(engine, pack, level, 1, nullptr, 2);
	Require(scene.GetObjects().FindNearbyPlayer(scene.GetObjects()[4], 2) == -1, "Native selector included the strict radius boundary.");
	Require(scene.GetObjects().FindNearbyPlayer(scene.GetObjects()[4], 3) == 1, "Equal native cell-distance ties did not prefer player two.");
	Require(scene.GetObjects().FindActiveInteractionPlayer(scene.GetObjects()[4]) == -1, "Idle players qualified for the active-interaction selector.");
	std::array<PlayerInput, 2> inputs = { PlayerInput{ 1, 0, false }, PlayerInput{ 0, -1, false } };
	scene.Update(0, inputs);
	inputs[0].Action = true;
	scene.Update(0.001f, inputs);
	Require(scene.GetObjects().FindActiveInteractionPlayer(scene.GetObjects()[4]) == 0, "Sole active interaction was replaced by the nearest idle player.");
	inputs[1].Action = true;
	scene.Update(0.001f, inputs);
	Require(scene.GetObjects().FindActiveInteractionPlayer(scene.GetObjects()[4]) == 1, "Two active players did not use the native nearest/tie selector.");
	scene.GetItems().GetPowerup() = { ItemCode::Rocket, 10, 0 };
	{
		LevelScene::PlayerScope owner(scene, 1);
		scene.GetItems().GetPowerup() = { ItemCode::Flower, 10, 0 };
	}
	const SpriteDraw first = scene.GetObjects().GetPlayer(0).GetState().Draw;
	const SpriteDraw second = scene.GetObjects().GetPlayer(1).GetState().Draw;
	const std::vector<SpriteDraw>& draws = scene.Draws();
	for (const SpriteDraw& actor : { first, second })
		Require(std::any_of(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == actor.Sprite && draw.X == actor.X && draw.Y == actor.Y; }), "A player's compound animation was omitted by the renderer.");
	const Sprite* power = engine.LoadSprite(data.EditionAsset(GameEdition::XXX, "Sven2/gfx/symbol_power.bmp"), 13, 1);
	const Sprite* flower = engine.LoadSprite(data.EditionAsset(GameEdition::XXX, "Sven2/gfx/symbol_flowers.bmp"), 8, 1);
	Require(std::any_of(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == power && draw.X == first.X + 28 && draw.Y == first.Y - 4; }), "Player one's effect detached from its own animation.");
	Require(std::any_of(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == flower && draw.X == second.X + 28 && draw.Y == second.Y - 4; }), "Player two's effect was omitted or attached to player one.");
	Require(scene.GetPlayer().GetNumber() == 0, "Rendering leaked an active-player scope.");
	if (capture)
		engine.DrawFrame(draws, "two-player-interactions.bmp");
	scene.Update(1, inputs);
	Require(scene.TakeScoreDelta() == 6 && scene.GetObjects()[2].InteractionPlayer == -1 && scene.GetObjects()[3].InteractionPlayer == -1, "Concurrent interactions did not release ownership and add private effect rewards to one shared score.");
	const LevelInfo moving = FixtureLevel(engine, "multiplayer-selector-offsets", { { 0, 240, 240, 0 }, { 0, 400, 240, 0 }, { 4, 320, 320, 0 }, { 12, 640, 400, 0 } }, 0, GameEdition::XXX);
	LevelScene offset(engine, pack, moving, 1, nullptr, 2);
	offset.Update(0.2f, 1, 0, false);
	Require(offset.GetObjects().FindNearbyPlayer(offset.GetObjects()[2], 4) == 0, "Native target selection ignored a player's fractional movement position.");
	bytestream bonusBytes = data.ReadAsset(moving.Map);
	const u32 flags = 0x02;
	std::memcpy(bonusBytes.data() + 8, &flags, sizeof(flags));
	{
		std::ofstream output(data.GetRoot() / std::filesystem::u8path(moving.Map), std::ios::binary | std::ios::trunc);
		output.write(reinterpret_cast<cstr>(bonusBytes.data()), static_cast<std::streamsize>(bonusBytes.size()));
	}
	LevelScene bonus(engine, pack, moving, 1, nullptr, 2);
	Require(bonus.GetBonusMode() == BonusMode::Alien && bonus.GetObjects().FindNearbyPlayer(bonus.GetObjects()[2], 500) == -1, "Normal native target selector remained active in multiplayer bonus mode.");
	Log("PASS: multiplayer foundation selectors, independent compound/effect rendering, concurrent ownership and shared score; menu/AI integration remains pending.");
}

void CheckMultiplayerEnemies(MudGE& engine)
{
	LevelPackInfo pack = *std::find_if(engine.GetData().GetPacks().begin(), engine.GetData().GetPacks().end(), [](const LevelPackInfo& candidate) { return candidate.Name == "XS" && candidate.Edition == GameEdition::Zwo; });
	pack.Edition = GameEdition::XXX;
	for (ObjectKind kind : { ObjectKind::Dog, ObjectKind::Farmer, ObjectKind::Girl, ObjectKind::Alien })
	{
		const str name = std::to_string(static_cast<i32>(kind));
		const LevelInfo notice = FixtureLevel(engine, "multiplayer-notice-" + name, { { 0, 80, 80, 0 }, { 0, 400, 240, 0 }, { static_cast<i32>(kind), 480, 320, 0 }, { 12, 400, 200, 0 }, { 2, 640, 400, 0 } }, 0, GameEdition::XXX);
		LevelScene scene(engine, pack, notice, 1, nullptr, 2);
		std::array<PlayerInput, 2> inputs{};
		if (kind == ObjectKind::Farmer)
		{
			inputs[1] = { 0, -1, false };
			scene.Update(0, inputs);
			inputs[1].Action = true;
		}
		scene.Update(0.001f, inputs);
		Require(scene.GetObjects()[2].Hunting && scene.GetObjects()[2].TargetPlayer == 1, "Enemy ignored player two's native notice/activity selection.");
		WorldObject& enemy = scene.GetObjects()[2];
		enemy.State = ActorState::Walking;
		enemy.Hunting = true;
		enemy.TargetPlayer = 0;
		enemy.TargetX = enemy.X + 40;
		enemy.TargetY = enemy.Y;
		scene.GetObjects().Get(2).Update(0.001f);
		Require(enemy.TargetPlayer == 0, "Enemy retargeted during an unfinished step.");

		const LevelInfo catchLevel = FixtureLevel(engine, "multiplayer-catch-" + name, { { 0, 80, 80, 0 }, { 0, 440, 240, 0 }, { static_cast<i32>(kind), 360, 240, 2 }, { 2, 640, 400, 0 } }, 0, GameEdition::XXX);
		LevelScene caught(engine, pack, catchLevel, 1, nullptr, 2);
		caught.Update(0, 0, 0, false);
		caught.GetItems().GetPowerup() = { ItemCode::Flower, 10, 0 };
		{
			LevelScene::PlayerScope owner(caught, 1);
			caught.GetItems().GetPowerup() = { ItemCode::Rocket, 10, 0 };
		}
		WorldObject& attacker = caught.GetObjects()[2];
		attacker.State = ActorState::Walking;
		attacker.TargetPlayer = 0;
		attacker.Hunting = true;
		attacker.Facing = 2;
		attacker.TargetX = attacker.X + 40;
		attacker.TargetY = attacker.Y;
		caught.GetObjects().Get(2).Update(1);
		Require(attacker.TargetPlayer == 1 && caught.GetCombat().Enemy == -1 && caught.GetItems().GetPowerup().Code == ItemCode::Flower, "Enemy catch interrupted the wrong player's combat/effect.");
		{
			LevelScene::PlayerScope owner(caught, 1);
			Require(caught.GetCombat().Enemy == 2 && caught.GetItems().GetPowerup().Code == ItemCode::None, "Caught player two did not enter its own combat or lose its effect.");
		}
		caught.GetObjects().Get(2).Update(20);
		{
			LevelScene::PlayerScope owner(caught, 1);
			Require(caught.GetCombat().Enemy == -1, "Player-two combat did not release its owner on completion.");
		}
		Require(caught.TakeLifeHits() == (kind == ObjectKind::Girl ? 0 : 1), "Player-two combat applied the wrong shared life consequence.");
		Require(caught.GetPlayer().GetNumber() == 0, "Enemy update leaked its target-player scope.");
	}
	const LevelInfo priority = FixtureLevel(engine, "multiplayer-catch-priority", { { 0, 240, 200, 0 }, { 0, 280, 200, 0 }, { 5, 240, 240, 1 }, { 2, 640, 400, 0 } }, 0, GameEdition::XXX);
	LevelScene scene(engine, pack, priority, 1, nullptr, 2);
	scene.Update(0, 0, 0, false);
	Require(scene.GetObjects().FindCaughtPlayer(scene.GetObjects()[2]) == 1, "Catch selector preferred player index over the native facing-cell order.");
	scene.GetObjects().Combats[1].Enemy = 2;
	Require(scene.GetObjects().FindCaughtPlayer(scene.GetObjects()[2]) == -1, "Catch selector skipped an unavailable first occupied neighbor to catch another player.");
	const LevelInfo concurrent = FixtureLevel(engine, "multiplayer-concurrent-catches", { { 0, 240, 240, 0 }, { 0, 480, 240, 0 }, { 4, 160, 240, 2 }, { 4, 560, 240, 6 }, { 2, 640, 400, 0 } }, 0, GameEdition::XXX);
	LevelScene battles(engine, pack, concurrent, 1, nullptr, 2);
	battles.Update(0, 0, 0, false);
	for (size_t index : { size_t{ 2 }, size_t{ 3 } })
	{
		WorldObject& enemy = battles.GetObjects()[index];
		enemy.State = ActorState::Walking;
		enemy.Facing = index == 2 ? 2 : 6;
		enemy.TargetX = enemy.X + (index == 2 ? 40 : -40);
		enemy.TargetY = enemy.Y;
		battles.GetObjects().Get(index).Update(1);
	}
	Require(battles.GetObjects().Combats[0].Enemy == 2 && battles.GetObjects().Combats[1].Enemy == 3, "Concurrent catches overwrote another player's combat.");
	battles.GetObjects().Get(2).Update(20);
	battles.GetObjects().Get(3).Update(20);
	Require(battles.TakeLifeHits() == 2 && battles.GetObjects().Combats[0].Enemy == -1 && battles.GetObjects().Combats[1].Enemy == -1, "Concurrent fights did not independently finish and report shared life hits.");
	Log("PASS: all four enemies notice/catch player two, retain mid-step targets, scope combat/effects, preserve facing-cell priority and finish simultaneous fights.");
}

void CheckBonusControllers(MudGE& engine, const fpath& saves, bool capture)
{
	const GameData& data = engine.GetData();
	LevelPackInfo pack = *std::find_if(data.GetPacks().begin(), data.GetPacks().end(), [](const LevelPackInfo& candidate) { return candidate.Name == "XS" && candidate.Edition == GameEdition::Zwo; });
	pack.Edition = GameEdition::XXX;
	const i32 directions[4][2] = { { 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 } };
	for (BonusMode mode : { BonusMode::Farmer, BonusMode::Girl, BonusMode::Alien })
	{
		const str family = mode == BonusMode::Farmer ? "Guy" : (mode == BonusMode::Girl ? "Broemse" : "Alien");
		const u32 flags = u32{ 1 } << static_cast<u32>(mode);
		for (i32 facing = 0; facing < 4; ++facing)
		{
			const str name = family + "-" + std::to_string(facing);
			const i32 dx = directions[facing][0], dy = directions[facing][1];
			const LevelInfo level = FixtureLevel(engine, "bonus-action-" + name, { { 0, 280 - dx * 40, 240 - dy * 40, 2 * facing + 1 }, { 2, 280, 240, facing, 8 } }, 0, GameEdition::XXX, flags);
			LevelScene scene(engine, pack, level, 1);
			const GameData::SpriteDefinition& stand = data.GetSpriteDefinition(family + "Stand", GameEdition::XXX);
			Require(scene.GetBonusMode() == mode && scene.GetPlayerDraw().Sprite == engine.LoadSprite(stand.File, stand.Rows, stand.Columns), "Bonus spawn did not draw its controlled character immediately.");
			i32 score = 0;
			for (i32 cycle = 0; cycle < 2; ++cycle)
			{
				scene.Update(0, dx, dy, false);
				scene.Update(0.001f, dx, dy, true);
				Require(scene.GetObjects()[1].InteractionPlayer == 0, "Bonus controller failed to claim the neighboring sheep.");
				if (capture && facing == 0 && cycle == 0)
					engine.DrawFrame(scene.Draws(), ("bonus-action-" + family + ".bmp").c_str());
				f32 elapsed = 0.001f;
				for (i32 tick = 0; tick < 500 && scene.GetObjects()[1].InteractionPlayer >= 0; ++tick)
				{
					elapsed += 0.01f;
					scene.Update(0.01f, 0, 0, false);
					for (const SpriteDraw& draw : scene.Draws())
						Require(draw.Sprite && draw.Frame >= 0 && draw.Frame < draw.Sprite->Rows * draw.Sprite->Columns, "Bonus action emitted an invalid sprite frame.");
				}
				const str descriptor = data.StringParam("a" + (mode == BonusMode::Farmer ? str("Lars") : family) + "Shave" + (facing == 0 || facing == 3 ? "Back" : "Front") + "Bonus", "", GameEdition::XXX);
				const f32 duration = static_cast<f32>(std::count(descriptor.begin(), descriptor.end(), ',') + 1) / (mode == BonusMode::Alien ? 20.0f : 15.0f);
				Require(elapsed >= duration && elapsed < duration + 0.04f, "Bonus action completed at an incorrect native frame rate or phase.");
				score += scene.TakeScoreDelta();
				if (cycle == 0)
					Require(scene.GetObjects()[1].Impatience == 10 && !scene.GetObjects()[1].Freed && score == 2, "Bonus cycle did not raise mood by two with the native two-point reward.");
			}
			Require(scene.Completed() && scene.GetObjects()[1].Freed && score == 14 && scene.GetPlayerCompletions()[0] == 1 && !scene.GetBonusUnlocked(), "Bonus sheep completion/reward/contribution/unlock bookkeeping was incorrect.");
			const Sprite* flash = engine.LoadSprite(data.EditionAsset(GameEdition::XXX, "Sven2/gfx/anim_clouds_with_flash1.bmp"), 16, 1);
			const std::vector<size_t> clouds = scene.GetObjects().Find(ObjectKind::CompletionEffect);
			Require(!clouds.empty() && scene.GetObjects()[clouds.back()].Draw.Sprite == flash, "Bonus completion used a heart cloud instead of lightning.");
			const std::vector<SoundRequest> sounds = scene.TakeAudioEvents();
			Require(std::any_of(sounds.begin(), sounds.end(), [&](const SoundRequest& sound) { return sound.Asset == data.SoundPath("SfxExplodeFlash", GameEdition::XXX); }), "Bonus lightning completion omitted its native sound.");
			if (capture && facing == 0)
				engine.DrawFrame(scene.Draws(), ("bonus-lightning-" + family + ".bmp").c_str());
		}
		const LevelInfo moving = FixtureLevel(engine, "bonus-motion-" + family, { { 0, 240, 240, 2 }, { 2, 640, 400, 0 } }, 0, GameEdition::XXX, flags);
		LevelScene motion(engine, pack, moving, 1);
		motion.Update(0.1f, 1, 0, false);
		const f32 speed = static_cast<f32>(data.NumericParam(family + "Speed", 0, GameEdition::XXX) * data.NumericParam(family + "RunMultiplier", 1, GameEdition::XXX)) * (mode == BonusMode::Farmer ? 1.2f : 1);
		Require(std::abs(motion.GetPlayer().GetX() - 240 - speed * 0.1f) < 0.01f, "Bonus movement used Sven's speed instead of its native character parameters.");
		if (mode != BonusMode::Alien)
		{
			const ItemCode code = mode == BonusMode::Farmer ? ItemCode::Beer : ItemCode::Teddy;
			const LevelInfo distraction = FixtureLevel(engine, "bonus-distraction-" + family, { { 0, 240, 240, 2 }, { 2, 640, 400, 0 } }, static_cast<i32>(code), GameEdition::XXX, flags);
			LevelScene distracted(engine, pack, distraction, 1);
			distracted.Update(0.01f, 0, 0, false);
			Require(distracted.TakeTimeDelta() == -5 && !distracted.UfoActive(), "Bonus distraction omitted its five-second cost or started a UFO.");
			const f32 x = distracted.GetPlayer().GetX();
			distracted.Update(0.1f, 1, 0, false);
			Require(distracted.GetPlayer().GetX() == x, "Bonus distraction allowed movement during its animation.");
			distracted.Update(10, 0, 0, false);
			distracted.Update(0.1f, 1, 0, false);
			Require(distracted.GetPlayer().GetX() > x, "Bonus distraction never released control.");
		}
		const LevelInfo artifact = FixtureLevel(engine, "bonus-artifact-" + family, { { 0, 240, 240, 2 }, { 2, 640, 400, 0 } }, static_cast<i32>(ItemCode::Artifact), GameEdition::XXX, flags);
		LevelScene exit(engine, pack, artifact, 1);
		exit.Update(0.01f, 0, 0, false);
		Require(exit.Completed() && !exit.UfoActive() && !exit.GetObjects()[1].Freed, "Bonus artifact ran the ordinary UFO collection instead of ending the round.");
		if (capture)
			engine.DrawFrame(exit.Draws(), ("bonus-artifact-" + family + ".bmp").c_str());
		LevelScene timeout(engine, pack, moving, 1);
		timeout.TimeExpired();
		timeout.Update(0.1f, 0, 0, false);
		Require(timeout.Completed() && !timeout.UfoActive(), "Bonus timeout waited for ordinary UFO cleanup.");
		GameUi ui(engine, saves);
		ui.ShowBonus(mode);
		const std::vector<SpriteDraw>& intro = ui.Draws(0.1f);
		const GameData::SpriteDefinition& portrait = data.GetSpriteDefinition(family + "Stand", GameEdition::XXX);
		const Sprite* portraitSprite = engine.LoadSprite(portrait.File, portrait.Rows, portrait.Columns);
		const auto portraitDraw = std::find_if(intro.begin(), intro.end(), [&](const SpriteDraw& draw) { return draw.Sprite == portraitSprite; });
		Require(portraitDraw != intro.end() && std::any_of(portraitDraw + 1, intro.end(), [](const SpriteDraw& draw) { return draw.Y >= 260 && draw.Y < 280; }), "Bonus instruction text was painted before its overlapping portrait.");
		if (capture)
			engine.DrawFrame(intro, ("bonus-intro-" + family + ".bmp").c_str());
		WindowInput::Result confirm;
		confirm.Confirm = true;
		Require(ui.GetPage() == GameUi::Page::Bonus && ui.Handle(confirm) == UiAction::Resume, "Bonus instruction screen did not acknowledge correctly.");
	}
	const LevelInfo normal = FixtureLevel(engine, "bonus-campaign-normal", { { 0, 240, 240, 2 }, { 2, 280, 240, 1, 0 } }, 0, GameEdition::XXX);
	const LevelInfo bonus = FixtureLevel(engine, "bonus-campaign-bonus", { { 0, 240, 240, 2 }, { 0, 480, 240, 6 }, { 2, 640, 400, 0 } }, static_cast<i32>(ItemCode::Artifact), GameEdition::XXX, 2);
	LevelSetInfo set = pack.LevelSets.front();
	set.Levels = { normal, bonus, normal };
	GameSession locked(engine, pack, set);
	locked.DebugNextLevel();
	Require(locked.LevelNumber() == 3 && locked.GetBonusMode() == BonusMode::Sven, "Locked bonus map was not skipped.");
	GameSession unlocked(engine, pack, set);
	for (i32 tick = 0; tick < 5000 && !unlocked.GetResults(); ++tick)
		unlocked.Update(0.01f, 1, 0, tick % 2 != 0);
	Require(unlocked.GetResults() && unlocked.GetResult().Sheep == 1, "First-attempt Frigida campaign fixture never completed.");
	unlocked.SkipBonuses();
	unlocked.Advance();
	Require(unlocked.LevelNumber() == 2 && unlocked.GetBonusMode() == BonusMode::Farmer, "Uninterrupted Frigida failed to unlock the following bonus.");
	for (i32 tick = 0; tick < 200 && !unlocked.GetResults(); ++tick)
		unlocked.Update(0.01f, 0, 0, false);
	Require(unlocked.GetResults() && unlocked.GetResult().Lives == 0 && unlocked.GetResult().TimeRemaining == 0 && unlocked.GetLives() == 3 && !unlocked.UfoActive(), "Bonus artifact result timing or time/life-bonus policy was incorrect.");
	unlocked.SkipBonuses();
	unlocked.Advance();
	Require(unlocked.LevelNumber() == 3 && unlocked.GetBonusMode() == BonusMode::Sven, "Bonus completion did not return to normal play.");
	set.Levels = { bonus };
	GameSession multiplayer(engine, pack, set, 2);
	Require(multiplayer.GetPlayerCount() == 2 && multiplayer.GetBonusMode() == BonusMode::Alien, "Two-player bonus did not force both players into alien mode.");
	for (i32 tick = 0; tick < 200 && !multiplayer.GetResults(); ++tick)
		multiplayer.Update(0.01f, std::array<PlayerInput, 2>{});
	Require(multiplayer.GetResults() && multiplayer.GetResult().PlayerCount == 2 && multiplayer.GetResult().TimeRemaining == 0, "Two-player artifact exit did not produce multiplayer results without an unearned time bonus.");
	if (capture)
	{
		GameUi ui(engine, saves);
		ui.ShowResults(multiplayer.GetResult());
		engine.DrawFrame(ui.Draws(0.1f), "multiplayer-bonus-results.bmp");
	}
	Log("PASS: all bonus roles/facings, native cycle/movement/distraction/artifact/timeout rules, intro, unlock/skip/return campaign flow and two-player alien mode.");
}

void CheckMultiplayerPlay(MudGE& engine, const fpath& saves, bool capture)
{
	const GameData& data = engine.GetData();
	LevelPackInfo pack = *std::find_if(data.GetPacks().begin(), data.GetPacks().end(), [](const LevelPackInfo& candidate) { return candidate.Name == "XS" && candidate.Edition == GameEdition::Zwo; });
	pack.Edition = GameEdition::XXX;
	const LevelInfo collision = FixtureLevel(engine, "multiplayer-shared-cell", { { 0, 240, 240, 2 }, { 0, 320, 240, 6 }, { 2, 640, 400, 0 } }, 0, GameEdition::XXX);
	LevelScene bodies(engine, pack, collision, 1, nullptr, 2);
	std::array<PlayerInput, 2> inputs = { PlayerInput{ 1, 0, false }, PlayerInput{ -1, 0, false } };
	bodies.Update(0.4f, inputs);
	Require(std::abs(bodies.GetObjects().GetPlayer(0).GetX() - 280) < 0.01f && bodies.GetObjects().GetPlayer(1).GetX() == 320, "Two players overlapped or overwrote the shared destination reservation.");
	const Sprite* icon1 = engine.LoadSprite(data.GetSpriteDefinition("IconPlayer1", GameEdition::XXX).File, 13, 1);
	const Sprite* icon2 = engine.LoadSprite(data.GetSpriteDefinition("IconPlayer2", GameEdition::XXX).File, 13, 1);
	const std::vector<SpriteDraw>& draws = bodies.Draws();
	Require(std::any_of(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == icon1 && draw.X == 265 && draw.Frame < 12; }) && std::any_of(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == icon2 && draw.X == 305 && draw.Frame < 12; }), "Multiplayer identifiers were missing or attached to the wrong player.");
	if (capture)
		engine.DrawFrame(draws, "multiplayer-identifiers.bmp");
	for (ItemCode code : { ItemCode::Rocket, ItemCode::SlowMushroom, ItemCode::StinkMushroom, ItemCode::Flower, ItemCode::AlienPower })
	{
		bodies.GetItems().GetPowerup() = { code, 10, 0 };
		const std::vector<SpriteDraw>& powered = bodies.Draws();
		Require(std::none_of(powered.begin(), powered.end(), [&](const SpriteDraw& draw) { return draw.Sprite == icon1; }) && std::any_of(powered.begin(), powered.end(), [&](const SpriteDraw& draw) { return draw.Sprite == icon2; }), "Equipped effect did not replace only its owner's player marker.");
	}
	if (capture)
		engine.DrawFrame(bodies.Draws(), "multiplayer-powered-marker.bmp");
	bodies.GetItems().GetPowerup() = {};
	const std::vector<SpriteDraw>& restored = bodies.Draws();
	Require(std::any_of(restored.begin(), restored.end(), [&](const SpriteDraw& draw) { return draw.Sprite == icon1; }), "Player marker did not return after the equipped effect ended.");
	const LevelInfo demon = FixtureLevel(engine, "multiplayer-demon-target", { { 0, 80, 80, 0 }, { 0, 280, 200, 4 }, { 2, 280, 240, 0, 10, 0, static_cast<i32>(ActorState::DemonTransforming) } }, 0, GameEdition::XXX);
	LevelScene shocked(engine, pack, demon, 1, nullptr, 2);
	shocked.GetItems().GetPowerup() = { ItemCode::Flower, 10, 0 };
	{
		LevelScene::PlayerScope owner(shocked, 1);
		shocked.GetItems().GetPowerup() = { ItemCode::Rocket, 10, 0 };
	}
	for (i32 tick = 0; tick < 500 && shocked.GetObjects()[2].State != ActorState::DemonShocking; ++tick)
		shocked.Update(0.01f, 0, 0, false);
	Require(shocked.GetObjects()[2].State == ActorState::DemonShocking && shocked.GetObjects()[2].TargetPlayer == 1 && shocked.GetItems().GetPowerup().Code == ItemCode::Flower, "Demon sheep shocked or cleared the effect of the wrong player.");
	{
		LevelScene::PlayerScope owner(shocked, 1);
		Require(shocked.GetItems().GetPowerup().Code == ItemCode::None, "Caught player two retained its effect during demon shock.");
	}
	const LevelInfo teleports = FixtureLevel(engine, "multiplayer-splash-owners", { { 0, 240, 240, 2 }, { 0, 480, 240, 6 }, { 2, 640, 400, 0 } }, 0, GameEdition::XXX);
	LevelScene splashes(engine, pack, teleports, 1, nullptr, 2);
	const i32 first = splashes.GetObjects()[0].Tile + 1, second = splashes.GetObjects()[1].Tile - 1;
	splashes.GetPlayer().Teleport(first);
	splashes.Update(0.1f, 0, 0, false);
	{
		LevelScene::PlayerScope owner(splashes, 1);
		splashes.GetPlayer().Teleport(second);
	}
	const Sprite* splashSprite = engine.LoadSprite(data.EditionAsset(GameEdition::XXX, "Sven2/gfx/FX_all_splash.bmp"), 14, 1);
	const std::vector<SpriteDraw>& splashDraws = splashes.Draws();
	Require(std::any_of(splashDraws.begin(), splashDraws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == splashSprite && draw.X == first % 22 * 40 - 70 && draw.Frame > 0; }) && std::any_of(splashDraws.begin(), splashDraws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == splashSprite && draw.X == second % 22 * 40 - 70 && draw.Frame == 0; }), "Second teleport replaced or restarted the other player's splash.");
	const LevelInfo complete = FixtureLevel(engine, "multiplayer-campaign-completion", { { 0, 240, 240, 2 }, { 0, 480, 240, 6 }, { 12, 280, 240, 1, 0 }, { 12, 440, 240, 2, 0 } }, 0, GameEdition::XXX);
	LevelSetInfo set = pack.LevelSets.front();
	set.Levels = { complete };
	GameSession session(engine, pack, set, 2);
	session.Update(0.001f, inputs);
	inputs[0].Action = inputs[1].Action = true;
	session.Update(0.001f, inputs);
	for (i32 tick = 0; tick < 1000 && !session.GetResults(); ++tick)
		session.Update(0.01f, std::array<PlayerInput, 2>{});
	RoundResult result = session.GetResult();
	Require(session.GetResults() && result.PlayerCount == 2 && result.Sheep == 2 && result.PlayerSheep[0] == 1 && result.PlayerSheep[1] == 1, "Multiplayer campaign lost a player's animal completion/contribution.");
	const i32 score = session.GetScore();
	const i32 bonus = result.TimeRemaining * 2 + result.SheepRemaining * 10 + result.LivesRemaining * 10;
	if (capture)
	{
		GameUi ui(engine, saves);
		ui.ShowResults(result);
		engine.DrawFrame(ui.Draws(0.1f), "multiplayer-level-results.bmp");
	}
	session.SkipBonuses();
	Require(session.Ready() && session.GetScore() == score + bonus, "XXX result conversion disagreed with the shared time/animal/life multipliers.");
	session.Advance();
	Require(session.Finished(), "Multiplayer final-level acknowledgement did not finish the episode.");
	GameUi ui(engine, saves / L"multiplayer-score");
	result = session.GetResult();
	ui.ShowResults(result);
	if (capture)
		engine.DrawFrame(ui.Draws(0.1f), "multiplayer-results.bmp");
	WindowInput::Result confirm;
	confirm.Confirm = true;
	ui.Handle(confirm);
	Require(ui.GetPage() == GameUi::Page::Name, "Multiplayer results did not reach name entry.");
	WindowInput::Result name;
	name.Text = "Two players";
	ui.Handle(name);
	ui.Handle(confirm);
	const str bank = (ui.GetPack().Id.empty() ? ui.GetPack().Name : ui.GetPack().Id) + "-0-2player.tsv";
	Require(std::filesystem::exists(saves / L"multiplayer-score" / std::filesystem::u8path(bank)), "Multiplayer highscore overwrote the single-player bank.");
	const LevelInfo attacks = FixtureLevel(engine, "multiplayer-shared-life-insurance", { { 0, 240, 240, 0 }, { 0, 480, 240, 0 }, { 4, 160, 240, 2 }, { 4, 560, 240, 6 }, { 2, 640, 400, 0 } }, 0, GameEdition::XXX);
	set.Levels = { attacks };
	GameSession insurance(engine, pack, set, 2);
	for (i32 tick = 0; tick < 2500 && insurance.GetLives() == 3; ++tick)
		insurance.Update(0.01f, std::array<PlayerInput, 2>{});
	Require(insurance.GetLives() == 2 && insurance.GetScore() == 0, "Concurrent attacks did not use one shared native initial-refund/life-insurance state.");
	LevelPackInfo zwo = pack;
	zwo.Edition = GameEdition::Zwo;
	const LevelInfo original = FixtureLevel(engine, "multiplayer-zwo-spawn-policy", { { 0, 240, 240, 2 }, { 2, 640, 400, 0 } });
	set.Levels = { original };
	GameSession coop(engine, zwo, set, 2);
	GameSession single(engine, zwo, set);
	Require(coop.GetPlayerCount() == 2 && single.GetPlayerCount() == 1, "Zwo co-op did not preserve the independently selected single-player mode.");
	Log("PASS: two-player body reservations, independent teleport effects/identifiers, joint completion/results/highscore bank and optional Zwo co-op.");
}

void CheckZwoCoopSpawns(MudGE& engine, const fpath& saves, bool capture)
{
	const GameData& data = engine.GetData();
	const LevelPackInfo& pack = *std::find_if(data.GetPacks().begin(), data.GetPacks().end(), [](const LevelPackInfo& candidate) { return candidate.Name == "XS" && candidate.Edition == GameEdition::Zwo; });
	auto write = [&](const LevelInfo& level, const bytestream& bytes) {
		std::ofstream output(data.GetRoot() / std::filesystem::u8path(level.Map), std::ios::binary | std::ios::trunc);
		output.write(reinterpret_cast<cstr>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		Require(static_cast<bool>(output), "Cannot write isolated co-op fixture.");
	};
	auto integer = [](bytestream& bytes, size_t offset, i32 value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); };
	const LevelInfo fallback = FixtureLevel(engine, "zwo-coop-crowded-start", { { 0, 240, 240, 0 }, { 2, 640, 400, 0 } });
	bytestream bytes = engine.ReadAsset(fallback.Map);
	const i32 origin = (240 + 60) / TileSize + 1 + ((240 + 60) / TileSize + 1) * Stride;
	for (const i32* direction : Directions)
		bytes[16 + (origin + direction[0] + direction[1] * Stride) * 24 + 12] = static_cast<u8>(ItemCode::Gift);
	write(fallback, bytes);
	LevelScene crowded(engine, pack, fallback, 1, nullptr, 2);
	Require(crowded.GetObjects().GetPlayer(1).GetState().Tile == origin - 2 * Stride && crowded.GetObjects().GetPlayer(1).GetIndex() == 2, "Crowded spawn did not find the nearest reachable empty ring.");
	LevelScene repeat(engine, pack, fallback, 79, nullptr, 2);
	Require(repeat.GetObjects().GetPlayer(1).GetState().Tile == crowded.GetObjects().GetPlayer(1).GetState().Tile, "Generated spawn depends on gameplay randomness.");
	const LevelInfo directed = FixtureLevel(engine, "zwo-coop-directional-start", { { 0, 240, 240, 0 }, { 2, 640, 400, 0 } });
	bytes = engine.ReadAsset(directed.Map);
	bytes[16 + origin * 24 + 4] = TileDirection[static_cast<i32>(PlayerMotion::East)];
	integer(bytes, 16 + (origin + 1) * 24, static_cast<i32>(OccupancyCode::Wall));
	integer(bytes, 16 + (origin - 1) * 24, static_cast<i32>(OccupancyCode::Wall));
	bytes[16 + (origin + 1) * 24 + 4] = TileDirection[static_cast<i32>(PlayerMotion::East)];
	write(directed, bytes);
	LevelScene oneWay(engine, pack, directed, 1, nullptr, 2);
	Require(oneWay.GetObjects().GetPlayer(1).GetState().Tile == origin + 2, "Spawn search ignored direction filtering or placed a player on a directional wall.");
	const LevelInfo spawner = FixtureLevel(engine, "zwo-coop-spawner-start", { { 0, 240, 240, 0 }, { 2, 640, 400, 0 } });
	bytes = engine.ReadAsset(spawner.Map);
	integer(bytes, 16 + (origin - Stride) * 24 + 20, static_cast<i32>(SpawnerCode::Mixed));
	write(spawner, bytes);
	LevelScene spawning(engine, pack, spawner, 1, nullptr, 2);
	Require(spawning.GetObjects().GetPlayer(1).GetState().Tile != origin - Stride, "Player two spawned on an item spawner.");
	const LevelInfo pending = FixtureLevel(engine, "zwo-coop-unadopted-player", { { 1, 640, 400, 0 }, { 0, 0, 40, 0 }, { 2, 560, 400, 0 } });
	LevelScene adopted(engine, pack, pending, 1, nullptr, 2);
	Require(adopted.GetObjects().GetPlayer(0).GetIndex() == 1 && adopted.GetObjects().GetPlayer(1).GetX() == 0 && adopted.GetObjects().GetPlayer(1).GetY() == 0, "Pending player state falsely occupied the origin or replaced an authored object.");
	const LevelInfo blocked = FixtureLevel(engine, "zwo-coop-no-space", { { 0, 240, 240, 0 }, { 2, 640, 400, 0 } });
	bytes = engine.ReadAsset(blocked.Map);
	for (size_t tile = 0; tile < LevelFile::CellCount; ++tile)
		if (static_cast<i32>(tile) != origin)
			integer(bytes, 16 + tile * 24, static_cast<i32>(OccupancyCode::Wall));
	write(blocked, bytes);
	bool rejected = false;
	try
	{
		LevelScene invalid(engine, pack, blocked, 1, nullptr, 2);
	}
	catch (const std::runtime_error& error)
	{
		rejected = str(error.what()).find("no reachable free spawn cell") != str::npos;
	}
	Require(rejected && engine.ReadAsset(blocked.Map) == bytes, "Impossible co-op spawn overlapped an actor or modified the source map.");
	LevelScene playableSingle(engine, pack, blocked, 1);
	Require(playableSingle.GetPlayerCount() == 1, "Co-op spawn rejection broke single-player loading.");
	const LevelInfo full = FixtureLevel(engine, "zwo-coop-full-world", { { 0, 240, 240, 0 }, { 1, 640, 400, 0 }, { 2, 560, 400, 0 } });
	bytes = engine.ReadAsset(full.Map);
	for (size_t slot = 3; slot < LevelFile::ObjectCount; ++slot)
		std::memcpy(bytes.data() + 16 + 9048 + slot * 100, bytes.data() + 16 + 9048 + 100, 100);
	write(full, bytes);
	rejected = false;
	try
	{
		LevelScene invalid(engine, pack, full, 1, nullptr, 2);
	}
	catch (const std::runtime_error& error)
	{
		rejected = str(error.what()).find("No free world object slot") != str::npos;
	}
	Require(rejected, "A full authored world was overwritten to fit player two.");
	const LevelInfo complete = FixtureLevel(engine, "zwo-coop-joint-completion", { { 0, 240, 240, 2 }, { 12, 280, 240, 1, 0 }, { 12, 280, 200, 1, 0 } });
	LevelSetInfo set = pack.LevelSets.front();
	set.Levels = { complete, fallback };
	GameSession session(engine, pack, set, 2);
	std::array<PlayerInput, 2> input = { PlayerInput{ 1, 0, false }, PlayerInput{ 1, 0, false } };
	session.Update(0.001f, input);
	input[0].Action = input[1].Action = true;
	session.Update(0.001f, input);
	for (i32 tick = 0; tick < 1000 && !session.GetResults(); ++tick)
		session.Update(0.01f, std::array<PlayerInput, 2>{});
	Require(session.GetResults() && session.GetResult().PlayerCount == 2 && session.GetResult().PlayerSheep == std::array<i32, 2>{ 1, 1 }, "Generated player did not participate in the shared Zwo campaign results.");
	if (capture)
	{
		GameUi ui(engine, saves);
		ui.ShowResults(session.GetResult());
		engine.DrawFrame(ui.Draws(0.1f), "zwo-coop-results.bmp");
	}
	session.SkipBonuses();
	session.Advance();
	Require(session.LevelNumber() == 2 && session.GetPlayerCount() == 2 && !session.GetResults(), "Campaign advance failed to regenerate player two.");
	session.DebugRestartLevel();
	Require(session.GetPlayerCount() == 2 && session.LevelNumber() == 2, "Restart dropped Zwo co-op mode.");
	const LevelInfo combat = FixtureLevel(engine, "zwo-coop-generated-catch", { { 0, 240, 240, 0 }, { 4, 240, 120, 4 }, { 2, 640, 400, 0 } });
	LevelScene caught(engine, pack, combat, 1, nullptr, 2);
	const size_t generated = caught.GetObjects().GetPlayer(1).GetIndex();
	caught.GetItems().SpawnItem(ObjectKind::Flower, caught.GetObjects()[generated].Tile);
	caught.Update(0.001f, 0, 0, false);
	{
		LevelScene::PlayerScope owner(caught, 1);
		Require(caught.GetItems().GetPowerup().Code == ItemCode::Flower, "Generated player failed to collect its own item.");
	}
	for (i32 tick = 0; tick < 500 && caught.GetObjects().Combats[1].Enemy < 0; ++tick)
		caught.Update(0.01f, 0, 0, false);
	Require(caught.GetObjects().Combats[1].Enemy == 1 && caught.GetObjects().Combats[0].Enemy == -1 && caught.GetObjects()[1].TargetPlayer == 1, "Enemy catch confused a generated world slot with the player number.");
	const LevelInfo artifact = FixtureLevel(engine, "zwo-coop-generated-artifact", { { 0, 240, 240, 0 }, { 2, 640, 400, 0 } });
	LevelScene beamed(engine, pack, artifact, 1, nullptr, 2);
	beamed.GetItems().SpawnItem(ObjectKind::Artifact, beamed.GetObjects().GetPlayer(1).GetState().Tile);
	bool returned = false;
	for (i32 tick = 0; tick < 3000 && (!returned || beamed.UfoActive()); ++tick)
	{
		beamed.Update(0.01f, 0, 0, false);
		LevelScene::PlayerScope owner(beamed, 1);
		returned = returned || beamed.GetItems().GetPowerup().Code == ItemCode::AlienPower;
	}
	Require(returned && !beamed.UfoActive() && beamed.GetItems().GetPowerup().Code == ItemCode::None, "Generated player artifact return changed player one's state.");
	const LevelInfo& tutorial = pack.LevelSets.front().Levels.front();
	const LevelFile tutorialFile(engine.ReadAsset(tutorial.Map), tutorial.Map);
	Require(!tutorialFile.GetPuddles().empty(), "Tutorial fixture has no authored teleport destinations.");
	LevelScene teleported(engine, pack, tutorial, 1, nullptr, 2);
	const f32 firstX = teleported.GetPlayer().GetX(), firstY = teleported.GetPlayer().GetY();
	const i32 oldTile = teleported.GetObjects().GetPlayer(1).GetState().Tile;
	{
		LevelScene::PlayerScope owner(teleported, 1);
		teleported.GetPlayer().Teleport(tutorialFile.GetPuddles().front());
		const i32 landing = teleported.GetPlayer().GetState().Tile;
		Require(landing != oldTile && teleported.GetGrid().GetReservationOwner(landing) == static_cast<i32>(teleported.GetPlayerIndex()) && teleported.GetGrid().GetReservationOwner(oldTile) != static_cast<i32>(teleported.GetPlayerIndex()), "Generated player teleport did not move its own collision ownership.");
	}
	Require(teleported.GetPlayer().GetX() == firstX && teleported.GetPlayer().GetY() == firstY, "Generated player's teleport moved the authored player.");
	Log("PASS: Zwo co-op nearest/directional/spawner/pending-player/no-space cases; generated-player interactions, pickup, catch, artifact, results, advance and restart.");
}

void CheckZwoCoopCatalog(MudGE& engine, bool capture)
{
	size_t levels = 0;
	for (const LevelPackInfo& pack : engine.GetData().GetPacks())
	{
		if (pack.Edition != GameEdition::Zwo)
			continue;
		for (const LevelSetInfo& set : pack.LevelSets)
		{
			for (const LevelInfo& level : set.Levels)
			{
				try
				{
					const bytestream bytes = engine.ReadAsset(level.Map);
					const LevelFile source(bytes, level.Map);
					LevelScene single(engine, pack, level, 1);
					LevelScene coop(engine, pack, level, 1, nullptr, 2);
					Require(single.GetPlayerCount() == 1 && coop.GetPlayerCount() == 2 && coop.GetObjects().Size() == single.GetObjects().Size() + 1, "Zwo co-op changed authored objects instead of adding one player.");
					const PlayerCharacter& first = coop.GetObjects().GetPlayer(0);
					const PlayerCharacter& second = coop.GetObjects().GetPlayer(1);
					Require(first.GetIndex() == single.GetPlayerIndex() && first.GetX() == single.GetPlayer().GetX() && first.GetY() == single.GetPlayer().GetY(), "Generated player moved the authored spawn.");
					const i32 tile = second.GetState().Tile;
					const LevelCell& cell = source.GetCells()[tile];
					Require(tile != first.GetState().Tile && cell.Terrain == OccupancyCode::Empty && cell.Direction == 0 && cell.ItemCode == ItemCode::None && !IsSpawner(cell.Spawner), "Generated player occupies an unsafe cell.");
					Require(coop.GetGrid().GetReservationOwner(tile) == static_cast<i32>(second.GetIndex()) && coop.GetGrid().GetReservationOwner(first.GetState().Tile) == static_cast<i32>(first.GetIndex()), "Co-op spawn did not establish separate collision ownership.");
					Require(!coop.GetObjects().CanOccupy(tile, first.GetIndex()), "Player one's movement can overlap the generated player.");
					for (size_t index = 0; index < single.GetObjects().Size(); ++index)
						Require(coop.GetObjects()[index].Original == single.GetObjects()[index].Original && coop.GetObjects()[index].Temperament == single.GetObjects()[index].Temperament, "Co-op spawn changed authored slots or consumed gameplay randomness.");
					if (capture && levels == 0)
						engine.DrawFrame(coop.Draws(), "zwo-coop-generated-spawn.bmp");
					for (i32 tick = 0; tick < 1800; ++tick)
					{
						if (tick == 1200)
							coop.TimeExpired();
						coop.Update(1.0f / 60, std::array<PlayerInput, 2>{});
					}
					for (const SpriteDraw& draw : coop.Draws())
						Require(draw.Sprite && draw.Frame >= 0 && draw.Frame < draw.Sprite->Rows * draw.Sprite->Columns, "Zwo co-op emitted an invalid frame.");
					for (const str& sound : coop.TakeSounds())
						engine.GetData().AssetPath(sound);
					Require(engine.ReadAsset(level.Map) == bytes, "Co-op load altered the source level.");
					++levels;
				}
				catch (const std::exception& error)
				{
					throw std::runtime_error(level.Map + ": " + error.what());
				}
			}
		}
	}
	Require(levels == 105, "Zwo co-op catalog coverage is incomplete.");
	Log("PASS: all 105 Zwo maps generate safe co-op spawns, preserve single-player/authored data/randomness and simulate 30 seconds.");
}

void CheckStreaksAndMermaids(MudGE& engine, bool capture)
{
	const std::vector<LevelPackInfo>& packs = engine.GetData().GetPacks();
	const LevelPackInfo& zwo = *std::find_if(packs.begin(), packs.end(), [](const LevelPackInfo& pack) { return pack.Edition == GameEdition::Zwo && pack.Name == "XS"; });
	LevelPackInfo xxx = zwo;
	xxx.Edition = GameEdition::XXX; // Fixtures reuse Zwo scenery while exercising the v7 reader.
	const Sprite* action = engine.LoadSprite(engine.GetData().GetSpriteDefinition("MermaidShag", GameEdition::XXX).File, engine.GetData().GetSpriteDefinition("MermaidShag", GameEdition::XXX).Rows, 1);
	auto fixture = [&](const str& name, GameEdition edition, i32 facing = 2) {
		return FixtureLevel(engine, name, { { 0, 240 - Directions[facing][0] * 40, 200 - Directions[facing][1] * 40, facing }, { 28, 240, 200, 0 }, { 2, 600, 400, 0 } }, 0, edition);
	};
	auto idle = [](LevelScene& scene, Mermaid& mermaid) {
		Require(mermaid.Reveal(), "Dormant mermaid did not reveal.");
		for (i32 tick = 0; tick < 500 && mermaid.GetMermaidState() != MermaidState::Idle; ++tick)
			scene.Update(0.01f, 0, 0, false);
		Require(mermaid.GetMermaidState() == MermaidState::Idle, "Mermaid emergence never reached idle.");
	};
	for (GameEdition edition : { GameEdition::Zwo, GameEdition::XXX })
	{
		const LevelPackInfo& pack = edition == GameEdition::Zwo ? zwo : xxx;
		for (i32 count : { 1, 2, 3, 4, 5, 6, 7, 16, 17, 32 })
		{
			LevelScene scene(engine, pack, fixture("streak-" + std::to_string(count) + "-" + std::to_string(static_cast<i32>(edition)), edition), 1);
			Mermaid& mermaid = dynamic_cast<Mermaid&>(scene.GetObjects().Get(1));
			for (i32 completion = 0; completion < count; ++completion)
			{
				scene.RecordAnimalCompletion(scene.GetObjects()[2]);
				scene.Update(0.02f, 0, 0, false);
			}
			Require(mermaid.GetMermaidState() == MermaidState::Dormant && scene.TakeScoreDelta() == 0, "Streak fired before its first five-second window expired.");
			scene.Update(5.1f - count * 0.02f, 0, 0, false);
			const i32 award = count < 3 || count > 16 ? 0 : (count <= 6 ? 10 * count : 70);
			Require(scene.TakeScoreDelta() == award, "Streak score differs from the native 3..16 switch.");
			Require((mermaid.GetMermaidState() == MermaidState::Dormant) == (count < 3), "Mermaid appearance did not follow the streak threshold.");
			Require(scene.GetRemainingTargets() == 1, "Mermaid was counted as a required animal.");
			if (count >= 3)
			{
				Require(scene.GetObjects().Find(ObjectKind::HeartParticle).size() == static_cast<size_t>(std::min(5 * count - 5, 128)), "Streak heart count differs from native behavior.");
				const std::vector<str> sounds = scene.TakeSounds();
				Require(std::find(sounds.begin(), sounds.end(), engine.GetData().SoundPath("SfxMultipleShag", GameEdition::XXX)) != sounds.end(), "Streak reward sound is missing.");
				const std::vector<SpriteDraw>& draws = scene.Draws();
				const Sprite* hearts = scene.GetObjects()[scene.GetObjects().Find(ObjectKind::HeartParticle).front()].Draw.Sprite;
				const Sprite* playerSprite = scene.GetPlayerDraw().Sprite;
				const auto firstHeart = std::find_if(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == hearts; });
				const auto playerDraw = std::find_if(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == playerSprite; });
				Require(firstHeart != draws.end() && playerDraw != draws.end() && firstHeart > playerDraw, "Streak hearts must render after the world, not behind Sven.");
				if (capture && count == 3 && edition == GameEdition::XXX)
					engine.DrawFrame(scene.Draws(), "mermaid-streak.bmp");
			}
			scene.Update(1, 0, 0, false);
			Require(scene.TakeScoreDelta() == 0, "A completed streak paid twice.");
		}
		for (i32 facing = 0; facing < 8; ++facing)
		{
			LevelScene scene(engine, pack, fixture("mermaid-facing-" + std::to_string(facing) + "-" + std::to_string(static_cast<i32>(edition)), edition, facing), 1);
			Mermaid& mermaid = dynamic_cast<Mermaid&>(scene.GetObjects().Get(1));
			idle(scene, mermaid);
			scene.Update(0, Directions[facing][0], Directions[facing][1], false);
			scene.Update(0.001f, 0, 0, true);
			const bool accepted = facing >= 2 && facing <= 4;
			Require((mermaid.GetMermaidState() == MermaidState::Interacting) == accepted, "Mermaid accepted the wrong approach direction.");
			if (!accepted)
				continue;
			Require(scene.GetPlayerDraw().Sprite == action && !scene.GetObjects()[1].Draw.Sprite, "Mermaid compound animation draws duplicate sprites.");
			if (capture && facing == 2 && edition == GameEdition::XXX)
				engine.DrawFrame(scene.Draws(), "mermaid-interaction.bmp");
			// Queue one repeat per animation; the seventh completion resolves her.
			for (i32 cycle = 1; cycle <= 7; ++cycle)
			{
				if (cycle < 7)
				{
					scene.Update(0, 0, 0, false);
					scene.Update(0, 0, 0, true);
				}
				for (i32 tick = 0; tick < 500 && scene.GetObjects()[1].Attempts < cycle && !scene.GetObjects()[1].Freed; ++tick)
					scene.Update(0.01f, 0, 0, false);
				Require(scene.GetObjects()[1].Attempts == cycle, "Mermaid animation/repeat failed to complete.");
				Require(scene.TakeScoreDelta() == (cycle == 7 ? 46 : 16), "Mermaid cycle or resolution score is wrong.");
			}
			Require(scene.GetObjects()[1].Freed && scene.GetRemainingTargets() == 1 && scene.TakeTimeDelta() == 10 && scene.TakeLivesGained() == 0, "Mermaid completion must grant ten seconds, not lives or target completion.");
			Require(scene.GetObjects()[scene.GetPlayerIndex()].Tile == scene.GetObjects()[1].Tile && scene.GetGrid().GetOccupancy(scene.GetObjects()[1].Tile) == OccupancyCode::Player, "Mermaid completion did not transfer Sven's collision reservation.");
		}
	}
	LevelScene interrupted(engine, xxx, fixture("mermaid-interrupted", GameEdition::XXX), 1);
	Mermaid& interruptedMermaid = dynamic_cast<Mermaid&>(interrupted.GetObjects().Get(1));
	idle(interrupted, interruptedMermaid);
	interrupted.Update(0, 1, 0, false);
	interrupted.Update(0.001f, 0, 0, true);
	interrupted.GetPlayer().InterruptInteraction();
	interrupted.GetPlayer().RestorePlayer();
	Require(interruptedMermaid.GetMermaidState() == MermaidState::Idle && interrupted.GetObjects()[1].InteractionPlayer < 0 && interrupted.TakeScoreDelta() == 0, "Interrupted mermaid retained ownership or awarded a completion.");
	interrupted.Update(0, 0, 0, false);
	interrupted.Update(0.001f, 0, 0, true);
	// Force the next native idle cycle to expire while the player is interacting.
	for (i32 tick = 0; tick < 10000 && interruptedMermaid.GetMermaidState() == MermaidState::Interacting; ++tick)
	{
		interrupted.GetObjects().Get(1).Update(0.01f);
	}
	Require(interruptedMermaid.GetMermaidState() == MermaidState::Submerging && interrupted.GetObjects()[1].InteractionPlayer < 0 && interrupted.GetPlayerDraw().Sprite != action, "Mermaid timeout failed to release and restore Sven.");
	Require(interrupted.GetGrid().GetOccupancy(interrupted.GetObjects()[1].Tile) == OccupancyCode::Empty, "Submerging mermaid left collision behind.");
	for (i32 tick = 0; tick < 500 && interruptedMermaid.GetMermaidState() != MermaidState::Dormant; ++tick)
		interrupted.GetObjects().Get(1).Update(0.01f);
	Require(interruptedMermaid.Reveal(), "Timed-out mermaid cannot appear again.");
	LevelScene owners(engine, xxx, FixtureLevel(engine, "mermaid-owners", { { 0, 200, 200, 2 }, { 0, 240, 160, 4 }, { 28, 240, 200, 0 }, { 2, 600, 400, 0 } }, 0, GameEdition::XXX), 1, nullptr, 2);
	Mermaid& ownedMermaid = dynamic_cast<Mermaid&>(owners.GetObjects().Get(2));
	idle(owners, ownedMermaid);
	owners.Update(0, std::array<PlayerInput, 2>{ PlayerInput{ 1, 0, false }, PlayerInput{ 0, 1, false } });
	owners.Update(0.001f, std::array<PlayerInput, 2>{ PlayerInput{ 0, 0, true }, PlayerInput{ 0, 0, true } });
	Require(owners.GetObjects()[2].InteractionPlayer == 0 && owners.GetObjects().GetPlayer(1).GetState().Draw.Sprite != action, "Both players acquired the same mermaid.");
	owners.GetObjects().GetPlayer(0).InterruptInteraction();
	owners.Update(0, std::array<PlayerInput, 2>{ PlayerInput{ 1, 0, false }, PlayerInput{ 0, 1, false } });
	owners.Update(0.001f, std::array<PlayerInput, 2>{ PlayerInput{}, PlayerInput{ 0, 0, true } });
	Require(owners.GetObjects()[2].InteractionPlayer == 1, "Released mermaid cannot be acquired by player two.");
	const f32 otherX = owners.GetObjects().GetPlayer(0).GetX(), otherY = owners.GetObjects().GetPlayer(0).GetY();
	for (i32 cycle = 1; cycle <= 7; ++cycle)
	{
		if (cycle < 7)
		{
			owners.Update(0, std::array<PlayerInput, 2>{});
			owners.Update(0, std::array<PlayerInput, 2>{ PlayerInput{}, PlayerInput{ 0, 0, true } });
		}
		for (i32 tick = 0; tick < 500 && owners.GetObjects()[2].Attempts < cycle && !owners.GetObjects()[2].Freed; ++tick)
			owners.Update(0.01f, std::array<PlayerInput, 2>{});
		Require(owners.GetObjects()[2].Attempts == cycle, "Player two's mermaid repeat stalled.");
	}
	Require(owners.TakeScoreDelta() == 142 && owners.TakeTimeDelta() == 10 && owners.GetObjects().GetPlayer(0).GetX() == otherX && owners.GetObjects().GetPlayer(0).GetY() == otherY, "Player two's mermaid reward moved or credited the wrong player.");
	Require(owners.GetGrid().GetReservationOwner(owners.GetObjects()[2].Tile) == static_cast<i32>(owners.GetObjects().GetPlayer(1).GetIndex()), "Mermaid completion reserved the cell for the wrong player.");
	// Separate player histories must not combine to reveal her.
	LevelScene histories(engine, xxx, FixtureLevel(engine, "mermaid-histories", { { 0, 200, 200, 2 }, { 0, 400, 200, 4 }, { 28, 240, 200, 0 }, { 2, 600, 400, 0 } }, 0, GameEdition::XXX), 1, nullptr, 2);
	for (size_t number : { size_t{ 0 }, size_t{ 1 }, size_t{ 0 }, size_t{ 1 } })
	{
		LevelScene::PlayerScope player(histories, number);
		histories.RecordAnimalCompletion(histories.GetObjects()[3]);
		histories.Update(0.02f, std::array<PlayerInput, 2>{});
	}
	histories.Update(5.1f, std::array<PlayerInput, 2>{});
	Require(histories.TakeScoreDelta() == 0 && dynamic_cast<Mermaid&>(histories.GetObjects().Get(2)).GetMermaidState() == MermaidState::Dormant, "Streak histories were shared between players.");
	LevelScene resolved(engine, xxx, FixtureLevel(engine, "streak-real-completions", { { 0, 200, 200, 2 }, { 2, 240, 200, 0 }, { 11, 320, 200, 0 }, { 25, 400, 200, 0 }, { 2, 600, 400, 0 }, { 28, 240, 360, 0 } }, 0, GameEdition::XXX), 1);
	for (size_t index : { size_t{ 1 }, size_t{ 2 }, size_t{ 3 } })
	{
		dynamic_cast<AnimalCharacter&>(resolved.GetObjects().Get(index)).Resolve();
		resolved.Update(0.02f, 0, 0, false);
	}
	resolved.TakeScoreDelta();
	resolved.Update(5.1f, 0, 0, false);
	Require(resolved.TakeScoreDelta() == 30 && dynamic_cast<Mermaid&>(resolved.GetObjects().Get(5)).GetMermaidState() != MermaidState::Dormant, "Actual sheep/chicken/penguin completion did not register a streak.");
	LevelScene blocked(engine, xxx, FixtureLevel(engine, "mermaid-blocked", { { 0, 240, 200, 2 }, { 28, 240, 200, 0 }, { 2, 600, 400, 0 } }, 0, GameEdition::XXX), 1);
	blocked.Update(0, 0, 0, false);
	Require(!dynamic_cast<Mermaid&>(blocked.GetObjects().Get(1)).Reveal(), "Mermaid revealed through an occupied player cell.");
	// Exact zero is intentionally not expiration: native ticks positive timers only.
	LevelScene boundary(engine, xxx, fixture("streak-zero-boundary", GameEdition::XXX), 1);
	boundary.RecordAnimalCompletion(boundary.GetObjects()[2]);
	for (i32 tick = 0; tick < 160; ++tick)
		boundary.Update(0.015625f, 0, 0, false);
	boundary.RecordAnimalCompletion(boundary.GetObjects()[2]);
	boundary.RecordAnimalCompletion(boundary.GetObjects()[2]);
	for (i32 tick = 0; tick < 160; ++tick)
		boundary.Update(0.015625f, 0, 0, false);
	boundary.Update(0.001f, 0, 0, false);
	Require(boundary.TakeScoreDelta() == 0 && dynamic_cast<Mermaid&>(boundary.GetObjects().Get(1)).GetMermaidState() == MermaidState::Dormant, "Exact-zero streak timer did not preserve the native strict comparisons.");
	Log("PASS: native five-second per-player streaks, score buckets/hearts, mermaid directions, repeats, rewards, collision, interruption, timeout and ownership on v6/v7 maps.");
}

void CheckWeatherAndLevelHelp(MudGE& engine, bool capture)
{
	const std::vector<LevelPackInfo>& packs = engine.GetData().GetPacks();
	const LevelPackInfo& zwo = *std::find_if(packs.begin(), packs.end(), [](const LevelPackInfo& pack) { return pack.Edition == GameEdition::Zwo && pack.Name == "XS"; });
	LevelPackInfo profile = zwo;
	profile.Edition = GameEdition::XXX;
	for (u32 flags = 0; flags < 256; ++flags)
	{
		const LevelInfo level = FixtureLevel(engine, "weather-flags", { { 0, 200, 200, 2 } }, 0, GameEdition::XXX, flags);
		const LevelFile file(engine.ReadAsset(level.Map), level.Map);
		const WeatherMode expected = flags & 128 ? WeatherMode::Bubbles : (flags & 64 ? WeatherMode::Rain : (flags & 32 ? WeatherMode::Snow : WeatherMode::None));
		Require(file.GetWeatherMode() == expected, "Native weather flag priority differs.");
	}
	const LevelInfo oldFlags = FixtureLevel(engine, "weather-zwo-flags", { { 0, 200, 200, 2 } }, 0, GameEdition::Zwo, 224);
	Require(LevelFile(engine.ReadAsset(oldFlags.Map), oldFlags.Map).GetWeatherMode() == WeatherMode::None, "Zwo flags were reinterpreted as XXX weather.");
	for (ObjectKind kind : { ObjectKind::Snowflake, ObjectKind::Raindrop, ObjectKind::RisingBubble })
	{
		const u32 flags = kind == ObjectKind::Snowflake ? 32u : (kind == ObjectKind::Raindrop ? 64u : 128u);
		const LevelInfo level = FixtureLevel(engine, "weather-cadence-" + std::to_string(static_cast<i32>(kind)), { { 0, 200, 200, 2 } }, 0, GameEdition::XXX, flags);
		LevelScene scene(engine, profile, level, 1);
		for (i32 tick = 0; tick < 10; ++tick)
			scene.Update(1.0f / 60, 0, 0, false);
		Require(scene.GetObjects().Find(kind).empty(), "Weather spawned before the ten-tick entry delay expired.");
		scene.Update(1.0f / 60, 0, 0, false);
		Require(scene.GetObjects().Find(kind).size() == 1, "Weather did not spawn after the strict-negative timer boundary.");
		const i32 interval = kind == ObjectKind::Snowflake ? 1 : (kind == ObjectKind::Raindrop ? 2 : 3);
		for (i32 tick = 0; tick < interval; ++tick)
			scene.Update(1.0f / 60, 0, 0, false);
		Require(scene.GetObjects().Find(kind).size() == 1, "Weather used an inclusive-zero recurring timer.");
		scene.Update(1.0f / 60, 0, 0, false);
		Require(scene.GetObjects().Find(kind).size() == 2, "Weather recurring interval differs from native.");
		if (capture)
		{
			scene.Update(2, 0, 0, false);
			const str name = "weather-" + std::to_string(static_cast<i32>(kind)) + ".bmp";
			engine.DrawFrame(scene.Draws(), name.c_str());
		}
		LevelScene restarted(engine, profile, level, 1);
		restarted.Update(0.01f, 0, 0, false);
		Require(restarted.GetObjects().Find(kind).empty(), "Weather entry delay was not reset with the level.");
	}
	const LevelInfo plain = FixtureLevel(engine, "weather-pool", { { 0, 200, 200, 2 } }, 0, GameEdition::XXX);
	LevelScene pool(engine, profile, plain, 1);
	WorldObject particle;
	particle.Kind = ObjectKind::Snowflake;
	particle.X = 200;
	for (i32 count = 0; count < 100; ++count)
		Require(pool.GetObjects().AddTransient(particle), "Native weather pool rejected an available slot.");
	Require(!pool.GetObjects().AddTransient(particle) && EmitterParticle::Spawn(pool, ObjectKind::HeartParticle, 200, 200, true), "Weather failed to reserve 28 transient slots for other effects.");
	// Compare particle construction/motion with the saved native formulas and MSVC rand sequence.
	for (ObjectKind kind : { ObjectKind::Snowflake, ObjectKind::Raindrop, ObjectKind::RisingBubble })
	{
		u32 sharedRandom = 7;
		LevelScene scene(engine, profile, plain, 7, &sharedRandom);
		u32 expectedRandom = sharedRandom;
		auto random = [&](i32 limit) { expectedRandom = expectedRandom * 214013u + 2531011u; return static_cast<i32>((expectedRandom >> 16) & 32767) % limit; };
		const bool snow = kind == ObjectKind::Snowflake;
		const i32 layer = random(3);
		constexpr i32 snowRange[] = { 6, 10, 20 }, snowSpeed[] = { 25, 35, 45 }, snowLife[] = { 2500, 3500, 5000 };
		constexpr i32 rainRange[] = { 30, 40, 50 }, rainSpeed[] = { 150, 190, 230 }, rainLife[] = { 250, 500, 1000 };
		const f32 vx = snow ? static_cast<f32>(random(snowRange[layer]) - snowRange[layer] / 2) * 0.1f : 0;
		const f32 vy = static_cast<f32>(random(snow ? snowSpeed[layer] : rainRange[layer]) + (snow ? snowSpeed[layer] : rainSpeed[layer]));
		const f32 life = static_cast<f32>(random(snow ? snowLife[layer] : rainLife[layer]) / 10 + 100);
		const i32 frame = random(layer == 2 ? 6 : 5) + (layer == 0 ? 0 : (layer == 1 ? 5 : 10));
		const i32 width = snow ? random(20) / 2 : 0;
		const f32 angle = snow ? static_cast<f32>(random(360)) * 0.01f : 0;
		particle.Kind = kind;
		particle.X = 200;
		particle.Y = 300;
		Require(scene.GetObjects().AddTransient(particle), "Cannot construct weather regression particle.");
		WorldObject& state = scene.GetObjects()[256];
		Require(sharedRandom == expectedRandom && state.Draw.Frame == frame, "Weather construction consumed a different native random sequence.");
		scene.GetObjects().Get(256).Update(0.25f);
		const i32 x = static_cast<i32>(200 + vx * 0.25f + (snow ? width * std::cos(angle * 57.295776f) : 0));
		const i32 y = static_cast<i32>(300 + vy * 0.25f * (kind == ObjectKind::RisingBubble ? -1 : 1));
		Require(state.Draw.X == x && state.Draw.Y == y, "Weather particle motion differs from native.");
		scene.GetObjects().Get(256).Update(life / 60 + 1);
		Require(state.Freed, "Weather particle lifetime failed to expire.");
	}
	auto helpLevel = [&](const str& name, i32 fontIndex, u16 firstId = 1) {
		LevelInfo level = FixtureLevel(engine, name, { { 0, 200, 200, 2 }, { 0, 400, 200, 4 }, { 2, 600, 400, 0 } }, 0, GameEdition::XXX);
		bytestream bytes = engine.ReadAsset(level.Map);
		const i32 firstTile = (260 / 40 + 1) * 22 + 260 / 40 + 1, secondTile = (260 / 40 + 1) * 22 + 460 / 40 + 1;
		const u32 first = static_cast<u32>(firstId) << 16, second = 2u << 16;
		std::memcpy(bytes.data() + 16 + firstTile * 24 + 20, &first, 4);
		std::memcpy(bytes.data() + 16 + secondTile * 24 + 20, &second, 4);
		const std::array<i32, 4> firstLayout{ 160, 100, 680, fontIndex }, secondLayout{ 40, 400, 360, 2 };
		std::memcpy(bytes.data() + 16 + 34648, firstLayout.data(), 16);
		std::memcpy(bytes.data() + 16 + 34648 + 16, secondLayout.data(), 16);
		std::ofstream file(engine.GetData().AssetPath(level.Map), std::ios::binary | std::ios::trunc);
		file.write(reinterpret_cast<cstr>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		Require(static_cast<bool>(file), "Cannot write help regression fixture.");
		return level;
	};
	for (i32 fontIndex = 0; fontIndex < 4; ++fontIndex)
	{
		const LevelInfo level = helpLevel("help-font-" + std::to_string(fontIndex), fontIndex);
		LevelScene scene(engine, profile, level, 1, nullptr, 2);
		std::vector<SpriteDraw> help;
		scene.AppendHelp(help);
		BitmapFont font(engine, fontIndex >= 2, fontIndex == 1 || fontIndex == 3, GameEdition::XXX);
		std::vector<SpriteDraw> glyph;
		font.Append(glyph, "N", 160, 100);
		Require(!help.empty() && help.front().X == 160 && help.front().Y == 100 && help.front().Sprite->Pixels == glyph.front().Sprite->Pixels, "Help record did not select player one's layout and native font.");
		if (capture && fontIndex == 2)
		{
			std::vector<SpriteDraw> draws = scene.Draws();
			scene.AppendHelp(draws);
			engine.DrawFrame(draws, "level-help.bmp");
		}
		scene.Update(0.04f, 1, 0, false);
		help.clear();
		scene.AppendHelp(help);
		Require(help.front().X == 160, "Help marker changed before Sven committed the next tile.");
		scene.Update(0.5f, 0, 0, false);
		help.clear();
		scene.AppendHelp(help);
		Require(!help.empty() && help.front().X == 40 && help.front().Y == 400, "Player two's help did not appear after player one left his marker.");
	}
	LevelScene invalid(engine, profile, helpLevel("help-invalid", 2, 257), 1);
	std::vector<SpriteDraw> help;
	invalid.AppendHelp(help);
	Require(help.empty(), "An invalid help ID indexed beyond the layout table.");
	LevelSetInfo set;
	set.Levels.push_back(helpLevel("help-session", 2));
	GameSession session(engine, profile, set);
	const size_t playing = session.Draws().size(), overlay = session.Draws(false).size();
	Require(playing > overlay, "Session did not append help after the HUD or hide it behind menu overlays.");
	BitmapFont font(engine, true, false, GameEdition::XXX);
	font.AppendLevelHelp(help, "AA#BB", 10, 20, 1000);
	Require(help.size() == 4 && help[0].X == 10 && help[0].Y == 20 && help[2].X == 10 && help[2].Y == 36, "Native help hash break or FDD line advance is wrong.");
	help.clear();
	font.AppendLevelHelp(help, "AAAA ", 10, 20, 11);
	Require(help.size() == 4 && help.front().Y == 36 && help.back().Y == 36, "Oversized help words must wrap once and remain intact.");
	help.clear();
	font.AppendLevelHelp(help, "A", 10, 20, 11);
	Require(help.size() == 1 && help.front().Y == 36, "A final single-glyph help word bypassed native overflow wrapping.");
	if (capture)
	{
		for (const LevelPackInfo& pack : packs)
		{
			if (pack.Edition != GameEdition::XXX)
				continue;
			for (const LevelSetInfo& episode : pack.LevelSets)
			{
				if (!episode.Levels.empty() && episode.Levels.front().Map.find("training01.lvl") != str::npos)
				{
					GameSession training(engine, pack, episode);
					engine.DrawFrame(training.Draws(), "training-help.bmp");
				}
			}
		}
	}
	LevelSetInfo rainy;
	rainy.Levels.push_back(FixtureLevel(engine, "weather-paused", { { 0, 200, 200, 2 } }, 0, GameEdition::XXX, 64));
	GameSession paused(engine, profile, rainy);
	const Sprite* rain = engine.LoadSprite(engine.GetData().EditionAsset(GameEdition::XXX, "SvenXXX/gfx/rain.bmp"), 16, 1);
	auto drops = [&]() {
		std::vector<SpriteDraw> result;
		for (const SpriteDraw& draw : paused.Draws())
			if (draw.Sprite == rain)
				result.push_back(draw);
		return result;
	};
	Require(drops().empty(), "Weather started without the level-entry countdown.");
	paused.UpdateAtmosphere(0.5f);
	Require(!drops().empty() && !paused.GetResults() && paused.GetScore() == 0 && paused.GetLives() == 3, "Paused atmosphere failed to update independently of gameplay.");
	paused.Update(0.01f, 0, 0, false);
	paused.Update(2, 0, 0, false);
	Require(paused.GetResults(), "Cannot enter weather result fixture.");
	const std::vector<SpriteDraw> before = drops();
	paused.Update(0.2f, 0, 0, false);
	const std::vector<SpriteDraw> after = drops();
	Require(!before.empty() && !after.empty() && before.front().Y != after.front().Y, "Weather froze during result conversion.");
	Log("PASS: weather flag priority/cadence, native random construction/motion/lifetimes/pool, four help fonts, marker ownership/commit, absolute layout, wrapping and menu suppression.");
}

void CheckCardAwards(MudGE& engine, const fpath& saves, bool capture)
{
	const std::vector<LevelPackInfo>& packs = engine.GetData().GetPacks();
	const LevelPackInfo& zwo = *std::find_if(packs.begin(), packs.end(), [](const LevelPackInfo& pack) { return pack.Edition == GameEdition::Zwo && pack.Name == "XS"; });
	LevelPackInfo profile = zwo;
	profile.Edition = GameEdition::XXX;
	profile.Id = "XXX-" + zwo.Id;
	LevelSetInfo episode;
	episode.Name = "Cards \"native\" test";
	episode.Levels = { FixtureLevel(engine, "card-frigida", { { 0, 240, 240, 2 }, { 2, 280, 240, 1, 0 } }, 0, GameEdition::XXX) };
	const fpath storage = saves / L"card-awards";
	const Sprite* zoom = engine.LoadSprite(engine.GetData().EditionAsset(GameEdition::XXX, "SvenXXX/gfx/Highsc_file_card_zoom.bmp"), 4, 1);
	const Sprite* blank = engine.LoadSprite(engine.GetData().EditionAsset(GameEdition::XXX, "SvenXXX/gfx/Highsc_file_card_blank.bmp"), 1, 1);
	const Sprite* timer = engine.LoadSprite(engine.GetData().EditionAsset(GameEdition::XXX, "SvenXXX/gfx/timer.bmp"), 1, 1);
	const str gong = engine.GetData().SoundPath("SfxShowCard", GameEdition::XXX), bonusSound = engine.GetData().SoundPath("SfxUnlockBonus", GameEdition::XXX);
	auto complete = [](GameSession& session, i32 player = 0) {
		for (i32 tick = 0; tick < 5000 && session.GetRemainingTargets() > 0; ++tick)
		{
			std::array<PlayerInput, 2> input{};
			input[player] = { 1, 0, tick % 2 != 0 };
			session.Update(0.01f, input);
		}
		Require(session.GetRemainingTargets() == 0, "Card award fixture did not complete Frigida.");
	};
	auto contains = [](const std::vector<str>& sounds, const str& sound) { return std::find(sounds.begin(), sounds.end(), sound) != sounds.end(); };
	GameSession first(engine, profile, episode, 1, storage);
	Require(first.GetEarnedCards() == 4 && !std::filesystem::exists(storage / L"xxx-cards.tsv"), "Initial card history must be four without an unsolicited save.");
	complete(first);
	Require(first.GetEarnedCards() == 5 && contains(first.TakeSounds(), gong), "First-attempt Frigida did not earn the next card and play its gong.");
	struct Stage
	{
		f32 Seconds;
		i32 ZoomFrame;
		bool Hold;
	};
	const Stage stages[] = { { 0, 3, false }, { 0.08f, 2, false }, { 0.08f, 1, false }, { 0.09f, 0, false }, { 0.1f, -1, true }, { 2, -1, true }, { 0.2f, 0, false }, { 0.08f, 1, false }, { 0.08f, 2, false }, { 0.08f, 3, false }, { 0.08f, -1, false }, { 0.5f, -1, false } };
	const Sprite* art = ShagAnimation::LoadCard(engine, 4).Sprite;
	for (const Stage& stage : stages)
	{
		first.UpdateAtmosphere(stage.Seconds);
		const std::vector<SpriteDraw>& draws = first.Draws();
		const auto cardZoom = std::find_if(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == zoom; });
		const auto cardBlank = std::find_if(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == blank; });
		if (stage.ZoomFrame >= 0)
			Require(cardZoom != draws.end() && cardZoom->Frame == stage.ZoomFrame && cardZoom->X == 695 && cardZoom->Y == 435, "Card zoom frame/placement differs from native thresholds.");
		else
			Require(cardZoom == draws.end(), "Card zoom outlived its native interval.");
		Require((cardBlank != draws.end()) == stage.Hold, "Card hold interval differs from native.");
		if (stage.Hold)
		{
			Require(cardBlank->X == 669 && cardBlank->Y == 394 && std::any_of(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == art && draw.X == 669 && draw.Y == 404 && draw.Frame == 0; }), "Card preview must use the next catalog entry's static frame above the blank card.");
			const auto hud = std::find_if(draws.begin(), draws.end(), [&](const SpriteDraw& draw) { return draw.Sprite == timer; });
			Require(hud != draws.end() && cardBlank < hud, "Card award was drawn above the HUD instead of below it.");
			if (capture && stage.Seconds == 0.1f)
				engine.DrawFrame(draws, "card-award-hold.bmp");
		}
		if (capture && stage.Seconds == 0)
			engine.DrawFrame(draws, "card-award-zoom.bmp");
	}
	Require(first.GetScore() == 30 && first.GetLives() == 3, "Card notification changed Frigida score or lives.");
	GameSession replay(engine, profile, episode, 1, storage);
	complete(replay);
	const std::vector<str> replaySounds = replay.TakeSounds();
	Require(replay.GetEarnedCards() == 5 && !contains(replaySounds, gong) && contains(replaySounds, bonusSound), "Replayed level awarded another card or lost its bonus-unlock cue.");
	replay.DebugRestartLevel();
	complete(replay);
	Require(replay.GetEarnedCards() == 5, "Restart bypassed the level's persistent card history.");
	LevelSetInfo otherEpisode = episode;
	otherEpisode.Name += " other";
	GameSession distinct(engine, profile, otherEpisode, 1, storage);
	complete(distinct);
	Require(distinct.GetEarnedCards() == 6, "Distinct episodes incorrectly shared a level award bit.");
	LevelSetInfo coop = episode;
	coop.Name += " co-op";
	coop.Levels = { FixtureLevel(engine, "card-coop", { { 0, 600, 400, 0 }, { 0, 240, 240, 2 }, { 2, 280, 240, 1, 0 } }, 0, GameEdition::XXX) };
	GameSession secondPlayer(engine, profile, coop, 2, storage);
	complete(secondPlayer, 1);
	Require(secondPlayer.GetEarnedCards() == 7 && contains(secondPlayer.TakeSounds(), gong), "Player two did not award the shared card collection.");
	LevelSetInfo oldEpisode = episode;
	oldEpisode.Levels = { FixtureLevel(engine, "card-zwo", { { 0, 240, 240, 2 }, { 2, 280, 240, 1, 0 } }) };
	GameSession oldSource(engine, zwo, oldEpisode, 1, storage);
	complete(oldSource);
	Require(oldSource.GetEarnedCards() == 8, "Zwo source map failed to use the shared XXX card reward.");
	LevelSetInfo normal = episode;
	normal.Levels = { FixtureLevel(engine, "card-chicken", { { 0, 240, 240, 2 }, { 11, 280, 240, 1, 0 } }, 0, GameEdition::XXX) };
	GameSession ordinary(engine, profile, normal, 1, storage);
	complete(ordinary);
	Require(ordinary.GetEarnedCards() == 8 && !contains(ordinary.TakeSounds(), gong), "Ordinary animal completion awarded a Frigida card.");
	LevelSetInfo interruptedEpisode = episode;
	interruptedEpisode.Name += " interrupted";
	interruptedEpisode.Levels = { FixtureLevel(engine, "card-second-attempt", { { 0, 240, 240, 2 }, { 2, 280, 240, 1, 0 } }, 0, GameEdition::XXX) };
	GameSession interrupted(engine, profile, interruptedEpisode, 1, storage);
	interrupted.Update(0.01f, 1, 0, false);
	interrupted.Update(0.01f, 1, 0, true);
	interrupted.Update(10, 0, 0, false);
	complete(interrupted);
	Require(interrupted.GetEarnedCards() == 8 && !contains(interrupted.TakeSounds(), gong), "Second-attempt Frigida incorrectly earned a first-attempt card.");
	LevelScene requests(engine, profile, episode.Levels.front(), 1);
	WorldObject eligible = requests.GetObjects()[1];
	eligible.Temperament = 2;
	eligible.Attempts = 1;
	requests.RecordAnimalCompletion(eligible);
	requests.RecordAnimalCompletion(eligible);
	Require(requests.GetBonusUnlocked() && requests.TakeBonusUnlocks() == 2 && requests.TakeBonusUnlocks() == 0, "Simultaneous qualifying completions were lost or replayed.");
	LevelSetInfo reused = episode;
	reused.Name += " repeated asset";
	reused.Levels.push_back(reused.Levels.front());
	GameSession repeatedAsset(engine, profile, reused, 1, saves / L"card-repeated-asset");
	complete(repeatedAsset);
	for (i32 tick = 0; tick < 300 && !repeatedAsset.GetResults(); ++tick)
		repeatedAsset.Update(0.01f, 0, 0, false);
	repeatedAsset.SkipBonuses();
	repeatedAsset.Advance();
	const std::vector<SpriteDraw>& carried = repeatedAsset.Draws();
	Require(std::any_of(carried.begin(), carried.end(), [&](const SpriteDraw& draw) { return draw.Sprite == blank; }), "Changing levels cleared the native application-scoped card popup prematurely.");
	complete(repeatedAsset);
	Require(repeatedAsset.LevelNumber() == 2 && repeatedAsset.GetEarnedCards() == 6, "Native per-level-position award flags were collapsed by a reused map filename.");
	const fpath cappedStorage = saves / L"card-cap";
	std::filesystem::create_directories(cappedStorage);
	{
		std::ofstream file(cappedStorage / L"xxx-cards.tsv");
		file << "SvenCards 1\n39\n";
	}
	LevelSetInfo cappedEpisode = episode;
	cappedEpisode.Levels.push_back(FixtureLevel(engine, "card-cap-bonus", { { 0, 240, 240, 2 }, { 2, 280, 240, 1 } }, 0, GameEdition::XXX, 2));
	GameSession capped(engine, profile, cappedEpisode, 1, cappedStorage);
	complete(capped);
	const std::vector<str> cappedSounds = capped.TakeSounds();
	Require(capped.GetEarnedCards() == 39 && !contains(cappedSounds, gong) && contains(cappedSounds, bonusSound), "Full collection awarded an invalid card or suppressed the bonus cue.");
	for (i32 tick = 0; tick < 300 && !capped.GetResults(); ++tick)
		capped.Update(0.01f, 0, 0, false);
	capped.SkipBonuses();
	capped.Advance();
	Require(capped.GetBonusMode() == BonusMode::Farmer, "Full card collection prevented entry to an earned bonus level.");
	GameUi gallery(engine, saves / L"card-gallery");
	Require(gallery.CardCount() == 39, "Earned progress locked the intentionally accessible gallery.");
	const fpath corrupt = saves / L"card-corrupt";
	std::filesystem::create_directories(corrupt);
	{
		std::ofstream file(corrupt / L"xxx-cards.tsv");
		file << "SvenCards 1\n999\n";
	}
	bool rejected = false;
	try
	{
		GameSession bad(engine, profile, episode, 1, corrupt);
	}
	catch (const std::runtime_error&)
	{
		rejected = true;
	}
	Require(rejected, "Malformed card progress was accepted and could be overwritten.");
	const fpath failed = saves / L"card-write-failure";
	std::filesystem::create_directories(failed / L"xxx-cards.tsv.tmp");
	{
		std::ofstream file(failed / L"xxx-cards.tsv");
		file << "SvenCards 1\n4\n";
	}
	GameSession memory(engine, profile, episode, 1, failed);
	complete(memory);
	GameSession retained(engine, profile, episode, 1, failed);
	Require(memory.GetEarnedCards() == 5 && retained.GetEarnedCards() == 4, "Failed card save lost the in-memory award or overwrote the previous file.");
	Log("PASS: native card award frames/coordinates/layer/cues, atomic saved progress, replay/restart/episode/P2/Zwo ownership, cap/bonus behavior, gallery access and malformed/failed-save handling.");
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
		if (!engine.GetData().HasEdition(GameEdition::XXX))
		{
			const LevelPackInfo& pack = engine.GetData().GetPacks().front();
			GameUi ui(engine, saves / L"legacy-coop-fallback");
			ui.SetPlayerCount(2);
			GameSession session(engine, pack, pack.LevelSets.front(), 2);
			LevelScene scene(engine, pack, pack.LevelSets.front().Levels.front(), 1, nullptr, 2);
			Require(ui.GetPlayerCount() == 1 && session.GetPlayerCount() == 1 && scene.GetPlayerCount() == 1 && scene.GetObjects().Find(ObjectKind::Player).size() == 1, "Archival Zwo-only data attempted to use unavailable XXX multiplayer resources.");
		}
		if (editions)
		{
			CheckEditionFoundation(engine, saves);
			if (editionVisual)
				engine.StartVideo(false);
			CheckPresentationCorrections(engine, saves, editionVisual);
			CheckSharedPlayerRuntime(engine);
			CheckPlayerMovement(engine);
			CheckInteractionOwnership(engine);
			CheckSharedItems(engine, editionVisual);
			CheckArtifactAndMultiplier(engine, editionVisual);
			CheckMultiplayerFoundations(engine, editionVisual);
			CheckMultiplayerEnemies(engine);
			CheckBonusControllers(engine, saves, editionVisual);
			CheckMultiplayerPlay(engine, saves, editionVisual);
			CheckZwoCoopSpawns(engine, saves, editionVisual);
			CheckZwoCoopCatalog(engine, editionVisual);
			CheckStreaksAndMermaids(engine, editionVisual);
			CheckWeatherAndLevelHelp(engine, editionVisual);
			CheckCardAwards(engine, saves, editionVisual);
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
