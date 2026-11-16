#pragma once
#include "Types.h"
#include "LevelCodes.h"
#include <map>
#include "Engine/Structures.h"

class LevelScene;
class LevelFile;
class LevelGrid
{
	using Object = WorldObject;

public:
	explicit LevelGrid(LevelScene& scene);
	LevelGrid(const LevelGrid&) = delete;
	LevelGrid& operator=(const LevelGrid&) = delete;
	void Load(const LevelFile& file);
	i32 TileAt(f32 x, f32 y) const;
	bool Unoccupied(i32 tile, size_t except) const;

private:
	friend class AnimalCharacter;
	friend class EnemyCharacter;
	friend class LevelItems;
	friend class LevelScene;
	friend class PlayerCharacter;
	LevelScene& Scene;
	std::map<SpawnerCode, std::vector<i32>> Spawners;
	std::array<OccupancyCode, 22 * 17> Tiles{};
	std::array<ItemCode, 22 * 17> ItemCodes{};
	std::array<i32, 22 * 17> ItemObjects{}, Reservations{};
	std::array<u8, 22 * 17> TileDirections{};
	std::vector<i32> Puddles;
};
