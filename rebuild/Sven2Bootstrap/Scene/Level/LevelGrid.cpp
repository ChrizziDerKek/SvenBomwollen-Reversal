#include "Scene/Level/LevelGrid.h"
#include "Scene/LevelScene.h"
#include "Scene/Level/LevelFile.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

LevelGrid::LevelGrid(LevelScene& scene)
    : Scene(scene)
{}

void LevelGrid::Load(const LevelFile& file)
{
	Reservations.fill(-1);
	Spawners.clear();
	for (size_t tile = 0; tile < Tiles.size(); ++tile)
	{
		const LevelCell& cell = file.GetCells()[tile];
		Tiles[tile] = cell.Terrain;
		TileDirections[tile] = cell.Direction;
		ItemCodes[tile] = cell.ItemCode;
		ItemObjects[tile] = cell.ItemObject;
		if (IsSpawner(cell.Spawner))
			Spawners[cell.Spawner].push_back(static_cast<i32>(tile));
	}
	Puddles = file.GetPuddles();
}

i32 LevelGrid::TileAt(f32 x, f32 y) const
{
	x += PlayerCenter;
	y += PlayerCenter;
	if (x < 0 || y < 0 || x >= Columns * TileSize || y >= Rows * TileSize)
		return -1;
	return (static_cast<i32>(y) / TileSize + 1) * Stride + static_cast<i32>(x) / TileSize + 1;
}

bool LevelGrid::Unoccupied(i32 tile, size_t except) const
{
	if (tile < 0 || static_cast<size_t>(tile) >= Tiles.size())
		return false;
	if (Tiles[tile] != OccupancyCode::Empty && Tiles[tile] != OccupancyCode::Player)
		return false;
	if (Reservations[tile] >= 0 && Reservations[tile] != static_cast<i32>(except))
		return false;
	for (size_t i = 0; i < Scene.Objects.Size(); ++i)
	{
		if (i == except || Scene.Objects[i].Freed || (!Scene.IsAnimal(Scene.Objects[i].Kind) && !Scene.IsEnemy(Scene.Objects[i].Kind)))
			continue;
		if (Scene.Objects[i].Tile == tile || TileAt(Scene.Objects[i].TargetX, Scene.Objects[i].TargetY) == tile)
			return false;
	}
	for (size_t number = 0; number < Scene.GetPlayerCount(); ++number)
	{
		const PlayerCharacter& player = Scene.Objects.GetPlayer(number);
		if (except != player.GetIndex() && (TileAt(player.X, player.Y) == tile || TileAt(player.TargetX, player.TargetY) == tile))
			return false;
	}
	return true;
}
