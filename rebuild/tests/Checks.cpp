#include "Scene/Objects/ObjectCatalog.h"
#include "Scene/Characters/Characters.h"
#include "Utility/Logger.h"
#include "Engine/MudGE.h"
#include "Scene/LevelScene.h"
#include "Game/GameUi.h"
#include "Game/GameSession.h"
#include "FSR1/FsrPresenter.h"
#include <algorithm>
#include <cmath>
#include <fstream>
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
	auto p = FsrPresenter::LogicalMouse({960, 540}, 1920, 1080);
	Require(p.x == 400 && p.y == 300, "Scaled mouse center must map to the logical game center.");
	p = FsrPresenter::LogicalMouse({239, 100}, 1920, 1080);
	Require(p.x < 0 && p.y < 0, "Black bars must not activate game UI.");
	p = FsrPresenter::LogicalMouse({1679, 1079}, 1920, 1080);
	Require(p.x == 799 && p.y == 599, "Bottom-right scaled pixel must stay inside logical bounds.");
	r = FsrPresenter::ViewPort(800, 1000);
	Require(r.top == 200 && r.bottom == 800, "Tall windows must letterbox without distorting the game.");
	p = FsrPresenter::LogicalMouse({0, 0}, 0, 0);
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
	for (i32 x : {160, 640, 1119})
	{
		const auto c = flat[360 * 1280 + x];
		for (i32 shift : {0, 8, 16})
			Require(std::abs(static_cast<i32>((c >> shift) & 255) - static_cast<i32>((0x336699 >> shift) & 255)) <= 1,
			        "FSR must preserve constant colors, channels and edge clamping.");
	}
	for (i32 y = 0; y < 600; ++y)
		for (i32 x = 0; x < 800; ++x)
			source[y * 800 + x] = x < 400 ? 0x202020 : (y > x - 400 ? 0xf0c080 : 0x4070c0);
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
	RECT rect{0, 0, 800, 600};
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
	Require(file && header.bfType == 0x4d42 && info.biBitCount == 32 && info.biHeight < 0,
	        "Unexpected native capture format.");
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
LevelInfo FixtureLevel(MudGE& engine, const str& name,
                       std::initializer_list<FixtureActor> actors, i32 pickup = 0)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack)
	                             { return pack.Name == "XS"; });
	auto level = xs->LevelSets.front().Levels.front();
	// v6 layout is documented in docs/VERIFIED_FORMAT.md and LevelScene's parser.
	bytestream data(16 + 9048 + 25600);
	auto integer = [&](size_t offset, i32 value)
	{ std::memcpy(data.data() + offset, &value, 4); };
	auto real = [&](size_t offset, f32 value)
	{ std::memcpy(data.data() + offset, &value, 4); };
	integer(0, 6);
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
		const i32 size = actor.Kind == 5 ? 160 : actor.Kind >= 7 && actor.Kind <= 10 ? 80
		                                                                             : 120;
		const i32 x = actor.X + 60 - size / 2, y = actor.Y + 60 - size / 2;
		real(offset, static_cast<f32>(x));
		real(offset + 4, static_cast<f32>(y));
		real(offset + 8, static_cast<f32>(x));
		real(offset + 12, static_cast<f32>(y));
		integer(offset + 20, size);
		integer(offset + 24, size);
		integer(offset + 92, actor.Kind == 5 ? 40 : actor.Kind == 0                   ? 30
		                                        : actor.Kind >= 7 && actor.Kind <= 10 ? 60
		                                                                              : 0);
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
			constexpr i32 itemCodes[] = {130, 131, 129, 128};
			integer(offset + 68, 1); // Permanent distraction fixture.
			data[16 + tile * 24 + 12] = static_cast<u8>(itemCodes[actor.Kind - 7]);
			integer(16 + tile * 24 + 16, slot);
		}
		else
			integer(16 + tile * 24, actor.Kind == 0 ? 100 : actor.Kind >= 11 ? actor.Kind - 4
			                                                                 : actor.Kind);
		if (slot == 0 && pickup)
			data[16 + tile * 24 + 12] = static_cast<u8>(pickup);
		++slot;
	}
	level.Map = "XS/levels/check-" + name + ".lvl";
	std::ofstream output(engine.GetData().GetRoot() / std::filesystem::u8path(level.Map), std::ios::binary);
	output.write(reinterpret_cast<cstr>(data.data()), static_cast<std::streamsize>(data.size()));
	if (!output)
		throw std::runtime_error("Cannot write isolated regression level.");
	return level;
}

void CheckDepthOrdering(MudGE& engine, bool capture = false)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& p)
	                             { return p.Name == "XS"; });
	auto addScenery = [&](LevelInfo level, i32 y, i32 texture = 0, i32 height = 80, i32 adjustment = 0, i32 x = 240)
	{
		auto bytes = engine.ReadAsset(level.Map);
		const size_t record = 16 + 9048 + 2 * 100;
		auto integer = [&](size_t offset, i32 value)
		{ std::memcpy(bytes.data() + record + offset, &value, 4); };
		auto real = [&](size_t offset, f32 value)
		{ std::memcpy(bytes.data() + record + offset, &value, 4); };
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
	auto index = [](const std::vector<SpriteDraw>& draws, const Sprite* sprite, i32 x, i32 y)
	{
		const auto found = std::find_if(draws.begin(), draws.end(), [&](const auto& d)
		                                { return d.Sprite == sprite && d.X == x && d.Y == y; });
		Require(found != draws.end(), "Depth regression sprite is missing.");
		return found - draws.begin();
	};
	cstr files[] = {"Chicken", "Goat", "Pig"};
	for (i32 kind = 11; kind <= 13; ++kind)
	{
		const auto* animalSprite = engine.LoadSprite(str("Sven2/gfx/Animals/") + files[kind - 11] + "_Stand.bmp", 4, 1);
		const auto north = FixtureLevel(engine, "depth-north-" + std::to_string(kind), {{0, 240, 160, 4}, {kind, 240, 200, 1}});
		LevelScene approach(engine, *xs, north, 1);
		const auto& front = approach.Draws();
		Require(index(front, animalSprite, 240, 200) > index(front, approach.GetPlayerDraw().Sprite, 240, 160),
		        "Sven approaching from above painted his legs over the animal.");
		if (capture)
			engine.DrawFrame(front, ("animal-north-" + std::to_string(kind) + ".bmp").c_str());
		const auto south = addScenery(FixtureLevel(engine, "depth-interaction-" + std::to_string(kind), {{0, 240, 240, 0}, {kind, 240, 200, 0}}), 230);
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
			Require(index(active, interacting.GetPlayerDraw().Sprite, 240, 200) < index(active, scenery, 240, 230),
			        "Animal interaction jumped in front of intervening scenery.");
			Require(std::none_of(active.begin(), active.end(), [&](const auto& d)
			                     { return d.Sprite == animalSprite; }),
			        "Animal stand sprite was drawn again over its combined interaction.");
			if (capture && frame == 0)
				engine.DrawFrame(active, ("animal-behind-interaction-" + std::to_string(kind) + ".bmp").c_str());
			interacting.Update(0.01f, 0, 0, false);
		}
	}
	const auto moving = addScenery(FixtureLevel(engine, "depth-mid-step", {{0, 240, 160, 4}, {11, 480, 400, 0}}), 175);
	const auto& texture = engine.GetData().TileSet(*xs, moving).Tiles.front();
	const auto* scenery = engine.LoadSprite(texture.File, texture.Frames, 1);
	LevelScene walk(engine, *xs, moving, 1);
	const auto& before = walk.Draws();
	Require(index(before, walk.GetPlayerDraw().Sprite, 240, 160) < index(before, scenery, 240, 175), "Mid-step depth fixture has the wrong initial order.");
	walk.Move(0.1f, 0, 1);
	Require(walk.GetPlayerDraw().Y > 160 && walk.GetPlayerDraw().Y < 200, "Depth fixture did not stop between grid cells.");
	const auto& after = walk.Draws();
	Require(index(after, walk.GetPlayerDraw().Sprite, walk.GetPlayerDraw().X, walk.GetPlayerDraw().Y) > index(after, scenery, 240, 175),
	        "Moving Sven kept the old tile's depth until committing his step.");
	const auto xxl = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& p)
	                              { return p.Name == "XXL"; });
	auto heapLevel = FixtureLevel(engine, "depth-forest-heap", {{0, 240, 280, 4}, {11, 480, 400, 0}});
	heapLevel.TileSet = xxl->LevelSets.front().Levels.front().TileSet;
	heapLevel = addScenery(heapLevel, 379, 2, 19, 20, 284);
	const auto& heapImage = engine.GetData().TileSet(*xxl, heapLevel).Tiles.at(2);
	const auto* heap = engine.LoadSprite(heapImage.File, heapImage.Frames, 1);
	LevelScene crossing(engine, *xxl, heapLevel, 1);
	crossing.Move(0.1f, 0, 1);
	const auto& crossingDraws = crossing.Draws();
	Require(crossing.GetPlayerDraw().Y > 280 && crossing.GetPlayerDraw().Y < 320 &&
	            index(crossingDraws, crossing.GetPlayerDraw().Sprite, crossing.GetPlayerDraw().X, crossing.GetPlayerDraw().Y) > index(crossingDraws, heap, 284, 379),
	        "Forest dirt heap covered Sven after his rendered depth crossed it mid-step.");
	if (capture)
		engine.DrawFrame(crossingDraws, "forest-heap-mid-step.bmp");
	Log("PASS: interpolated depth, north animal approaches and occluded chicken/goat/pig interactions.");
}

