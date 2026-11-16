#include "Engine/SoundRequest.h"
#include "Types.h"
#pragma once
#include "Engine/BitmapFont.h"
#include "Scene/LevelScene.h"
#include "Engine/Structures.h"
#include <memory>

class GameSession
{
public:
	GameSession(MudGE& engine, const LevelPackInfo& pack, const LevelSetInfo& levelSet);
	void Update(f32 seconds, i32 dx, i32 dy, bool actionPressed);
	void Advance();
	void SkipBonuses();
	void DebugNextLevel();
	void DebugRestartLevel();
	void DebugChangeLives(i32 amount);
	bool DebugSpawn(ObjectKind kind);
	const std::vector<SpriteDraw>& Draws();
	std::vector<str> TakeSounds();
	std::vector<SoundRequest> TakeAudioEvents();
	bool GetResults() const
	{
		return Results;
	}
	bool Ready() const
	{
		return Results && BonusFinished;
	}
	bool Finished() const
	{
		return EpisodeComplete;
	}
	bool GameOver() const
	{
		return Lives == 0;
	}
	i32 GetScore() const
	{
		return Score;
	}
	i32 GetLives() const
	{
		return Lives;
	}
	bool UfoActive() const
	{
		return Scene->UfoActive();
	}
	i32 LevelNumber() const
	{
		return static_cast<i32>(LevelIndex) + 1;
	}
	i32 GetTimeBonus() const
	{
		return TimeBonus;
	}
	i32 GetRemainingTargets() const
	{
		return Scene->GetRemainingTargets();
	}
	RoundResult GetResult() const;
	const LevelInfo& Level() const
	{
		return LevelSet.Levels.at(LevelIndex);
	}

private:
	void Load(size_t index);
	void AddScore(i32 amount);
	void ConvertBonuses(f32 seconds);
	void AnimateScore(f32 seconds);
	struct LifeEffect
	{
		f32 X, Y, VX, VY;
		bool Incoming;
	};
	void UpdateLifeEffects(f32 seconds);
	void SpawnLifeEffect(i32 slot, bool incoming);
	void GainLife();
	void ApplyLifeHit();
	void EmitSound(cstr name);
	i32 RandomInt(i32 limit);
	MudGE& Engine;
	const LevelPackInfo& Pack;
	const LevelSetInfo& LevelSet;
	std::unique_ptr<LevelScene> Scene;
	BitmapFont Font, GreenFont;
	const Sprite *Clock, *ScoreIcon, *LifeIcon;
	std::vector<SpriteDraw> DrawCommands;
	std::vector<SoundRequest> Sounds;
	std::vector<LifeEffect> LifeEffects;
	size_t LevelIndex = 0;
	f32 Time = 0, Ticks = 0, ScoreTick = 0;
	i32 ScoreChanges = 0;
	i32 Score = 0, Lives = 3, NextLife = 750, WarningSecond = -1, ScoreFrame = 0;
	i32 VisibleLives = 3, LastAttackScore = 0;
	f32 LifeInsuranceAge = 0;
	bool InitialRefundUsed = false;
	u32 RandomState = GetTickCount();
	i32 TimeBonus = 0, SheepBonus = 0, LifeBonus = 0;
	i32 ResultSheep = 0, ResultLives = 0;
	i32 PreviousSheep = 0;
	bool Results = false, BonusFinished = false, TimedOut = false;
	bool EpisodeComplete = false;
	bool CompletionScheduled = false;
	f32 CompletionRemaining = 0;
};
