#pragma once
#include "Types.h"

// File-format IDs are kept at the serialization boundary, not invented at runtime.
enum class ObjectKind : i32
{
	Inactive = -1,
	Player = 0,
	Scenery = 1,
	Sheep = 2,
	Alien = 3,
	Dog = 4,
	Farmer = 5,
	Girl = 6,
	Beer = 7,
	Teddy = 8,
	Bone = 9,
	Artifact = 10,
	Chicken = 11,
	Goat = 12,
	Pig = 13,
	Rocket = 14,
	Gift = 15,
	SlowMushroom = 16,
	StinkMushroom = 17,
	Flower = 18,
	CompletionEffect = 32,
	ElectricFlash = 34,
	Particle = 37
};

enum class ObjectPhase
{
	Effects,
	Items,
	Enemies,
	Animals,
	Sheep,
	Player
};

namespace Kinds
{
inline constexpr i32 Player = static_cast<i32>(ObjectKind::Player);
inline constexpr i32 Scenery = static_cast<i32>(ObjectKind::Scenery);
inline constexpr i32 Sheep = static_cast<i32>(ObjectKind::Sheep);
inline constexpr i32 Alien = static_cast<i32>(ObjectKind::Alien);
inline constexpr i32 Dog = static_cast<i32>(ObjectKind::Dog);
inline constexpr i32 Farmer = static_cast<i32>(ObjectKind::Farmer);
inline constexpr i32 Girl = static_cast<i32>(ObjectKind::Girl);
inline constexpr i32 Beer = static_cast<i32>(ObjectKind::Beer);
inline constexpr i32 Teddy = static_cast<i32>(ObjectKind::Teddy);
inline constexpr i32 Bone = static_cast<i32>(ObjectKind::Bone);
inline constexpr i32 Artifact = static_cast<i32>(ObjectKind::Artifact);
inline constexpr i32 Chicken = static_cast<i32>(ObjectKind::Chicken);
inline constexpr i32 Goat = static_cast<i32>(ObjectKind::Goat);
inline constexpr i32 Pig = static_cast<i32>(ObjectKind::Pig);
inline constexpr i32 Rocket = static_cast<i32>(ObjectKind::Rocket);
inline constexpr i32 Gift = static_cast<i32>(ObjectKind::Gift);
inline constexpr i32 SlowMushroom = static_cast<i32>(ObjectKind::SlowMushroom);
inline constexpr i32 StinkMushroom = static_cast<i32>(ObjectKind::StinkMushroom);
inline constexpr i32 Flower = static_cast<i32>(ObjectKind::Flower);
inline constexpr i32 CompletionEffect = static_cast<i32>(ObjectKind::CompletionEffect);
inline constexpr i32 ElectricFlash = static_cast<i32>(ObjectKind::ElectricFlash);
inline constexpr i32 Particle = static_cast<i32>(ObjectKind::Particle);
} // namespace Kinds