void CheckSerializedPowerups(MudGE& engine)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& p)
	                             { return p.Name == "XS"; });
	const auto* icons = engine.LoadSprite("Sven2/gfx/icons.bmp", 5, 1);
	cstr durations[] = {"FireworkTime", "GiftTime", "ImpotentTime", "SmellTime", "FlowerTime"};
	for (i32 kind = 14; kind <= 18; ++kind)
	{
		auto level = FixtureLevel(engine, "serialized-powerup-" + std::to_string(kind), {{0, 240, 200, 2}, {7, 280, 200, 0}, {2, 640, 400, 0}});
		auto bytes = engine.ReadAsset(level.Map);
		const size_t record = 16 + 9048 + 100;
		auto put = [&](size_t off, i32 value)
		{ std::memcpy(bytes.data() + off, &value, 4); };
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
		Require(std::any_of(initial.begin(), initial.end(), [&](const auto& d)
		                    { return d.Sprite == icons && d.Frame == kind - 14; }),
		        "Serialized powerup used the wrong icon frame.");
		Step(pickup, 1, 0);
		const auto& collected = pickup.Draws();
		Require(std::none_of(collected.begin(), collected.end(), [&](const auto& d)
		                     { return d.Sprite == icons; }),
		        "Serialized powerup did not disappear on collection.");
		Require(!pickup.TakeSounds().empty(), "Serialized powerup did not activate on collection.");
		const f32 duration = static_cast<f32>(engine.GetData().NumericParam(durations[kind - 14], 10));
		expiry.Update(duration + 0.1f, 0, 0, false);
		const auto& expired = expiry.Draws();
		Require(std::none_of(expired.begin(), expired.end(), [&](const auto& d)
		                     { return d.Sprite == icons; }),
		        "Serialized powerup did not expire at its native Zwo duration.");
	}
	Log("PASS: all five serialized Zwo powerup kinds render, collect and expire.");
}

