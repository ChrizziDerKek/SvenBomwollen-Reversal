#pragma once
#include "Types.h"
#include <array>

class FsrPresenter;
enum class ScalingFilter;

// Only trivial ABI containers cross the VC6/modern-MSVC boundary.
// Engine constructors allocate strings; consuming-by-value methods destroy them.
struct LegacyString
{
	u32 Words[4]{};
};
static_assert(sizeof(LegacyString) == 16);
static_assert(sizeof(any) == 4, "Original Sven2 DLLs require Win32/x86");

struct Sprite
{
	std::vector<u32> Pixels;
	i32 Width;
	i32 FrameWidth;
	i32 FrameHeight;
	i32 Rows;
	i32 Columns;
	bool Keyed;
};

struct SpriteDraw
{
	const Sprite* Sprite;
	i32 X;
	i32 Y;
	i32 Frame;
	bool Mirror = false;
	i32 ClipBottom = 0; // Exclusive screen Y; zero uses the full drawing surface.
};

struct LevelInfo
{
	str Name;
	str Map;
	str TileSet;
	str Music;
};

struct LevelSetInfo
{
	str Name;
	std::vector<LevelInfo> Levels;
};

struct TileInfo
{
	str File;
	u16 Frames = 1;
};

struct TileSetInfo
{
	str Name;
	str Background;
	std::vector<TileInfo> Tiles;
};

struct LevelPackInfo
{
	str Name;
	str Descriptor;
	std::vector<LevelSetInfo> LevelSets;
	std::vector<TileSetInfo> TileSets;
};

struct RoundResult
{
	i32 Score = 0;
	i32 Level = 1;
	i32 Time = 0;
	i32 Sheep = 0;
	i32 Lives = 0;
	i32 TimeRemaining = 0;
	i32 SheepRemaining = 0;
	i32 LivesRemaining = 0;
	bool EpisodeComplete = false;
	bool GameOver = false;
	i32 TotalSheep = 0;
	i32 ScoreFrame = 0;
};

enum class AnimationSequence : u32;

struct WorldObject
{
	SpriteDraw Draw{};
	f64 DepthOffset = 0;
	i32 Kind = 1;
	i32 Tile = -1;
	i32 Facing = 0;
	i32 Impatience = 6;
	i32 Temperament = 0;
	i32 State = 0;
	i32 TurnTo = -1;
	i32 AngerTicks = 0;
	i32 Attempts = 0;
	f32 CommittedY = 0;
	f32 Age = 0;
	f32 Idle = 0;
	f32 Pulse = 30;
	f32 X = 0;
	f32 Y = 0;
	f32 TargetX = 0;
	f32 TargetY = 0;
	bool Freed = false;
	i32 MoodBlink = 0;
	i32 IdleVariant = 0;
	bool IdleAnimating = false;
	i32 ItemTarget = -1;
	i32 Steps = 0;
	i32 Permanent = 0;
	f32 SoundClock = 0;
	f32 CenterX = 60;
	f32 CenterY = 60;
	f32 AnimRate = 20;
	f32 Lift = 0;
	bool Hunting = false;
	bool Retry = false;
	std::array<u32, 25> Original{};
};

struct TimePopup
{
	f32 X;
	f32 Y;
	f32 Speed;
	f32 Life;
	i32 Amount;
};

struct ActorSprites
{
	const Sprite* Stand = nullptr;
	std::array<const Sprite*, 8> Walk{};
	std::array<const Sprite*, 8> Idle{};
	std::array<const Sprite*, 4> Sleep{};
	std::array<const Sprite*, 4> Wake{};
	std::array<const Sprite*, 4> Beam{};
	std::array<const Sprite*, 4> Distraction{};
	std::array<const Sprite*, 4> Battle{};
	std::array<const Sprite*, 4> Grumble{};
	std::array<const Sprite*, 4> DogPlay{};
};

struct EnemyRules
{
	f32 Speed = 0;
	f32 Run = 1;
	f32 Diagonal = 0.75f;
	f32 WalkRate = 15;
	f32 RunRate = 30;
	f32 Idle = 4;
	f32 IdleRandom = 1;
	f32 Sleep = 15;
	f32 WakeRange = 2;
	f32 Range = 3;
	f32 FightTime = 1.5f;
	i32 HuntSteps = 20;
	i32 WalkSteps = 5;
	i32 WalkRandom = 2;
};

struct UfoFlight
{
	i32 State = -1;
	i32 Target = -1;
	i32 TargetTile = -1;
	f32 X = 0;
	f32 Y = 0;
	f32 TargetX = 0;
	f32 TargetY = 0;
	f32 VX = 0;
	f32 VY = 0;
	f32 Radius = 900;
	f32 AngleX = 0;
	f32 AngleY = 0;
	f32 AngularSpeed = 0.01f;
	f32 Clock = 0;
	f32 Age = 0;
	f32 BeamAge = 0;
	bool CollectAnimals = false;
	bool Beam = false;
};
