#include "PlayerAction.h"
#pragma once
#include "Types.h"
#include "Engine/Structures.h"
#include "Scene/ShagAnimation.h"
#include "BaseCharacter.h"

class PlayerCharacter final : public BaseCharacter
{
	using Object = WorldObject;

public:
	PlayerCharacter(LevelScene& scene, size_t number = 0);
	size_t GetNumber() const
	{
		return Number;
	}
	PlayerCharacter(const PlayerCharacter&) = delete;
	PlayerCharacter& operator=(const PlayerCharacter&) = delete;
	void MarkPlayerCell();
	void CancelPlayerStep();
	void Teleport(i32 source);
	void RestorePlayer();
	void Act(bool continuation = false);
	void FinishInteraction();
	void AdvancePlayer(f32 dt, i32 dx, i32 dy);
	void SetInput(const PlayerInput& input);
	void Update(f32 seconds) override;

private:
	friend class AnimalCharacter;
	friend class EnemyCharacter;
	friend class LevelEffects;
	friend class LevelGrid;
	friend class ObjectManager;
	friend class LevelItems;
	friend class LevelScene;
	friend class SceneRenderer;
	friend class SceneResources;
	friend class UfoController;
	bool CanRepeatInteraction(const WorldObject& animal) const;

	i32 MovementX = 0;
	size_t Number = 0;
	PlayerPowerup Powerup;
	f32 SplashAge = 100;
	i32 MovementY = 0;
	i32 PlayerBeamState = 0;
	f32 PlayerLift = 0;
	f32 PlayerBeamClock = 0;
	f32 PlayerShockAge = -1;
	f32 FrigidaRate = 16;
	const ShagAnimation* Interaction = nullptr;
	f32 AnimationAge = 0;
	f32 ActionAge = 0;
	f32 Speed = 145;
	f32 Diagonal = 0.75f;
	f32 WalkRate = 24;
	f32 DefaultRate = 15;
	f32 ActionRate = 20;
	bool ActionArmed;
	bool ActionQueued = false;
	i32 Facing = 0;
	i32 ActiveSheep = -1;
	PlayerActionState PlayerAction = PlayerActionState::Idle;
	i32 SelectedInteraction = -1;
	f32 X{};
	f32 Y{};
	f32 TargetX{};
	f32 TargetY{};
};