void DriveTutorial(MudGE& engine,
                   const std::function<void(f32, i32, i32, bool)>& update,
                   const std::function<const std::vector<SpriteDraw>&()>& draws,
                   const std::function<i32()>& remaining)
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
	constexpr i32 directions[][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
	auto cellAt = [](const SpriteDraw& draw)
	{
		const i32 x = (draw.X + 60) / 40, y = (draw.Y + 60) / 40;
		return x >= 0 && x < 20 && y >= 0 && y < 15 ? y * 20 + x : -1;
	};
	f32 elapsed = 0;
	auto tick = [&](f32 seconds, i32 dx, i32 dy, bool action)
	{
		update(seconds, dx, dy, action);
		elapsed += seconds;
	};
	while (remaining() > 0 && elapsed < 150)
	{
		const auto& frame = draws();
		auto hero = std::find_if(frame.begin(), frame.end(), [&](const auto& draw)
		                         { return draw.Sprite == getPlayerDraw; });
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
	Require(std::any_of(credits.begin(), credits.end(), [&](const auto& draw)
	                    { return draw.Sprite == smallFont && draw.X == 120 && draw.Y == 220 && draw.Frame == 2; }),
	        "Credits shifted the Coloration row into subsequent text.");
	Require(std::any_of(credits.begin(), credits.end(), [&](const auto& draw)
	                    { return draw.Sprite == smallFont && draw.X == 120 && draw.Y == 380 && draw.Frame == 21; }),
	        "Credits did not retain the original two-column thanks row.");
	ui.Show(Page::Highscores);
	Require(ui.Draws(0).size() > 100, "Original highscore defaults are absent.");
	const auto firstEpisode = ui.GetPack().Name + "/" + ui.GetLevelSet().Name;
	WindowInput::Result nextEpisode;
	nextEpisode.Click = true;
	nextEpisode.Mouse = {275, 100};
	ui.Handle(nextEpisode);
	Require(ui.GetPack().Name + "/" + ui.GetLevelSet().Name != firstEpisode, "Highscore mouse arrow did not change episodes.");
	WindowInput::Result previousEpisode;
	previousEpisode.Left = true;
	ui.Handle(previousEpisode);
	Require(ui.GetPack().Name + "/" + ui.GetLevelSet().Name == firstEpisode, "Highscore keyboard traversal did not restore the episode.");
	WindowInput::Result cards;
	cards.Click = true;
	cards.Mouse = {510, 40};
	ui.Handle(cards);
	const auto& cardDraws = ui.Draws(0);
	Require(std::any_of(cardDraws.begin(), cardDraws.end(), [](const auto& draw)
	                    { return draw.X == 310 && draw.Y == 150; }),
	        "Card omitted its title.");
	Require(std::any_of(cardDraws.begin(), cardDraws.end(), [](const auto& draw)
	                    { return draw.X == 310 && draw.Y == 200; }),
	        "Card omitted its description.");
	const auto variants = ShagAnimation::Load(engine);
	for (i32 card = 0; card < 33; ++card)
	{
		const auto& draws = ui.Draws(0);
		Require(std::any_of(draws.begin(), draws.end(), [&](const auto& draw)
		                    { return draw.Sprite == variants[static_cast<size_t>(card) * 2].Sprite; }),
		        "An available original card was incorrectly locked.");
		WindowInput::Result nextCard;
		nextCard.Right = true;
		ui.Handle(nextCard);
	}
	WindowInput::Result playCard;
	playCard.Confirm = true;
	ui.Handle(playCard);
	const auto cardSounds = ui.TakeSounds();
	Require(cardSounds.size() == 1 && cardSounds.front() == variants.front().Sound,
	        "Card playback omitted its original sound.");
	ui.Handle(playCard);
	Require(ui.TakeSounds().empty(), "Card input restarted an animation that was already playing.");
	const auto& playback = ui.Draws(0.11f);
	Require(std::any_of(playback.begin(), playback.end(), [&](const auto& draw)
	                    { return draw.Sprite == variants.front().Sprite && draw.Frame == variants.front().Frames[1]; }),
	        "Card browser did not use the original 15 fps animation clock.");
	const auto* arrows = engine.LoadSprite("Sven2/gfx/pijlen.bmp", 8, 1);
	WindowInput::Result arrowHover;
	arrowHover.MouseMoved = true;
	arrowHover.Mouse = {245, 360};
	ui.Handle(arrowHover);
	const auto& hoveredArrow = ui.Draws(0);
	Require(std::any_of(hoveredArrow.begin(), hoveredArrow.end(), [&](const auto& draw)
	                    { return draw.Sprite == arrows && draw.X == 238 && draw.Y == 350 && draw.Frame == 3; }),
	        "Card arrow did not change color on hover.");
	const auto* paper = engine.LoadSprite("Sven2/gfx/Help_BG.jpg", 1, 1);
	const auto* scoreTabs = engine.LoadSprite("Sven2/gfx/Highsc_file_cards.bmp", 10, 1);
	Require(std::all_of(hoveredArrow.begin(), hoveredArrow.end(), [&](const auto& draw)
	                    { return draw.Sprite != scoreTabs || draw.ClipBottom == 60; }),
	        "Highscore tabs extend over the paper body.");
	Require(std::any_of(hoveredArrow.begin(), hoveredArrow.end(), [&](const auto& draw)
	                    { return draw.Sprite == paper; }),
	        "Highscore paper backdrop is absent.");
	ui.Show(Page::Pause);
	WindowInput::Result escape;
	escape.Back = true;
	Require(ui.Handle(escape) == UiAction::Resume, "Pause escape did not resume.");
	WindowInput::Result chooseNo;
	chooseNo.Right = true;
	ui.Handle(chooseNo);
	Require(ui.Handle(input) == UiAction::Resume, "Pause No did not resume.");
	ui.ShowResults({1250, 1, 180, 3, 3, 0, 0, 0, true, false});
	ui.Handle(input);
	Require(ui.GetPage() == Page::Name, "A qualifying round did not open name entry.");
	WindowInput::Result typing;
	typing.Text = u8"J\u00f6rg";
	ui.Handle(typing);
	BitmapFont nameFont(engine);
	const auto* largeFont = engine.LoadSprite("Sven2/gfx/font1.bmp", 88, 1);
	const auto& nameDraws = ui.Draws(0);
	Require(std::any_of(nameDraws.begin(), nameDraws.end(), [&](const auto& draw)
	                    { return draw.Sprite == largeFont && draw.X == 150 + nameFont.Width(typing.Text) && draw.Y == 130 && draw.Frame == 8; }),
	        "Highscore name caret did not use the original I glyph in the ranked row.");
	ui.Handle(input);
	Require(ui.GetPage() == Page::Highscores, "Name entry did not return to the score screen.");
	Highscores saved(saves / L"XS-0.tsv");
	Require(saved.GetEntries().front().Score == 1250, "Results did not persist the submitted highscore.");
	GameUi hover(engine, saves / L"hover");
	hover.Show(Page::GetPacks);
	WindowInput::Result mouse;
	mouse.MouseMoved = true;
	mouse.Mouse = {200, 170};
	hover.Handle(mouse);
	const auto& hoverDraws = hover.Draws(0);
	const auto* green = engine.LoadSprite("Sven2/gfx/fontgreen1.bmp", 88, 1);
	Require(std::any_of(hoverDraws.begin(), hoverDraws.end(), [&](const auto& draw)
	                    { return draw.Sprite == green && draw.Y == 158; }),
	        "Hover did not make the episode text green.");
	mouse.Click = true;
	Require(hover.Handle(mouse) == UiAction::Start, "Clicking a hovered episode did not start it.");
	GameUi exitMenu(engine, saves / L"exit-menu");
	WindowInput::Result exitClick;
	exitClick.Click = true;
	exitClick.Mouse = {650, 555};
	exitMenu.Handle(exitClick);
	Require(exitMenu.GetPage() == Page::Quit, "Exit button did not open confirmation.");
	const auto* logo = engine.LoadSprite("Sven2/gfx/SvenLogo.bmp", 1, 1);
	const auto* logoDetail = engine.LoadSprite("Sven2/gfx/SvenLogo_O.bmp", 4, 1);
	// Model a retained completed session being supplied after returning to title.
	const std::vector<SpriteDraw> staleLevel = {{logo, -777, -777, 0}};
	const auto& exitDraws = exitMenu.Draws(0, &staleLevel);
	Require(!exitMenu.UsesSceneBackground() && std::none_of(exitDraws.begin(), exitDraws.end(), [](const auto& draw)
	                                                        { return draw.X == -777; }),
	        "Title exit confirmation drew the retained level behind the menu.");
	Require(std::any_of(exitDraws.begin(), exitDraws.end(), [&](const auto& draw)
	                    { return draw.Sprite == logo && draw.X == 234 && draw.Y == 180; }) &&
	            std::any_of(exitDraws.begin(), exitDraws.end(), [&](const auto& draw)
	                        { return draw.Sprite == logoDetail && draw.X == 429 && draw.Y == 316; }),
	        "Exit confirmation removed the title artwork behind it.");
	Require(std::count_if(exitDraws.begin(), exitDraws.end(), [](const auto& draw)
	                      { return draw.Y == 545; }) == 4,
	        "Exit confirmation removed the four menu buttons.");
	exitMenu.Handle(chooseNo);
	Require(exitMenu.Handle(input) == UiAction::None && exitMenu.GetPage() == Page::Title,
	        "Declining exit did not return to the title.");
	exitMenu.Show(Page::Pause);
	const auto& pauseDraws = exitMenu.Draws(0, &staleLevel);
	Require(exitMenu.UsesSceneBackground() && pauseDraws.front().X == -777, "Pause dialog lost its active level background.");
	RoundResult completed{1250, 1, 180, 3, 3, 0, 0, 0, false, false, 3, 2};
	ui.ShowResults(completed);
	const auto firstDrawCount = ui.Draws(0.1f).size();
	ui.ShowResults(completed);
	Require(ui.Draws(0).size() == firstDrawCount, "Updating bonus values restarted the blinking prompt every frame.");
	const auto* scoreSprite = engine.LoadSprite("Sven2/gfx/score.bmp", 4, 1);
	const auto& bonusDraws = ui.Draws(0);
	Require(std::any_of(bonusDraws.begin(), bonusDraws.end(), [&](const auto& draw)
	                    { return draw.Sprite == scoreSprite && draw.X == 395 && draw.Y == 380 && draw.Frame == 2; }),
	        "Bonus screen lost the session's score-icon animation frame.");
	const auto* sheepSprite = engine.LoadSprite("Sven2/gfx/Sheep/Sheep_Stand.bmp", 4, 1);
	Require(std::any_of(bonusDraws.begin(), bonusDraws.end(), [&](const auto& draw)
	                    { return draw.Sprite == sheepSprite && draw.X == 310 - sheepSprite->FrameWidth / 2 && draw.Y == 240 - sheepSprite->FrameHeight / 2; }),
	        "Bonus sheep sprite was not centered at the original coordinates.");
	completed.EpisodeComplete = true;
	completed.TotalSheep = 17;
	ui.ShowResults(completed);
	const auto* packImage = engine.LoadSprite("Sven2/gfx/ScoreScreen_BG_levelpack.bmp", 1, 1);
	const auto& finishDraws = ui.Draws(0.1f);
	Require(std::any_of(finishDraws.begin(), finishDraws.end(), [&](const auto& draw)
	                    { return draw.Sprite == packImage && draw.X == 315 && draw.Y == 294; }),
	        "Pack completion screen omitted its original illustration.");
	Log("PASS: flat episode selection, hover, highscore traversal, pause and four help tabs.");
}
void CheckScores(const fpath& saves)
{
	const auto file = saves / L"persistence.tsv";
	Highscores scores(file, {{"A", 1000, 0}, {"B", 900, 0}});
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
	Require((placeholder->Pixels.front() & 0xffffff) == 0xff00ff,
	        "Original RGB565-equivalent chroma key was not normalized.");
	const i32 expected[] = {74, 61, 9, 33, 42, 32, 46, 37, 39, 60, 59, 66, 67, 73, 86, 43, 85};
	for (bool smallFont : {false, true})
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
	DriveTutorial(engine, [&](f32 dt, i32 dx, i32 dy, bool action)
	              { scene.Update(dt, dx, dy, action); }, [&]() -> const auto&
	              { return scene.Draws(); }, [&]
	              { return scene.GetRemainingTargets(); });
	Require(scene.Completed() && scene.GetCompletedSheep() == 3, "Tutorial did not reach completion after resolving its three targets.");
	Log("PASS: normal-entry sheep interaction, repeated input, target resolution, score and time rewards.");
}

void CheckReportedGameplay(MudGE& engine)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack)
	                             { return pack.Name == "XS"; });
	const auto walking = FixtureLevel(engine, "walking", {{0, 240, 200, 0}, {2, 640, 400, 0}});
	LevelScene east(engine, *xs, walking, 1), west(engine, *xs, walking, 1);
	east.Update(0.05f, 1, 0, false);
	west.Update(0.05f, -1, 0, false);
	Require(east.GetPlayerDraw().Mirror && !west.GetPlayerDraw().Mirror, "Sven's side-running sprite faces away from movement.");

	const auto angry = FixtureLevel(engine, "angry", {{0, 240, 200, 0}, {2, 240, 160, 0, 10}, {2, 640, 400, 0}});
	LevelScene kicked(engine, *xs, angry, 1);
	const auto variants = ShagAnimation::Load(engine);
	std::set<const Sprite*> usedVariants;
	i32 kickSounds = 0;
	for (i32 frame = 0; frame < 60 * 60 && kicked.GetRemainingTargets() == 2; ++frame)
	{
		kicked.Update(1.0f / 60, 0, 0, frame % 8 == 0);
		if (std::any_of(variants.begin(), variants.end(), [&](const auto& variant)
		                { return variant.Sprite == kicked.GetPlayerDraw().Sprite; }))
			usedVariants.insert(kicked.GetPlayerDraw().Sprite);
		for (const auto& sound : kicked.TakeSounds())
			if (sound == "Sven2/sfx/Sheep_kickSven.ogg")
				++kickSounds;
	}
	Require(kickSounds == 1, "Angry sheep repeatedly kicked during one stationary approach.");
	Require(kicked.GetRemainingTargets() == 1, "Sheep could not be interacted with after its kick.");
	Require(usedVariants.size() == 1, "Repeated action switched animation during one sheep interaction.");

	const auto smelly = FixtureLevel(engine, "smelly", {{0, 200, 240, 0}, {2, 240, 160, 0, 6}, {2, 640, 400, 0}}, 135);
	LevelScene stink(engine, *xs, smelly, 1);
	stink.Update(0.01f, 0, 0, false);
	Step(stink, 0, -1);
	const auto sounds = stink.TakeSounds();
	Require(std::find(sounds.begin(), sounds.end(), "Sven2/sfx/item_stinky.ogg") != sounds.end(), "Stinky mushroom was not picked up.");
	Require(std::any_of(sounds.begin(), sounds.end(), [](const auto& sound)
	                    { return sound == "Sven2/sfx/Sheep_beamup.ogg" || sound == "Sven2/sfx/Sheep_angry2.ogg"; }),
	        "Smell did not emit the sheep's anger reaction.");
	const auto* mood = engine.LoadSprite("Sven2/gfx/symbols.bmp", 12, 1);
	const auto& stinkDraws = stink.Draws();
	Require(std::any_of(stinkDraws.begin(), stinkDraws.end(), [&](const auto& draw)
	                    { return draw.Sprite == mood && draw.Y == 160 && draw.Frame >= 10; }),
	        "Smell did not anger the neighboring sheep.");

	const auto cold = FixtureLevel(engine, "frigida", {{0, 240, 200, 0}, {2, 240, 160, 0, 6}});
	LevelScene frigida(engine, *xs, cold, 1);
	for (i32 frame = 0; frame < 90; ++frame)
		frigida.Update(1.0f / 60, 0, 0, frame % 8 == 0);
	const auto* kettle = engine.LoadSprite("Sven2/gfx/symbol_kettle.bmp", 12, 1);
	const auto& coldDraws = frigida.Draws();
	Require(std::any_of(coldDraws.begin(), coldDraws.end(), [&](const auto& draw)
	                    { return draw.Sprite == kettle && draw.Frame > 0; }),
	        "Frigida interaction hid its kettle indicator.");
	const auto kettlePosition = std::find_if(coldDraws.begin(), coldDraws.end(), [&](const auto& draw)
	                                         { return draw.Sprite == kettle; });
	const auto interactionPosition = std::find_if(coldDraws.begin(), coldDraws.end(), [&](const auto& draw)
	                                              { return draw.Sprite == frigida.GetPlayerDraw().Sprite; });
	Require(kettlePosition > interactionPosition, "Frigida interaction painted over its kettle indicator.");

	const auto enemy = FixtureLevel(engine, "fight", {{0, 240, 200, 0}, {4, 280, 200, 6}, {2, 640, 400, 0}});
	LevelScene fight(engine, *xs, enemy, 1);
	const auto* cloud = engine.LoadSprite("Sven2/gfx/anim_cloud.bmp", 10, 1);
	bool fought = false;
	for (i32 frame = 0; frame < 600 && !fought; ++frame)
	{
		fight.Update(1.0f / 60, 0, 0, false);
		const auto& draws = fight.Draws();
		fought = std::any_of(draws.begin(), draws.end(), [&](const auto& draw)
		                     { return draw.Sprite == cloud; });
	}
	Require(fought, "Enemy battle did not render the fight cloud.");
	const auto animals = FixtureLevel(engine, "animal-meters", {{0, 240, 200, 0}, {12, 240, 160, 0}, {11, 400, 160, 0}, {13, 560, 160, 0}});
	LevelScene otherAnimals(engine, *xs, animals, 1);
	const auto& animalDraws = otherAnimals.Draws();
	Require(std::count_if(animalDraws.begin(), animalDraws.end(), [&](const auto& draw)
	                      { return draw.Sprite == mood; }) == 3,
	        "Chicken, goat and pig did not all display their impatience meters.");
	otherAnimals.Update(0.1f, 0, 0, true);
	const auto& goatDraws = otherAnimals.Draws();
	const auto goatMeter = std::find_if(goatDraws.begin(), goatDraws.end(), [&](const auto& draw)
	                                    { return draw.Sprite == mood && draw.Y == 150 && draw.X < 320; });
	const auto goatInteraction = std::find_if(goatDraws.begin(), goatDraws.end(), [&](const auto& draw)
	                                          { return draw.Sprite == otherAnimals.GetPlayerDraw().Sprite; });
	Require(goatMeter != goatDraws.end() && goatMeter > goatInteraction,
	        "Goat interaction covered or omitted its impatience meter.");
	Log("PASS: run direction, single kick recovery, stable interaction, smell, Frigida kettle and fight animation.");
}

