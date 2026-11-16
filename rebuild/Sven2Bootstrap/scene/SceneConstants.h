#pragma once
#include "Types.h"
#include <map>

// Values retain the original table addresses for research cross-references.
enum class AnimationSequence : u32
{
	UfoBeam = 0x440398,
	AlienStandingIdle = 0x43D210,
	AlienVerticalIdle = 0x43D264,
	AlienDiagonalIdle = 0x43D2B8,
	AlienProbe = 0x43D30C,
	GirlWalk = 0x43DA3C,
	GirlTeddyBack = 0x43DADC,
	GirlTeddyFront = 0x43DCB0,
	GirlSvenBack = 0x43DE88,
	GirlSvenFront = 0x43E09C,
	GirlDogBack = 0x43E388,
	GirlDogFront = 0x43E4B8,
	FarmerDrink = 0x43F1D4,
	FarmerLieDown = 0x43F358,
	FarmerStandUp = 0x43F430,
	FarmerGrumbleBack = 0x43F4B0,
	FarmerGrumbleFront = 0x43F564,
	ChickenInteraction = 0x440488,
	GoatInteractionBack = 0x4404E4,
	GoatInteractionFront = 0x440550,
	PigInteractionBack = 0x4405BC,
	PigInteractionFront = 0x44061C,
	DemonTransform = 0x43FD3C,
	DemonRecover = 0x43FE2C,
	DemonShock = 0x43FEC8,
	ElectricFlash = 0x440048,
	PlayerElectricShock = 0x4401D8
};

constexpr bool SvenMirror[8] = {false, false, true, true, false, false, false, true};
constexpr bool EnemyWalkMirror[4][8] = {
    {false, true, true, true, false, false, false, false},
    {false, true, false, false, false, true, true, false},
    {false, true, false, false, false, true, true, false},
    {false, true, true, true, false, false, false, false}};
constexpr bool SheepMirror[4] = {false, true, false, true};

// Original enemy and small-animal frame tables; vector length represents the -1 terminator.
extern const std::map<AnimationSequence, std::vector<i32>> ActorFrameTables;
// Source Steering tables: 39 weighted choices per preferred direction.
extern const i32 Steering[4][8][39];
constexpr i32 Stride = 22, Columns = 20, Rows = 15, TileSize = 40;
constexpr i32 Empty = 0, Wall = 1, Puddle = 10, Water = 11;
constexpr f32 PlayerCenter = 60;

// 0x406140/0x406220 choose north when source and target share a cell.
inline i32 EnemyDirection(i32 source, i32 target)
{
	const i32 dx = target % Stride - source % Stride;
	const i32 dy = target / Stride - source / Stride;
	if (dx == 0)
		return dy > 0 ? 4 : 0;
	if (dx > 0)
		return dy < 0 ? 1 : dy > 0 ? 3
		                           : 2;
	return dy < 0 ? 7 : dy > 0 ? 5
	                           : 6;
}

constexpr i32 SplashFrames = 14;
constexpr f32 SplashFrameRate = 15; // kind 33 constructor, 0x40C18C
constexpr i32 Directions[8][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};

// Original 0x43FC54 (turn), 0x43FB6C (chew), 0x440300/0x440268 (whistle).
constexpr i32 SplashPlayerFrames[] = {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1};
constexpr i32 FrigidaFrames[] = {0, 0, 1, 2, 2, 0, 0};
constexpr i32 TurnFrames[] = {0, 0, 0, 0, 1, 2, 2, 2, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 5, 6, 7, 8, 7, 6, 5, 4, 5, 6, 7, 8, 7, 6, 5, 4, 4, 9, 10, 0, 0, 0, 0};
constexpr i32 ChewFrames[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
constexpr i32 WhistleStraight[] = {0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
constexpr i32 WhistleDiagonal[] = {0, 0, 0, 0, 1, 2, 3, 4, 4, 5, 6, 7, 8, 9, 9, 10, 11, 12, 13, 13, 14, 15};
constexpr u8 TileDirection[8] = {2, 3, 1, 4, 2, 3, 1, 4};
