#include "Types.h"
#include "Scene/ShagAnimation.h"
#pragma once
#include "Engine/Structures.h"
#include <map>

class LevelScene;
class SceneResources
{
	using Object = WorldObject;

public:
	explicit SceneResources(LevelScene& scene);
	SceneResources(const SceneResources&) = delete;
	SceneResources& operator=(const SceneResources&) = delete;
	const Sprite* GetSprite(const str& path, i32 frames);
	void InitializeActors();
	void InitializePlayer();

private:
	friend class AnimalCharacter;
	friend class EnemyCharacter;
	friend class LevelEffects;
	friend class LevelItems;
	friend class LevelScene;
	friend class PlayerCharacter;
	friend class SceneRenderer;
	LevelScene& Scene;
	const Sprite *Explosion = nullptr, *Hearts = nullptr;
	const Sprite *Digging = nullptr, *SleepSymbols = nullptr;
	std::array<const Sprite*, 4> PlayerBeamSprites{};
	std::map<i32, ActorSprites> ActorSprites;
	std::map<i32, EnemyRules> EnemyRules;
	std::map<i32, std::array<ShagAnimation, 4>> AnimalActions;
	std::map<str, const Sprite*> SpriteCache;
	std::map<i32, const Sprite*> ItemSprites;
	std::array<const Sprite*, 5> EffectSprites{};
	const Sprite* UfoSprite = nullptr;
	const Sprite* UfoShadow = nullptr;
	const Sprite* BeamSprite = nullptr;
	const Sprite* MoodSprite = nullptr;
	const Sprite* KettleSprite = nullptr;
	const Sprite* Stand = nullptr;
	const Sprite* SheepStand = nullptr;
	const Sprite* Chewing = nullptr;
	std::array<const Sprite*, 4> DemonSheep{}, DemonWalks{};
	std::array<const Sprite*, 8> Electric{};
	const Sprite* Flash = nullptr;
	std::array<const Sprite*, 4> Turns{};
	std::array<const Sprite*, 8> Whistles{};
	std::array<const Sprite*, 8> Walks{};
	std::array<const Sprite*, 4> SheepWalks{};
	std::vector<ShagAnimation> Interactions;
	std::array<const Sprite*, 8> Blinks{};
	std::array<const Sprite*, 4> Kicks{}, Frigida{};
};