void CheckEnemyAcquisition(MudGE& engine)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack)
	                             { return pack.Name == "XS"; });
	auto hasSound = [&](const std::vector<str>& sounds, cstr name)
	{
		const auto path = engine.GetData().SoundPath(name);
		return !path.empty() && std::find(sounds.begin(), sounds.end(), path) != sounds.end();
	};
	const auto outside = FixtureLevel(engine, "dog-boundary", {{0, 240, 200, 0}, {4, 360, 200, 6}, {2, 640, 400, 0}});
	const auto reservation = FixtureLevel(engine, "player-reservation", {{0, 240, 200, 0}, {5, 240, 240, 0, 6, 0, 0, 0.15f}, {2, 640, 400, 0}});
	LevelScene moving(engine, *xs, reservation, 1);
	moving.Update(0.2f, 1, 0, false);
	Require(moving.GetPlayerDraw().X > 260 && moving.GetPlayerDraw().X < 280, "Reservation fixture did not cross the half-step point.");
	const auto* farmerNorth = engine.LoadSprite("Sven2/gfx/Guy/anim_guy_walk_back.bmp", 16, 1);
	const auto* farmerDiagonal = engine.LoadSprite("Sven2/gfx/Guy/anim_guy_walk_diagonal_back.bmp", 16, 1);
	const auto& movingDraws = moving.Draws();
	Require(std::none_of(movingDraws.begin(), movingDraws.end(), [&](const auto& draw)
	                     { return draw.Sprite == farmerNorth || (draw.Sprite == farmerDiagonal && draw.Mirror); }),
	        "Enemy entered Sven's committed source or reserved destination during a half-finished step.");
	LevelScene distant(engine, *xs, outside, 1);
	distant.Update(0.1f, 0, 0, false);
	Require(!hasSound(distant.TakeSounds(), "SfxDogFoundSven"), "Dog acquired Sven at the excluded D=3 boundary.");
	const auto inside = FixtureLevel(engine, "dog-notice", {{0, 240, 200, 0}, {4, 320, 200, 6}, {2, 640, 400, 0}});
	LevelScene noticed(engine, *xs, inside, 1);
	noticed.Update(0.01f, 0, 0, false);
	Require(hasSound(noticed.TakeSounds(), "SfxDogFoundSven"), "Dog did not bark on acquiring Sven.");
	noticed.Update(0.2f, 0, 0, false);
	Require(!hasSound(noticed.TakeSounds(), "SfxDogFoundSven"), "Dog repeated its acquisition bark while walking the same tile.");

	const auto boxed = FixtureLevel(engine, "dog-blocked", {{0, 240, 200, 0}, {4, 320, 200, 6}, {2, 640, 400, 0}});
	{
		std::fstream file(engine.GetData().GetRoot() / std::filesystem::u8path(boxed.Map), std::ios::in | std::ios::out | std::ios::binary);
		const i32 dogTile = ((200 + 60) / 40 + 1) * 22 + (320 + 60) / 40 + 1;
		for (i32 dy = -1; dy <= 1; ++dy)
			for (i32 dx = -1; dx <= 1; ++dx)
			{
				if (!dx && !dy)
					continue;
				const i32 tile = dogTile + dy * 22 + dx;
				const i32 wall = 1;
				file.seekp(16 + tile * 24);
				file.write(reinterpret_cast<cstr>(&wall), 4);
			}
		Require(static_cast<bool>(file), "Cannot author blocked-enemy fixture.");
	}
	LevelScene blocked(engine, *xs, boxed, 1);
	blocked.Update(0.01f, 0, 0, false);
	Require(hasSound(blocked.TakeSounds(), "SfxDogFoundSven"), "Wall incorrectly suppressed enemy detection.");
	blocked.Update(2.0f, 0, 0, false);
	Require(!hasSound(blocked.TakeSounds(), "SfxDogFoundSven"), "Blocked enemy ignored its retry delay and repeated notice.");

	const auto bone = FixtureLevel(engine, "dog-bone", {{0, 240, 200, 0}, {4, 320, 200, 6}, {9, 320, 240, 0}, {2, 640, 400, 0}});
	LevelScene distracted(engine, *xs, bone, 1);
	distracted.Update(0.01f, 0, 0, false);
	const auto notice = distracted.TakeSounds();
	Require(hasSound(notice, "SfxDogFoundBone") && !hasSound(notice, "SfxDogFoundSven"),
	        "Dog did not prioritize a nearby bone over Sven.");
	distracted.Update(1.0f, 0, 0, false);
	Require(hasSound(distracted.TakeSounds(), "SfxDogDig"), "Dog failed to consume its bone and start digging.");

	const auto beer = FixtureLevel(engine, "farmer-beer", {{0, 640, 400, 0}, {5, 240, 200, 2}, {7, 280, 200, 0}, {2, 640, 120, 0}});
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
	Require(hasSound(sleepSounds, "SfxLarsFall") && hasSound(sleepSounds, "SfxLarsSleep"),
	        "Farmer's drinking-to-sleep transition omitted its fall or snore sound.");

	const auto idle = FixtureLevel(engine, "farmer-idle", {{0, 240, 200, 0}, {5, 320, 200, 6}, {2, 640, 400, 0}});
	LevelScene farmer(engine, *xs, idle, 1);
	farmer.Update(0.1f, 0, 0, false);
	Require(!hasSound(farmer.TakeSounds(), "SfxGuyFoundSven"), "Awake farmer used a dog-style proximity acquisition.");
	const auto active = FixtureLevel(engine, "farmer-activity", {{0, 240, 200, 0}, {2, 240, 160, 0}, {2, 640, 400, 0}, {5, 640, 40, 4}});
	LevelScene activity(engine, *xs, active, 1);
	activity.Update(0.01f, 0, 0, true);
	activity.Update(0.02f, 0, 0, false);
	Require(hasSound(activity.TakeSounds(), "SfxGuyFoundSven"), "Farmer ignored Sven's interaction outside proximity range.");
	for (i32 kind : {3, 6})
	{
		cstr sound = kind == 3 ? "SfxAlienFoundSven" : "SfxBroemseFoundSven";
		const auto nearby = FixtureLevel(engine, "notice-" + std::to_string(kind), {{0, 240, 200, 0}, {kind, 360, 200, 6}, {2, 640, 400, 0}});
		LevelScene acquisition(engine, *xs, nearby, 1);
		acquisition.Update(0.01f, 0, 0, false);
		Require(hasSound(acquisition.TakeSounds(), sound), "Alien or girl did not notice Sven inside its original square range.");
		const auto excluded = FixtureLevel(engine, "boundary-" + std::to_string(kind), {{0, 240, 200, 0}, {kind, 400, 200, 6}, {2, 640, 400, 0}});
		LevelScene boundary(engine, *xs, excluded, 1);
		boundary.Update(0.01f, 0, 0, false);
		Require(!hasSound(boundary.TakeSounds(), sound), "Alien or girl noticed Sven at the excluded D=4 boundary.");
	}
	Log("PASS: strict dog detection range, one-shot notice, bone priority/digging, farmer drink and activity detection.");
}

