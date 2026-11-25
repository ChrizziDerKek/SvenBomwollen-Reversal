#pragma once
#include "Types.h"

enum class WeatherMode
{
	None,
	Snow,
	Rain,
	Bubbles
};

// Native cell codes are a separate domain from serialized ObjectKind IDs.
enum class OccupancyCode : i32
{
	Empty = 0,
	Wall = 1,
	Sheep = 2,
	Alien = 3,
	Dog = 4,
	Farmer = 5,
	Girl = 6,
	Chicken = 7,
	Goat = 8,
	Pig = 9,
	Puddle = 10,
	Water = 11,
	Penguin = 12,
	Turtle = 13,
	Rabbit = 14,
	Mermaid = 15,
	PlayerReservation = 99,
	Player = 100
};

inline bool IsEnemyOccupancy(OccupancyCode code)
{
	return code == OccupancyCode::Alien || code == OccupancyCode::Dog || code == OccupancyCode::Farmer || code == OccupancyCode::Girl;
}

enum class ItemCode : u8
{
	None = 0,
	Artifact = 128,
	Bone = 129,
	Beer = 130,
	Teddy = 131,
	Rocket = 132,
	Gift = 133,
	SlowMushroom = 134,
	StinkMushroom = 135,
	Flower = 136,
	AlienPower = 137
};

inline bool IsPlayerPowerup(ItemCode code)
{
	return code == ItemCode::Rocket || code == ItemCode::Gift || code == ItemCode::SlowMushroom || code == ItemCode::StinkMushroom || code == ItemCode::Flower;
}

enum class SpawnerCode : i32
{
	None = 0,
	Distraction = 140,
	Powerup = 141,
	Mixed = 142
};

inline bool IsSpawner(SpawnerCode code)
{
	return code == SpawnerCode::Distraction || code == SpawnerCode::Powerup || code == SpawnerCode::Mixed;
}
