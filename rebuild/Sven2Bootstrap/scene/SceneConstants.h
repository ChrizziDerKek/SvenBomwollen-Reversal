#pragma once
#include "Types.h"
#include <map>

// Values retain the original table addresses for research cross-references.
enum class AnimationSequence : u32
{
    ufoBeam = 0x440398,
    alienStandingIdle = 0x43D210,
    alienVerticalIdle = 0x43D264,
    alienDiagonalIdle = 0x43D2B8,
    alienProbe = 0x43D30C,
    girlWalk = 0x43DA3C,
    girlTeddyBack = 0x43DADC,
    girlTeddyFront = 0x43DCB0,
    girlSvenBack = 0x43DE88,
    girlSvenFront = 0x43E09C,
    girlDogBack = 0x43E388,
    girlDogFront = 0x43E4B8,
    farmerDrink = 0x43F1D4,
    farmerLieDown = 0x43F358,
    farmerStandUp = 0x43F430,
    farmerGrumbleBack = 0x43F4B0,
    farmerGrumbleFront = 0x43F564,
    chickenInteraction = 0x440488,
    goatInteractionBack = 0x4404E4,
    goatInteractionFront = 0x440550,
    pigInteractionBack = 0x4405BC,
    pigInteractionFront = 0x44061C,
    demonTransform = 0x43FD3C,
    demonRecover = 0x43FE2C,
    demonShock = 0x43FEC8,
    electricFlash = 0x440048,
    playerElectricShock = 0x4401D8
};

constexpr bool svenMirror[8] = { false,false,true,true,false,false,false,true };
constexpr bool enemyWalkMirror[4][8] = {
    {false,true,true,true,false,false,false,false},
    {false,true,false,false,false,true,true,false},
    {false,true,false,false,false,true,true,false},
    {false,true,true,true,false,false,false,false}
};
constexpr bool sheepMirror[4] = { false,true,false,true };

// Original enemy and small-animal frame tables; vector length represents the -1 terminator.
extern const std::map<AnimationSequence, std::vector<i32>> actorFrameTables;
// Source steering tables: 39 weighted choices per preferred direction.
extern const i32 steering[4][8][39];
constexpr i32 stride = 22, columns = 20, rows = 15, tileSize = 40;
constexpr i32 empty = 0, wall = 1, puddle = 10, water = 11;
constexpr f32 playerCenter = 60;

// 0x406140/0x406220 choose north when source and target share a cell.
inline i32 EnemyDirection(i32 source, i32 target)
{
    const i32 dx = target % stride - source % stride;
    const i32 dy = target / stride - source / stride;
    if (dx == 0) return dy > 0 ? 4 : 0;
    if (dx > 0) return dy < 0 ? 1 : dy>0 ? 3 : 2;
    return dy < 0 ? 7 : dy>0 ? 5 : 6;
}

constexpr i32 splashFrames = 14;
constexpr f32 splashFrameRate = 15; // kind 33 constructor, 0x40C18C
constexpr i32 directions[8][2] = { {0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1} };

// Original 0x43FC54 (turn), 0x43FB6C (chew), 0x440300/0x440268 (whistle).
constexpr i32 splashPlayerFrames[] = { 0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1 };
constexpr i32 frigidaFrames[] = { 0,0,1,2,2,0,0 };
constexpr i32 turnFrames[] = { 0,0,0,0,1,2,2,2,2,3,4,5,6,7,8,7,6,5,4,5,6,7,8,7,6,5,4,5,6,7,8,7,6,5,4,4,9,10,0,0,0,0 };
constexpr i32 chewFrames[] = { 0,1,2,3,4,5,6,7,8,9,10,0,1,2,3,4,5,6,7,8,9,10,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19 };
constexpr i32 whistleStraight[] = { 0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18 };
constexpr i32 whistleDiagonal[] = { 0,0,0,0,1,2,3,4,4,5,6,7,8,9,9,10,11,12,13,13,14,15 };
constexpr u8 tileDirection[8] = { 2,3,1,4,2,3,1,4 };
