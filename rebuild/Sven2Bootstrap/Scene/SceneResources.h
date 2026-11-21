#include "Types.h"
#include "Scene/ShagAnimation.h"
#include "Scene/Characters/PlayerAction.h"
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
	const Sprite* NamedSprite(const str& name);
	const std::vector<i32>& AnimationFrames(AnimationSequence sequence);
	const std::vector<i32>& FrameList(const str& name);
	const std::vector<i32>& PlayerFrames(PlayerAnimation animation);
	void InitializeActors();
	void InitializePlayer();

private:
	friend class AnimalCharacter;
	friend class EnemyCharacter;
	friend class LevelEffects;
	friend class LevelItems;
	friend class PowerupEffect;
	friend class LevelScene;
	friend class PlayerCharacter;
	friend class SceneRenderer;
	LevelScene& Scene;
	std::map<str, std::vector<i32>> FrameLists;
	std::map<ObjectKind, std::array<ShagAnimation, 4>> AnimalEating, AnimalIdle;
	std::map<ObjectKind, ShagAnimation> AnimalZero;
	const Sprite* MermaidJump = nullptr;
	ShagAnimation MermaidAction;
	const Sprite *Explosion = nullptr, *Hearts = nullptr;
	const Sprite *Digging = nullptr, *SleepSymbols = nullptr;
	std::array<const Sprite*, 4> PlayerBeamSprites{};
	std::map<ObjectKind, ActorSprites> ActorSprites;
	std::map<ObjectKind, EnemyRules> EnemyRules;
	std::map<ObjectKind, std::array<ShagAnimation, 4>> AnimalActions;
	std::map<str, const Sprite*> SpriteCache;
	std::map<ObjectKind, const Sprite*> ItemSprites;
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
	std::array<ShagAnimation, 4> BonusActions;
	std::array<const Sprite*, 8> Blinks{};
	std::array<const Sprite*, 4> Kicks{}, Frigida{};
};
