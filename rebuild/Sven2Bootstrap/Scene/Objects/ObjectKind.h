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
	Penguin = 25,
	Turtle = 26,
	Rabbit = 27,
	Mermaid = 28,
	CompletionEffect = 32,
	ElectricFlash = 34,
	Beam = 36,
	Particle = 37,
	BubbleEmitter = 43,
	YellowEmitter = 44,
	RocketBurst = 219,
	SlowBubble = 221,
	SlowNotice = 222,
	SmellBurst = 223,
	FlowerBurst = 224,
	BubbleParticle = 543,
	YellowParticle = 544
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
