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
	i32 GetInteractionMultiplier() const;
	f32 GetEnemyMotionScale() const;
	f32 GetSharedRocketRemaining() const;
	f32 GetSharedSlowRemaining() const;
	void ResetGiftChain();

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
	i32 GiftIndex = 0;
	f32 SharedRocketRemaining = 0;
	f32 SharedSlowRemaining = 0;
	bool SharedRocket = false;
	bool SharedSlow = false;
	static constexpr f32 DoubleRocketDuration = 10; // XXX 0x41A216/0x41A21C
	static constexpr f32 DoubleSlowDuration = 30;   // XXX 0x41A5E9
	static constexpr f32 SlowMotionScale = 0.5f;    // XXX 0x41A5F3
	void ApplyGlobalMood(i32 mood);
};