void CheckRound(MudGE& engine)
{
	const auto& getPacks = engine.GetData().GetPacks();
	auto pack = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& item)
	                         { return item.Name == "XS"; });
	Require(pack != getPacks.end(), "XS package missing.");
	GameSession game(engine, *pack, pack->LevelSets.front());
	DriveTutorial(engine, [&](f32 dt, i32 dx, i32 dy, bool action)
	              { game.Update(dt, dx, dy, action); }, [&]() -> const auto&
	              { return game.Draws(); }, [&]
	              { return game.GetRemainingTargets(); });
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
	LevelSetInfo finalSet{"Single tutorial", {pack->LevelSets.front().Levels.front()}};
	GameSession finalLevel(engine, *pack, finalSet);
	DriveTutorial(engine, [&](f32 dt, i32 dx, i32 dy, bool action)
	              { finalLevel.Update(dt, dx, dy, action); }, [&]() -> const auto&
	              { return finalLevel.Draws(); }, [&]
	              { return finalLevel.GetRemainingTargets(); });
	finalLevel.Update(1.51f, 0, 0, false);
	Require(finalLevel.GetResults() && !finalLevel.GetResult().EpisodeComplete, "Final level skipped its bonus screen.");
	finalLevel.SkipBonuses();
	Require(finalLevel.Ready() && !finalLevel.Finished(), "Final bonus completion prematurely opened the pack screen.");
	finalLevel.TakeSounds();
	finalLevel.Advance();
	const auto sounds = finalLevel.TakeSounds();
	Require(finalLevel.Finished() && finalLevel.GetResult().EpisodeComplete &&
	            std::find(sounds.begin(), sounds.end(), engine.GetData().SoundPath("SfxFinishPack")) != sounds.end(),
	        "Acknowledging the final bonus did not open pack completion with its sound.");
	Log("PASS: final level bonus, acknowledgement and separate pack-completion transition.");
}

void CheckLifeChanges(MudGE& engine, bool capture = false)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack)
	                             { return pack.Name == "XS"; });
	const auto arena = FixtureLevel(engine, "life-alien", {{0, 240, 200, 0}, {3, 280, 200, 6}, {2, 640, 400, 0}});
	const auto* icon = engine.LoadSprite("Sven2/gfx/Life_icon.bmp", 1, 1);
	const auto newLife = engine.GetData().SoundPath("SfxNewLife");
	const auto loseLife = engine.GetData().SoundPath("SfxLoseLife");
	LevelSetInfo freshSet{"Zero score", {arena}};
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
	Require(std::count_if(graceDraws.begin(), graceDraws.end(), [&](const auto& draw)
	                      { return draw.Sprite == icon && draw.Y == 565; }) == 2,
	        "Refund did not temporarily reduce the displayed life count.");
	Require(std::any_of(graceDraws.begin(), graceDraws.end(), [&](const auto& draw)
	                    { return draw.Sprite == icon && draw.Y < 500; }),
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
	LevelSetInfo campaign{"Lives", {setup, arena}};
	GameSession game(engine, *xs, campaign);
	DriveTutorial(engine, [&](f32 dt, i32 dx, i32 dy, bool action)
	              { game.Update(dt, dx, dy, action); }, [&]() -> const auto&
	              { return game.Draws(); }, [&]
	              { return game.GetRemainingTargets(); });
	Require(game.GetRemainingTargets() == 0, "Life setup tutorial did not finish.");
	game.Update(1.51f, 0, 0, false);
	game.SkipBonuses();
	Require(game.Ready() && game.GetScore() > 750, "Life setup did not cross the original score threshold.");
	game.Advance();
	game.Update(1.0f / 60, 0, 0, false);
	Require(game.GetLives() == 4, "Campaign score did not grant the next level's extra life.");
	const auto& gainDraws = game.Draws();
	Require(std::count_if(gainDraws.begin(), gainDraws.end(), [&](const auto& draw)
	                      { return draw.Sprite == icon && draw.Y == 565; }) == 3,
	        "Extra life appeared in the HUD before its animation arrived.");
	Require(std::any_of(gainDraws.begin(), gainDraws.end(), [&](const auto& draw)
	                    { return draw.Sprite == icon && draw.X == 96 && draw.Y < 0; }),
	        "Extra life did not start falling from above the screen.");
	game.Update(0.35f, 0, 0, false);
	if (capture)
		engine.DrawFrame(game.Draws(), "life-gain.bmp");
	game.Update(2.0f, 0, 0, false);
	const auto& landed = game.Draws();
	Require(std::count_if(landed.begin(), landed.end(), [&](const auto& draw)
	                      { return draw.Sprite == icon && draw.Y == 565; }) == 4,
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
	Require(std::count_if(lossDraws.begin(), lossDraws.end(), [&](const auto& draw)
	                      { return draw.Sprite == icon && draw.Y == 565; }) == 3,
	        "Alien life loss did not reduce the stationary HUD icons.");
	if (capture)
		engine.DrawFrame(lossDraws, "life-loss.bmp");
	Log("PASS: zero-score refund animation, campaign extra-life drop/landing, alien logical loss and outgoing icon.");
}

void CheckPlayerItems(MudGE& engine, bool capture = false)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack)
	                             { return pack.Name == "XS"; });
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
	    {132, "symbol_power.bmp", 13, "SfxFirework", "FireworkDuration", 22},
	    {134, "symbol_bubbles_good.bmp", 6, "SfxImpotence", "ImpotentDuration", 9},
	    {135, "symbol_smell.bmp", 14, "SfxSmell", "SmellDuration", 14},
	    {136, "symbol_flowers.bmp", 8, "SfxRose", "FlowerDuration", 14},
	};
	for (const auto& item : items)
	{
		const auto level = FixtureLevel(engine, "pickup-" + std::to_string(item.Code), {{0, 240, 200, 0}, {2, 640, 400, 0}}, item.Code);
		LevelScene scene(engine, *xs, level, 1);
		scene.Update(0.01f, 0, 0, false);
		const auto sounds = scene.TakeSounds();
		Require(std::any_of(sounds.begin(), sounds.end(), [&](const auto& sound)
		                    { return _stricmp(sound.c_str(), engine.GetData().SoundPath(item.Sound).c_str()) == 0; }),
		        "Player pickup omitted its original sound.");
		const auto* effect = engine.LoadSprite(str("Sven2/gfx/") + item.Effect, static_cast<i16>(item.Frames), 1);
		const auto& draws = scene.Draws();
		Require(std::any_of(draws.begin(), draws.end(), [&](const auto& draw)
		                    { return draw.Sprite == effect; }),
		        "Player pickup omitted its visual effect.");
		if (capture && item.Code == 134)
			engine.DrawFrame(draws, "slow-motion.bmp");
		scene.Update(0.1f, 1, 0, false);
		Require(scene.GetPlayerDraw().X >= 240 + item.Distance && scene.GetPlayerDraw().X <= 241 + item.Distance,
		        "Player pickup did not apply the original movement-speed modifier.");
		scene.Update(static_cast<f32>(engine.GetData().NumericParam(item.Duration, 20)) + 1, 0, 0, false);
		const auto& expired = scene.Draws();
		Require(std::none_of(expired.begin(), expired.end(), [&](const auto& draw)
		                     { return draw.Sprite == effect; }),
		        "Player pickup effect outlived its configured duration.");
	}
	const auto gift = FixtureLevel(engine, "gift-pickup", {{0, 240, 200, 0}, {2, 640, 400, 0}}, 133);
	for (u32 seed = 1; seed <= 64; ++seed)
	{
		LevelScene scene(engine, *xs, gift, seed);
		scene.Update(0.01f, 0, 0, false);
		const i32 lives = scene.TakeLivesGained();
		const f32 time = scene.TakeTimeDelta();
		Require((lives == 1 && time == 0) || (lives == 0 && (time == 5 || time == 10 || time == 15 || time == 25)),
		        "Gift pickup awarded a reward outside the original nested random branches.");
		Require(scene.TakeScoreDelta() == 0, "Gift pickup invented a score award.");
		scene.Update(0.1f, 0, 0, false);
		Require(scene.TakeLivesGained() == 0 && scene.TakeTimeDelta() == 0, "Gift was consumed more than once.");
	}
	Log("PASS: all four timed player powerups, speed/effect expiry, and 64 gift reward/consume scenarios.");
}

