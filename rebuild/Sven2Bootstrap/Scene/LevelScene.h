#include "Engine/SoundRequest.h"
#include "Types.h"
#include "Scene/Objects/ObjectManager.h"
#pragma once
#include "Engine/Structures.h"
#include "Scene/Level/LevelGrid.h"
#include "Scene/SceneResources.h"
#include "Scene/Characters/PlayerCharacter.h"
#include "Scene/Characters/EnemyCharacter.h"
#include "Scene/Characters/AnimalCharacter.h"
#include "Scene/Level/LevelItems.h"
#include "Scene/Level/UfoController.h"
#include "Scene/Level/LevelEffects.h"
#include "Scene/SceneRenderer.h"

class LevelScene
{
public:
	explicit LevelScene(MudGE& engine);
	LevelScene(const LevelScene&) = delete;
	LevelScene& operator=(const LevelScene&) = delete;
	LevelScene(MudGE& engine, u32 seed);
	LevelScene(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, u32 seed = GetTickCount(), u32* sharedRandomState = nullptr, size_t playerCount = 1);
	PlayerCharacter& GetPlayer();
	const PlayerCharacter& GetPlayer() const;
	size_t GetPlayerIndex() const;
	ObjectManager::CombatState& GetCombat()
	{
		return Objects.Combats[ActivePlayer];
	}
	size_t GetPlayerCount() const
	{
		return PlayerCount;
	}
	class PlayerScope
	{
	public:
		PlayerScope(LevelScene& scene, size_t number);
		~PlayerScope();
		PlayerScope(const PlayerScope&) = delete;
		PlayerScope& operator=(const PlayerScope&) = delete;

	private:
		LevelScene& Scene;
		size_t Previous;
	};
	void Move(f32 seconds, i32 dx, i32 dy);
	// actionPressed is the current space-key state; edges are buffered during an interaction.
	void Update(f32 seconds, i32 dx, i32 dy, bool actionPressed);
	void Update(f32 seconds, const std::array<PlayerInput, 2>& inputs);
	void TimeExpired();
	GameEdition GetGameplayEdition() const
	{
		return GameplayEdition;
	}
	BonusMode GetBonusMode() const
	{
		return Mode;
	}
	bool IsBonusLevel() const
	{
		return Mode != BonusMode::Sven;
	}
	bool GetBonusUnlocked() const
	{
		return BonusUnlocked;
	}
	void UnlockBonus()
	{
		BonusUnlocked = true;
	}
	void FinishBonus();
	void RecordAnimalCompletion(const WorldObject& animal);
	i32 FindInteractingPlayer(size_t slot) const;
	str CoreAsset(const str& path) const;
	bool SpawnNearPlayer(ObjectKind kind);
	size_t FindAvailableObjectSlot() const;
	bool Completed() const
	{
		return RemainingTargets == 0;
	}
	i32 GetRemainingTargets() const
	{
		return RemainingTargets;
	}
	i32 GetCompletedSheep() const
	{
		return CompletedSheep;
	}
	f32 GetInitialTime() const
	{
		return InitialTime;
	}
	bool UfoActive() const
	{
		return UfoController.Active();
	}
	i32 TakeScoreDelta();
	f32 TakeTimeDelta();
	std::vector<str> TakeSounds();
	std::vector<SoundRequest> TakeAudioEvents();
	i32 TakeLifeHits();    // attack events; GameSession applies campaign life insurance
	i32 TakeLivesGained(); // gift pickup; separate from the loss callback
	const std::vector<SpriteDraw>& Draws();
	// Consumes the splash event from both APIs, so callers may safely use either.
	bool TakeSplashSound();
	ObjectManager& GetObjects()
	{
		return Objects;
	}
	const LevelGrid& GetGrid() const
	{
		return Grid;
	}
	const ObjectManager& GetObjects() const
	{
		return Objects;
	}
	const SpriteDraw& GetPlayerDraw() const
	{
		return Objects[GetPlayerIndex()].Draw;
	}

public:
	LevelItems& GetItems()
	{
		return Items;
	}
	LevelEffects& GetEffects()
	{
		return Effects;
	}

private:
	friend class LevelGrid;
	friend class ObjectManager;
	friend class SceneResources;
	friend class PlayerCharacter;
	friend class EnemyCharacter;
	friend class AnimalCharacter;
	friend class Mermaid;
	friend class ParticleEmitter;
	friend class EmitterParticle;
	friend class LevelItems;
	friend class UfoController;
	friend class LevelEffects;
	friend class SceneRenderer;
	using Object = WorldObject;

	static bool IsAnimal(ObjectKind kind);
	static bool IsEnemy(ObjectKind kind);
	static OccupancyCode GetOccupancyCode(ObjectKind kind);
	f32 Parameter(cstr key, f32 fallback) const;
	i32 RandomInt(i32 limit);
	bool AdvanceActor(Object& actor, AnimationSequence table, f32 rate, f32 seconds);
	void EmitNamedSound(cstr name);
	void AddScore(i32 amount);
	void RecordLifeLoss();
	void RemoveActor(size_t index, bool playerResolved);
	MudGE& Engine;
	GameEdition GameplayEdition;
	BonusMode Mode = BonusMode::Sven;
	bool BonusUnlocked = false;
	bool BonusFinished = false;
	size_t PlayerCount = 1;
	size_t ActivePlayer = 0;
	i32 PendingLifeHits = 0;
	ObjectManager Objects;
	bool Expired = false;
	i32 RemainingTargets = 0, ScoreDelta = 0;
	i32 CompletedSheep = 0;
	f32 InitialTime = 0;
	std::vector<SoundRequest> Sounds;
	u32 RandomState = 1;
	u32* SharedRandomState = nullptr;
	LevelGrid Grid;
	SceneResources Resources;

	LevelItems Items;
	UfoController UfoController;
	LevelEffects Effects;
	SceneRenderer Renderer;
};
