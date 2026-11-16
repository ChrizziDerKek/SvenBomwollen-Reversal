#pragma once
#include "Types.h"
#include "Engine/Structures.h"

class LevelScene;
class LevelEffects
{
	using Object = WorldObject;

public:
	explicit LevelEffects(LevelScene& scene);
	LevelEffects(const LevelEffects&) = delete;
	LevelEffects& operator=(const LevelEffects&) = delete;
	void AddTime(i32 amount, f32 x, f32 y);
	void SpawnCompletionEffect(f32 x, f32 y, bool hearts);
	void UpdateEffects(f32 seconds);
	void UpdateWorldEffect(size_t index, f32 seconds);
	void SpawnDigEffect(const Object& actor);
	bool TakeSplashSound();

private:
	friend class AnimalCharacter;
	friend class EnemyCharacter;
	friend class LevelItems;
	friend class LevelScene;
	friend class ObjectManager;
	friend class PlayerCharacter;
	friend class SceneRenderer;
	LevelScene& Scene;
	std::vector<TimePopup> TimePopups;
	std::vector<Object> SceneEffects;
	std::array<SpriteDraw, 2> Splashes{};
	f32 SplashAge = 100, SplashPlayerAge = 100;
	bool SplashSound = false;
	f32 TimeDelta = 0;
};
