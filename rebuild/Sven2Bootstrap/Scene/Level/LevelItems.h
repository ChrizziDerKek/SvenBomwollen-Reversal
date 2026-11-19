#pragma once
#include "Types.h"
#include "Engine/Structures.h"

class LevelScene;
class LevelItems
{
	using Object = WorldObject;

public:
	explicit LevelItems(LevelScene& scene);
	LevelItems(const LevelItems&) = delete;
	LevelItems& operator=(const LevelItems&) = delete;
	i32 FindDistraction(const Object& actor, ItemCode code) const;
	void RemoveItem(i32 tile);
	void SpawnItem(ObjectKind kind, i32 tile);
	void SpawnItems();
	void UpdateItems(f32 seconds);
	void UpdateObject(size_t i, f32 seconds);
	void AffectNearbyAnimals();
	void PickupPlayerItem();
	PlayerPowerup& GetPowerup();

private:
	friend class AnimalCharacter;
	friend class EnemyCharacter;
	friend class LevelScene;
	friend class PlayerCharacter;
	friend class SceneRenderer;
	friend class UfoController;
	LevelScene& Scene;
	f32 SpawnClock = 0, SpawnPeriod = 10;
	i32 LivesGained = 0;
};