void CheckAttackReservations(MudGE& engine)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack)
	                             { return pack.Name == "XS"; });
	const auto level = FixtureLevel(engine, "attack-reservation", {{0, 240, 120, 4}, {4, 240, 40, 4}, {2, 640, 400, 0}}, 134);
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
					Require(scene.GetPlayerDraw().X > before.X + 20,
					        "An interrupted step left an impassable player reservation after the attack.");
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
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack)
	                             { return pack.Name == "XS"; });
	auto contains = [](const std::vector<SpriteDraw>& draws, const Sprite* sprite)
	{
		return std::any_of(draws.begin(), draws.end(), [&](const auto& draw)
		                   { return draw.Sprite == sprite; });
	};
	const auto cold = FixtureLevel(engine, "frigida-continuation", {{0, 240, 200, 0}, {2, 240, 160, 0}});
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

	const auto artifact = FixtureLevel(engine, "artifact-transform", {{0, 240, 200, 0}, {2, 640, 400, 0}}, 128);
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
		Require(std::all_of(draws.begin(), draws.end(), [](const auto& draw)
		                    { return draw.Sprite != nullptr; }),
		        "Hidden artifact phase published a null drawing sprite.");
		if (contains(draws, alienIcon))
		{
			Require(hero.X == 240 && hero.Y == 200, "Artifact beam moved Sven's ground position to another tile.");
			Require(std::any_of(draws.begin(), draws.end(), [&](const auto& draw)
			                    { return draw.Sprite == alienIcon && draw.X == 268 && draw.Y == 196; }),
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
	const auto movingArtifact = FixtureLevel(engine, "artifact-walking", {{0, 240, 240, 0}, {2, 640, 400, 0}});
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
	Require(movingLanded && movingPower.GetPlayerDraw().X == 240 && movingPower.GetPlayerDraw().Y == 200,
	        "Artifact collected while walking returned Sven to the wrong committed tile.");

	const auto bone = FixtureLevel(engine, "dig-particles", {{0, 640, 400, 0}, {4, 240, 200, 2}, {9, 280, 200, 0}, {2, 640, 120, 0}});
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

	const auto sleeping = FixtureLevel(engine, "farmer-zzz", {{0, 640, 400, 0}, {5, 240, 200, 0, 6, 0, 10, 20}, {2, 640, 120, 0}});
	LevelScene farmer(engine, *xs, sleeping, 1);
	const auto* zzz = engine.LoadSprite("Sven2/gfx/Guy/anim_guy_gfx_zzz.bmp", 13, 1);
	farmer.Update(0.1f, 0, 0, false);
	Require(contains(farmer.Draws(), zzz), "Sleeping farmer omitted the Zzz overlay.");
	if (capture)
		engine.DrawFrame(farmer.Draws(), "farmer-sleeping.bmp");
	const auto sleepingDog = FixtureLevel(engine, "dog-zzz", {{0, 640, 400, 0}, {4, 240, 200, 0, 6, 0, 10, 20}, {2, 640, 120, 0}});
	LevelScene dogSleep(engine, *xs, sleepingDog, 1);
	dogSleep.Update(0.1f, 0, 0, false);
	Require(contains(dogSleep.Draws(), zzz), "Sleeping dog omitted its Zzz overlay.");
	const auto wakingDog = FixtureLevel(engine, "dog-wake", {{0, 240, 200, 0}, {4, 320, 200, 0, 6, 0, 10, 20}, {2, 640, 120, 0}});
	LevelScene dogWake(engine, *xs, wakingDog, 1);
	dogWake.Update(0.1f, 0, 0, false);
	Require(!contains(dogWake.Draws(), zzz), "Dog did not wake when Sven entered its original wake range.");

	const auto girlLevel = FixtureLevel(engine, "girl-combined", {{0, 240, 200, 0}, {6, 240, 120, 4}, {2, 640, 400, 0}});
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
	Require(played && maximumFrame == 34 && time == -20 && girl.TakeLifeHits() == 0,
	        "Girl interaction failed its complete 35-frame front sheet or time-only penalty.");
	Log("PASS: Frigida continuity, artifact ground/icon recovery, dig particles, sleep Zzz and complete girl interaction.");
}

void CheckEnemyHeadings(MudGE& engine, bool capture = false)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack)
	                             { return pack.Name == "XS"; });
	constexpr i32 directions[][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
	constexpr bool mirrors[4][8] = {{0, 1, 1, 1, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 1, 1, 0}, {0, 1, 0, 0, 0, 1, 1, 0}, {0, 1, 1, 1, 0, 0, 0, 0}};
	cstr family[] = {"Alien", "Dog", "Guy", "Broemse"};
	cstr lower[] = {"alien", "dog", "guy", "broemse"};
	cstr headings[] = {"back", "diagonal_back", "side", "diagonal_front", "front", "diagonal_front", "side", "diagonal_back"};
	constexpr i16 rows[] = {12, 16, 16, 8};
	BitmapFont labels(engine, true);
	cstr directionNames[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
	for (i32 kind = 3; kind <= 6; ++kind)
	{
		std::array<const Sprite*, 8> sprites{};
		for (i32 d = 0; d < 8; ++d)
			sprites[d] = engine.LoadSprite(str("Sven2/gfx/") + family[kind - 3] + "/anim_" + lower[kind - 3] + "_walk_" + headings[d] + ".bmp", rows[kind - 3], 1);
		std::set<i32> covered;
		std::vector<SpriteDraw> montage;
		for (i32 facing = 0; facing < 8; ++facing)
			for (u32 seed = 1; seed <= 4; ++seed)
			{
				const auto level = FixtureLevel(engine, "heading-" + std::to_string(kind) + "-" + std::to_string(facing),
				                                {{0, 640, 400, 0}, {kind, 240, 120, facing, 6, 0, 0, 0}, {2, 640, 480, 0}});
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
				const auto after = std::find_if(draws.begin(), draws.end(), [&](const auto& draw)
				                                { return std::find(sprites.begin(), sprites.end(), draw.Sprite) != sprites.end(); });
				Require(after != draws.end(), "Heading fixture left walking before its first short step.");
				const i32 dx = (after->X > before.X) - (after->X < before.X), dy = (after->Y > before.Y) - (after->Y < before.Y);
				i32 direction = 0;
				while (direction < 8 && (dx != directions[direction][0] || dy != directions[direction][1]))
					++direction;
				Require(direction < 8 && after->Sprite == sprites[direction] && after->Mirror == mirrors[kind - 3][direction],
				        "Enemy walking sprite or mirroring disagreed with its actual movement direction.");
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
		Require(covered.size() == 8, "Heading regression did not cover all eight movement directions.");
		if (capture)
			engine.DrawFrame(montage, (str("directions-") + lower[kind - 3] + ".bmp").c_str());
	}
	Log("PASS: all eight authored walk directions and companion mirrors for all four enemy families.");
}

void CheckEnemyItems(MudGE& engine, bool capture = false)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack)
	                             { return pack.Name == "XS"; });
	auto heard = [&](const std::vector<str>& sounds, cstr name)
	{
		const auto path = engine.GetData().SoundPath(name);
		return std::any_of(sounds.begin(), sounds.end(), [&](const auto& sound)
		                   { return _stricmp(sound.c_str(), path.c_str()) == 0; });
	};
	const auto teddy = FixtureLevel(engine, "girl-teddy", {{0, 640, 400, 0}, {6, 240, 200, 2}, {8, 280, 200, 0}, {2, 640, 120, 0}});
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
	Require(std::none_of(recovered.begin(), recovered.end(), [&](const auto& draw)
	                     { return draw.Sprite == teddyFront || draw.Sprite == teddyBack; }),
	        "Girl remained in her teddy interaction after the original sequence completed.");
	const auto playDog = FixtureLevel(engine, "girl-dog", {{0, 640, 400, 0}, {6, 240, 120, 4}, {4, 240, 200, 0}, {2, 640, 480, 0}});
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
				Require(std::none_of(draws.begin(), draws.end(), [&](const auto& draw)
				                     { return draw.Sprite == dogStand; }),
				        "Girl-dog interaction retained a separate standing dog.");
			}
		}
	}
	Require(dogInteraction, "Girl did not enter the dog interaction in the authored encounter.");
	const auto* artifactSprite = engine.LoadSprite("Sven2/gfx/Artifact.bmp", 1, 1);
	for (i32 kind = 3; kind <= 6; ++kind)
	{
		const auto level = FixtureLevel(engine, "artifact-enemy-" + std::to_string(kind),
		                                {{0, 640, 400, 0}, {kind, 240, 200, 2, 6, 0, 0, 0}, {10, 280, 200, 0}, {2, 640, 120, 0}});
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
					Require(std::none_of(draws.begin(), draws.end(), [&](const auto& draw)
					                     { return draw.Sprite == artifactSprite; }),
					        "Enemy artifact pickup left the collectible behind.");
					for (const auto* path : {"Sven2/gfx/ufo.bmp", "Sven2/gfx/ufo_shadow.bmp", "Sven2/gfx/anim_beam.bmp"})
					{
						const auto* image = engine.LoadSprite(path, std::strstr(path, "anim_beam") ? 44 : 13, 1);
						Require(std::any_of(draws.begin(), draws.end(), [&](const auto& draw)
						                    { return draw.Sprite == image; }),
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
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack)
	                             { return pack.Name == "XS"; });
	const auto* back = engine.LoadSprite("Sven2/gfx/Sheep/anim_sheep_demon_back.bmp", 23, 1);
	const auto* front = engine.LoadSprite("Sven2/gfx/Sheep/anim_sheep_demon_front.bmp", 23, 1);
	const auto* run = engine.LoadSprite("Sven2/gfx/Sheep/anim_sheep_demon_run_front.bmp", 8, 1);
	const auto* flash = engine.LoadSprite("Sven2/gfx/flash.bmp", 2, 1);
	const auto* mood = engine.LoadSprite("Sven2/gfx/symbols.bmp", 12, 1);
	const auto* stand = engine.LoadSprite("Sven2/gfx/Sheep/Sheep_Stand.bmp", 4, 1);
	const auto* power = engine.LoadSprite("Sven2/gfx/symbol_power.bmp", 13, 1);
	const auto devil = engine.GetData().SoundPath("SfxDevil");
	const auto electro = engine.GetData().SoundPath("SfxDevilElectro");
	auto contains = [](const auto& draws, const auto* sprite)
	{
		return std::any_of(draws.begin(), draws.end(), [&](const auto& draw)
		                   { return draw.Sprite == sprite; });
	};
	const auto natural = FixtureLevel(engine, "demon-natural", {{0, 80, 240, 0}, {2, 480, 160, 0, 10}, {2, 480, 320, 0, 10}});
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
	const auto chance = FixtureLevel(engine, "demon-chance", {{0, 80, 240, 0}, {2, 480, 160, 0, 10, 0, 0, 0, 40}, {2, 480, 320, 0, 10, 0, 0, 0, 40}});
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
			Require(std::all_of(draws.begin(), draws.end(), [&](const auto& draw)
			                    { return draw.Sprite != mood || draw.Frame == 11; }),
			        "Failed demon attempt did not cap impatience back to 10.");
		}
	}
	Require(succeeded && failed, "Demon chance did not exercise both original random outcomes.");
	const auto protectedLevel = FixtureLevel(engine, "demon-frigida-protected", {{0, 80, 240, 0}, {2, 480, 160, 0, 10, 2, 0, 0, 40}, {2, 480, 320, 0, 0}});
	LevelScene protectedSheep(engine, *xs, protectedLevel, 1);
	protectedSheep.Update(30, 0, 0, false);
	const auto protectedSounds = protectedSheep.TakeSounds();
	Require(std::find(protectedSounds.begin(), protectedSounds.end(), devil) == protectedSounds.end(), "Frigida incorrectly entered the ordinary demon branch.");
	for (bool afterStep : {false, true})
	{
		const auto level = FixtureLevel(engine, afterStep ? "demon-moving-shock" : "demon-direct-shock",
		                                {{0, 240, 200, 0}, {2, afterStep ? 160 : 200, afterStep ? 160 : 200, afterStep ? 1 : 0, 12, 0, 11}, {2, 640, 400, 0}}, 132);
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
	const auto walk = FixtureLevel(engine, "demon-three-steps", {{0, 80, 240, 0}, {2, 480, 160, 2, 12, 0, 11}, {2, 640, 400, 0}});
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
	const auto blocked = FixtureLevel(engine, "demon-blocked", {{0, 80, 240, 0}, {2, 480, 200, 0, 12, 0, 11}, {1, 520, 160, 0}, {1, 520, 240, 0}, {1, 440, 240, 0}, {1, 440, 160, 0}});
	LevelScene trapped(engine, *xs, blocked, 1);
	trapped.Update(4, 0, 0, false);
	const auto& trappedDraws = trapped.Draws();
	Require(std::any_of(trappedDraws.begin(), trappedDraws.end(), [&](const auto& draw)
	                    { return draw.Sprite == stand && draw.X == 480 && draw.Y == 200; }),
	        "Blocked demon failed to recover at its original position.");
	const auto timeout = FixtureLevel(engine, "demon-timeout", {{0, 80, 240, 0}, {2, 480, 200, 0, 12, 0, 11}});
	LevelScene expired(engine, *xs, timeout, 1);
	expired.TimeExpired();
	for (i32 frame = 0; frame < 60 * 120 && !expired.Completed(); ++frame)
		expired.Update(1.0f / 60, 0, 0, false);
	Require(expired.Completed() && expired.GetCompletedSheep() == 0 && expired.TakeLifeHits() == 0,
	        "Timeout UFO failed to collect a sheep after its busy demon sequence.");
	Log("PASS: natural demon trigger, transform/run/recovery, direct and moving shock branches, flash, powerup cancellation and stun release.");
}

void CheckFarmerOccupancy(MudGE& engine)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack)
	                             { return pack.Name == "XS"; });
	constexpr i32 directions[8][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
	for (i32 state : {0, 10})
		for (const auto& direction : directions)
		{
			const i32 x = 240 - 40 * direction[0], y = 200 - 40 * direction[1];
			const auto level = FixtureLevel(engine, "farmer-solid-" + std::to_string(state) + "-" + std::to_string(x) + "-" + std::to_string(y),
			                                {{0, x, y, 0}, {5, 240, 200, 0, 6, 0, state, 1000}, {2, 640, 400, 0}});
			LevelScene scene(engine, *xs, level, 1);
			scene.Update(0.3f, direction[0], direction[1], false);
			Require(scene.GetPlayerDraw().X == x && scene.GetPlayerDraw().Y == y, "Player entered the farmer's occupied cell.");
		}
	Log("PASS: correctly centered 160px farmer blocks his logical cell from eight directions, awake and sleeping.");
}

void CheckGirlBeamReturn(MudGE& engine, bool capture = false)
{
	const auto& getPacks = engine.GetData().GetPacks();
	const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack)
	                             { return pack.Name == "XS"; });
	const auto* stand = engine.LoadSprite("Sven2/gfx/Broemse/Broemse_stand.bmp", 8, 1);
	const auto* back = engine.LoadSprite("Sven2/gfx/Broemse/anim_broemse_beam_up_diagonal_back.bmp", 5, 1);
	const auto* front = engine.LoadSprite("Sven2/gfx/Broemse/anim_broemse_beam_up_diagonal_front.bmp", 5, 1);
	const auto fall = engine.GetData().SoundPath("SfxSvenFall");
	for (i32 quadrant = 0; quadrant < 4; ++quadrant)
	{
		const auto level = FixtureLevel(engine, "girl-beam-return-" + std::to_string(quadrant),
		                                {{0, 640, 400, 0}, {6, 240, 200, quadrant * 2, 6, 0, 17}, {2, 640, 120, 0}});
		LevelScene scene(engine, *xs, level, 1);
		const auto* beam = quadrant == 0 || quadrant == 3 ? back : front;
		scene.Update(0.5f, 0, 0, false);
		const auto& rising = scene.Draws();
		Require(std::any_of(rising.begin(), rising.end(), [&](const auto& draw)
		                    { return draw.Sprite == beam && draw.Mirror == (quadrant < 2) && draw.X == 240 && draw.Y == 190; }),
		        "Girl beam-up body used the wrong anchor or mirrored direction.");
		if (capture && quadrant == 0)
			engine.DrawFrame(rising, "girl-beam-up.bmp");
		scene.Update(1.0f, 0, 0, false);
		const auto& hidden = scene.Draws();
		Require(std::none_of(hidden.begin(), hidden.end(), [&](const auto& draw)
		                     { return draw.Sprite == stand || draw.Sprite == back || draw.Sprite == front; }),
		        "Girl remained visible during the artifact's hidden wait.");
		scene.Update(4.5f, 0, 0, false);
		const auto& falling = scene.Draws();
		Require(std::any_of(falling.begin(), falling.end(), [&](const auto& draw)
		                    { return draw.Sprite == stand && draw.X == 240 && draw.Y < 200; }),
		        "Girl did not descend using her original standing body.");
		if (capture && quadrant == 0)
			engine.DrawFrame(falling, "girl-beam-return.bmp");
		scene.Update(1.5f, 0, 0, false);
		const auto& landed = scene.Draws();
		Require(std::any_of(landed.begin(), landed.end(), [&](const auto& draw)
		                    { return draw.Sprite == stand && draw.X == 240 && draw.Y == 200; }),
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
			const auto episode = std::find_if(pack.LevelSets.begin(), pack.LevelSets.end(), [](const auto& set)
			                                  { return set.Name == "Items and teleports"; });
			Require(episode != pack.LevelSets.end(), "ParityLab items/teleports episode is missing.");
			const auto& level = episode->Levels.front();
			LevelScene puddles(engine, pack, level, 1);
			for (i32 cell = 0; cell < 3; ++cell)
				Step(puddles, 1, 0);
			Step(puddles, 0, 1);
			Step(puddles, 0, 1);
			Require(puddles.TakeSplashSound() && puddles.GetPlayerDraw().X == 560 && puddles.GetPlayerDraw().Y == 280,
			        "Authored lab puddle did not teleport Sven to the other puddle's north exit.");
			LevelScene water(engine, pack, level, 1);
			for (i32 cell = 0; cell < 7; ++cell)
				Step(water, 1, 0);
			for (i32 cell = 0; cell < 5; ++cell)
				Step(water, 0, 1);
			Require(water.TakeSplashSound() && (water.GetPlayerDraw().X == 200 || water.GetPlayerDraw().X == 560) && water.GetPlayerDraw().Y == 280,
			        "Authored lab water did not send Sven to a valid puddle exit.");
			LevelScene artifact(engine, pack, level, 1);
			for (i32 cell = 0; cell < 12; ++cell)
				Step(artifact, 1, 0);
			for (i32 cell = 0; cell < 3; ++cell)
				Step(artifact, 0, -1);
			const auto* sprite = engine.LoadSprite("Sven2/gfx/Artifact.bmp", 1, 1);
			const auto& consumed = artifact.Draws();
			Require(std::none_of(consumed.begin(), consumed.end(), [&](const auto& draw)
			                     { return draw.Sprite == sprite; }),
			        "Authored lab artifact failed to disappear when Sven entered its cell.");
			const auto* icon = engine.LoadSprite("Sven2/gfx/icon_alien.bmp", 13, 1);
			const auto* ship = engine.LoadSprite("Sven2/gfx/ufo.bmp", 13, 1);
			const auto* shadow = engine.LoadSprite("Sven2/gfx/ufo_shadow.bmp", 13, 1);
			bool powered = false, visibleUfo = false;
			for (i32 frame = 0; frame < 3600 && !powered; ++frame)
			{
				artifact.Update(1.0f / 60, 0, 0, false);
				const auto& draws = artifact.Draws();
				visibleUfo = visibleUfo || (std::any_of(draws.begin(), draws.end(), [&](const auto& draw)
				                                        { return draw.Sprite == ship; }) &&
				                            std::any_of(draws.begin(), draws.end(), [&](const auto& draw)
				                                        { return draw.Sprite == shadow; }));
				powered = std::any_of(draws.begin(), draws.end(), [&](const auto& draw)
				                      { return draw.Sprite == icon; });
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
	Require(scene.TakeScoreDelta() == 0 && scene.TakeTimeDelta() == 0 && scene.TakeLifeHits() == 0,
	        "Timeout collection incorrectly awarded interaction rewards or took a life.");
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
		for (const auto& set : pack.LevelSets)
			for (const auto& level : set.Levels)
			{
				for (u32 seed : {1u, 73u})
				{
					LevelScene scene(engine, pack, level, seed);
					output << level.Map << ' ' << seed << '\n';
					for (i32 frame = 0; frame < 480; ++frame)
					{
						u64 hash = 14695981039346656037ull;
						auto mix = [&](const auto& value)
						{
							const auto* bytes = reinterpret_cast<const u8*>(&value);
							for (size_t i = 0; i < sizeof(value); ++i)
								hash = (hash ^ bytes[i]) * 1099511628211ull;
						};
						constexpr i32 direction[8][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
						constexpr f32 delta[] = {1.0f / 60, 0.03f, 0.1f, 0.25f};
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
									for (i32 byte = 0; byte < 4; ++byte)
										pixels = (pixels ^ ((pixel >> (8 * byte)) & 255)) * 1099511628211ull;
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
								str core = FindCharacterDefinition(Kinds::Player)->SpriteAsset;
								sound = core.substr(0, core.find('/')) + sound.substr(sound.find('/'));
							}
							for (char byte : sound)
								mix(byte);
						}
						output << frame << ' ' << hash << '\n';
					}
				}
			}
	Require(static_cast<bool>(output), "Cannot finish behavior trace.");
	Log("PASS: deterministic behavior trace written.");
}

void CheckObjectOwnership(MudGE& engine)
{
	const LevelInfo level = FixtureLevel(engine, "object-ownership", {{0, 80, 80, 0}, {2, 240, 240, 0}, {3, 360, 120, 0}, {4, 440, 240, 0}, {5, 520, 360, 0}, {6, 600, 200, 0}, {11, 120, 400, 0}, {12, 240, 400, 0}, {13, 360, 400, 0}});
	const LevelPackInfo& pack = *std::find_if(engine.GetData().GetPacks().begin(), engine.GetData().GetPacks().end(), [](const LevelPackInfo& value)
	                                          { return value.Name == "XS"; });
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
	effect.Kind = Kinds::CompletionEffect;
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
		const auto trace = argc == 3 && str(argv[1]) == "--trace" ? std::filesystem::absolute(std::filesystem::u8path(argv[2])) : fpath{};
		const auto catalog = argc == 3 && (str(argv[1]) == "--catalog" || preview || catalogAudio) ? std::filesystem::absolute(std::filesystem::u8path(argv[2])) : fpath{};
		wchar_t executable[32768]{};
		GetModuleFileNameW(nullptr, executable, 32768);
		const auto getRoot = fpath(executable).parent_path().parent_path().parent_path().parent_path().parent_path();
		const auto saves = getRoot / L"rebuild" / L"runtime" / L"checks" /
		                   (std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
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
			LinkFixtureData(getRoot / L"assets", fixtureData);
		MudGE engine(getRoot / L"games" / L"sven2", catalog.empty() ? fixtureData : catalog);
		engine.Construct();
		engine.InitializePlugins();
		engine.VerifyData();
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
					for (const auto& pack : engine.GetData().GetPacks())
						for (const auto& set : pack.LevelSets)
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
					for (const auto& set : pack.LevelSets)
						for (const auto& level : set.Levels)
						{
							LevelScene scene(engine, pack, level, 1);
							engine.DrawFrame(scene.Draws(), ("pack-level-" + std::to_string(++index) + ".bmp").c_str());
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
			exitClick.Mouse = {650, 555};
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
			collectionTab.Mouse = {510, 40};
			ui.Handle(collectionTab);
			engine.DrawFrame(ui.Draws(0), "cards-first.bmp");
			WindowInput::Result previousCard;
			previousCard.Left = true;
			ui.Handle(previousCard);
			engine.DrawFrame(ui.Draws(0), "cards-last.bmp");
			CheckCapturePixel("cards-last.bmp", 510, 65, paper->Pixels[65 * paper->Width + 510]);
			ui.ShowResults({1250, 1, 180, 3, 3, 0, 0, 0, false, false});
			engine.DrawFrame(ui.Draws(0), "results.bmp");
			ui.ShowResults({1250, 1, 180, 3, 3, 0, 0, 0, true, false, 17, 3});
			engine.DrawFrame(ui.Draws(0.1f), "pack-complete.bmp");
			WindowInput::Result continueResult;
			continueResult.Confirm = true;
			ui.Handle(continueResult);
			WindowInput::Result enterName;
			enterName.Text = "Tester";
			ui.Handle(enterName);
			engine.DrawFrame(ui.Draws(0), "name-entry.bmp");
			const auto& getPacks = engine.GetData().GetPacks();
			const auto xs = std::find_if(getPacks.begin(), getPacks.end(), [](const auto& pack)
			                             { return pack.Name == "XS"; });
			GameSession session(engine, *xs, xs->LevelSets.front());
			engine.DrawFrame(session.Draws(), "level.bmp");
			ui.Show(Page::Pause);
			engine.DrawFrame(ui.Draws(0, &session.Draws()), "pause.bmp");
			ui.Show(Page::Title);
			ui.Handle(exitClick);
			engine.DrawFrame(ui.Draws(0, &session.Draws()), "menu-exit-after-level.bmp");
			const auto walking = FixtureLevel(engine, "visual-walk", {{0, 240, 200, 0}, {2, 640, 400, 0}});
			LevelScene east(engine, *xs, walking, 1), west(engine, *xs, walking, 1);
			east.Update(0.1f, 1, 0, false);
			west.Update(0.1f, -1, 0, false);
			engine.DrawFrame(east.Draws(), "running-east.bmp");
			engine.DrawFrame(west.Draws(), "running-west.bmp");
			const auto cold = FixtureLevel(engine, "visual-frigida", {{0, 240, 200, 0}, {2, 240, 160, 0}});
			LevelScene frigida(engine, *xs, cold, 1);
			for (i32 frame = 0; frame < 90; ++frame)
				frigida.Update(1.0f / 60, 0, 0, frame % 8 == 0);
			engine.DrawFrame(frigida.Draws(), "frigida.bmp");
			const auto enemy = FixtureLevel(engine, "visual-fight", {{0, 240, 200, 0}, {4, 280, 200, 6}, {2, 640, 400, 0}});
			LevelScene fight(engine, *xs, enemy, 1);
			const auto* cloud = engine.LoadSprite("Sven2/gfx/anim_cloud.bmp", 10, 1);
			for (i32 frame = 0; frame < 600; ++frame)
			{
				fight.Update(1.0f / 60, 0, 0, false);
				const auto& draws = fight.Draws();
				if (std::any_of(draws.begin(), draws.end(), [&](const auto& draw)
				                { return draw.Sprite == cloud; }))
				{
					engine.DrawFrame(draws, "fight.bmp");
					break;
				}
			}
			const auto animals = FixtureLevel(engine, "visual-meters", {{0, 240, 200, 0}, {12, 240, 160, 0}, {11, 400, 160, 0}, {13, 560, 160, 0}});
			LevelScene meters(engine, *xs, animals, 1);
			engine.DrawFrame(meters.Draws(), "animal-meters.bmp");
			meters.Update(0.2f, 0, 0, true);
			engine.DrawFrame(meters.Draws(), "goat-interaction.bmp");
			Require(engine.StartAudio(), "Native regression audio backend failed to initialize.");
			for (cstr name : {"SfxDogFoundSven", "SfxGuyFoundBeer", "SfxLarsDrink", "SfxLarsSleep"})
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
