#include "Game/GameSession.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

GameSession::GameSession(MudGE& engine, const LevelPackInfo& pack, const LevelSetInfo& levelSet, size_t playerCount)
    : Engine(engine), Pack(pack), LevelSet(levelSet), HudTheme(engine.GetData().GetThemeEdition()), Font(engine, true, false, HudTheme), GreenFont(engine, true, true, HudTheme), Clock(engine.LoadSprite(engine.GetData().EditionAsset(HudTheme, "Sven2/gfx/timer.bmp"), 1, 1)), ScoreIcon(engine.LoadSprite(engine.GetData().EditionAsset(HudTheme, "Sven2/gfx/score.bmp"), 4, 1)), LifeIcon(engine.LoadSprite(engine.GetData().EditionAsset(HudTheme, "Sven2/gfx/Life_icon.bmp"), 1, 1))
{
	if (levelSet.Levels.empty())
		throw std::runtime_error("The selected levelset is Empty.");
	PlayerCount = pack.Edition == GameEdition::Zwo ? 1 : std::clamp(playerCount, size_t{ 1 }, size_t{ 2 });
	Load(0);
}

GameEdition GameSession::GetGameplayEdition() const
{
	return Engine.GetData().GetGameplayEdition();
}

void GameSession::Load(size_t index)
{
	std::unique_ptr<LevelScene> nextScene = std::make_unique<LevelScene>(Engine, Pack, LevelSet.Levels.at(index), RandomState, &RandomState, PlayerCount);
	Scene = std::move(nextScene);
	LevelIndex = index;
	Time = Scene->GetInitialTime();
	WarningSecond = -1;
	TimedOut = false;
	Results = false;
	EpisodeComplete = false;
	CompletionScheduled = false;
	CompletionRemaining = 0;
	BonusFinished = false;
	TimeBonus = SheepBonus = LifeBonus = 0;
	LifeEffects.clear();
	VisibleLives = Lives;
	InitialRefundUsed = false;
	LifeInsuranceAge = 0;
	if (GetGameplayEdition() == GameEdition::XXX)
		EmitSound("SfxStart");
	else
		Sounds.push_back("sven2/sfx/start1.ogg");
}

void GameSession::Advance()
{
	if (!Ready() || Finished() || GameOver())
		return;
	if (LevelIndex + 1 == LevelSet.Levels.size())
	{
		EpisodeComplete = true; // Original 0x410B38-0x410BC4, after final bonus acknowledgement.
		EmitSound("SfxFinishPack");
		return;
	}
	const i32 total = PreviousSheep + Scene->GetCompletedSheep();
	Load(LevelIndex + 1);
	PreviousSheep = total;
}

void GameSession::DebugNextLevel()
{
	if (Lives == 0)
		return;
	Results = true;
	BonusFinished = true;
	Time = 0;
	TimeBonus = SheepBonus = LifeBonus = 0;
	ResultSheep = Scene->GetCompletedSheep();
	ResultLives = Lives;
	Advance();
}

void GameSession::DebugRestartLevel()
{
	Load(LevelIndex);
}

void GameSession::DebugChangeLives(i32 amount)
{
	amount = std::clamp(amount, -Lives, 99 - Lives);
	LifeEffects.clear();
	VisibleLives = Lives;
	while (amount > 0)
	{
		GainLife();
		--amount;
	}
	while (amount < 0)
	{
		--Lives;
		VisibleLives = std::max(0, VisibleLives - 1);
		EmitSound("SfxLoseLife");
		SpawnLifeEffect(Lives, false);
		++amount;
	}
}

bool GameSession::DebugSpawn(ObjectKind kind)
{
	if (Results || !Scene->SpawnNearPlayer(kind))
		return false;
	CompletionScheduled = false;
	CompletionRemaining = 0;
	return true;
}

void GameSession::AddScore(i32 amount)
{
	Score = std::max(0, Score + amount);
	ScoreChanges += std::max(0, amount);
}

void GameSession::Update(f32 seconds, i32 dx, i32 dy, bool actionPressed)
{
	std::array<PlayerInput, 2> inputs{};
	inputs[0] = { dx, dy, actionPressed };
	Update(seconds, inputs);
}

void GameSession::Update(f32 seconds, const std::array<PlayerInput, 2>& inputs)
{
	if (!std::isfinite(seconds) || seconds <= 0)
		return;
	UpdateLifeEffects(seconds);
	if (Results)
	{
		ConvertBonuses(seconds);
		AnimateScore(seconds);
		return;
	}
	Ticks += seconds;
	if (LifeInsuranceAge > 0)
		LifeInsuranceAge += seconds;
	// The original checks this during gameplay, before processing the next input.
	if (Score > NextLife)
	{
		GainLife();
		NextLife = NextLife == 750 ? 1500 : NextLife + 1500;
	}
	Time = std::max(0.0f, Time - seconds);
	const i32 second = static_cast<i32>(Time);
	if (second < 10 && second != WarningSecond)
	{
		Sounds.push_back(second == 0 ? "sven2/sfx/weckeralarm1.ogg" : "sven2/sfx/wecker1.ogg");
		WarningSecond = second;
	}
	if (Time == 0 && !TimedOut)
	{
		Scene->TimeExpired();
		TimedOut = true;
	}
	Scene->Update(seconds, inputs);
	AddScore(Scene->TakeScoreDelta());
	Time = std::max(0.0f, Time + Scene->TakeTimeDelta());
	if (Time > 0)
		TimedOut = false;
	for (i32 hit = Scene->TakeLifeHits(); hit > 0 && Lives > 0; --hit)
		ApplyLifeHit();
	for (i32 gained = Scene->TakeLivesGained(); gained > 0; --gained)
		GainLife();
	std::vector<SoundRequest> events = Scene->TakeAudioEvents();
	Sounds.insert(Sounds.end(), events.begin(), events.end());
	AnimateScore(seconds);
	if (Scene->Completed())
	{
		if (!CompletionScheduled)
		{
			CompletionScheduled = true;
			CompletionRemaining = 1.5f; // Sven2_Level_ScheduleCompletionIfNoAnimals, 0x405668.
		}
		else
		{
			CompletionRemaining -= seconds;
		}
	}
	if ((CompletionScheduled && CompletionRemaining <= 0) || Lives == 0)
	{
		Results = true;
		Time = std::floor(Time);
		TimeBonus = static_cast<i32>(Time);
		SheepBonus = Scene->GetCompletedSheep();
		LifeBonus = Lives;
		ResultSheep = SheepBonus;
		ResultLives = LifeBonus;
		BonusFinished = Lives == 0;
		Ticks = 0;
		if (Lives == 0)
			Sounds.push_back("sven2/sfx/gameover.ogg");
	}
}

i32 GameSession::RandomInt(i32 limit)
{
	RandomState = RandomState * 214013u + 2531011u;
	return limit > 0 ? static_cast<i32>((RandomState >> 16) & 0x7fff) % limit : 0;
}

void GameSession::EmitSound(cstr name)
{
	const str path = Engine.GetData().SoundPath(name, GetGameplayEdition());
	if (!path.empty())
		Sounds.push_back(path);
}

void GameSession::SpawnLifeEffect(i32 slot, bool incoming)
{
	// Original object constructors 65/66, 0x40CF84-0x40D0FB.
	if (incoming)
		LifeEffects.push_back({ static_cast<f32>(slot * 32), -32, 0, RandomInt(100) * 0.04f, true });
	else
		LifeEffects.push_back({ static_cast<f32>(slot * 32), 565, (50 - RandomInt(100)) * 0.04f, (20 - RandomInt(100)) * 0.04f, false });
}

void GameSession::UpdateLifeEffects(f32 seconds)
{
	f32 remaining = seconds;
	while (remaining > 0)
	{
		const f32 dt = std::min(remaining, 1.0f / 60);
		remaining -= dt;
		for (LifeEffect& effect : LifeEffects)
		{
			effect.X += effect.VX * dt * 60;
			effect.Y += effect.VY * dt * 60;
			effect.VY += dt * 5;
			if (!effect.Incoming)
				effect.VX += effect.VX <= 0 ? dt : -dt;
			if (effect.Incoming && effect.Y > 565)
			{
				++VisibleLives; // Original kind66 increments the HUD count on arrival.
				effect.Incoming = false;
				effect.Y = 601;
			}
		}
		LifeEffects.erase(std::remove_if(LifeEffects.begin(), LifeEffects.end(), [&](const LifeEffect& effect) { return effect.Y > 600 || effect.X < -LifeIcon->FrameWidth; }), LifeEffects.end());
	}
}

void GameSession::GainLife()
{
	// 0x411AA0 changes the logical count now, the displayed count on landing.
	EmitSound("SfxNewLife");
	SpawnLifeEffect(Lives, true);
	++Lives;
}

void GameSession::ApplyLifeHit()
{
	// 0x412B00 compares the campaign score, not the current level's score.
	const bool initialRefund = Score == LastAttackScore && !InitialRefundUsed;
	if (initialRefund)
		InitialRefundUsed = true;
	else
		LastAttackScore = Score;
	const bool refund = initialRefund || (LifeInsuranceAge > 0 && LifeInsuranceAge < 5);
	if (refund)
	{
		EmitSound("SfxNewLife");
		SpawnLifeEffect(Lives - 1, false);
		--VisibleLives;
		SpawnLifeEffect(Lives - 1, true);
		if (!initialRefund)
			LifeInsuranceAge = 0.1f;
	}
	else
	{
		--Lives;
		--VisibleLives;
		LifeInsuranceAge = 0.1f;
		EmitSound("SfxLoseLife");
		SpawnLifeEffect(Lives, false);
	}
}

void GameSession::AnimateScore(f32 seconds)
{
	if (ScoreChanges == 0)
		return;
	ScoreTick -= seconds * 60;
	while (ScoreChanges > 0 && ScoreTick <= 0)
	{
		--ScoreChanges;
		ScoreTick += 2;
		ScoreFrame = (ScoreFrame + 1) % 4;
		if (ScoreFrame == 0)
			Sounds.push_back("sven2/sfx/points.ogg");
	}
}

void GameSession::ConvertBonuses(f32 seconds)
{
	if (BonusFinished)
		return;
	Ticks += seconds * 60;
	while (Ticks >= 1 && Time > 0)
	{
		--Time;
		--Ticks;
		AddScore(1);
	}
	if (Time > 0)
		return;
	while (Ticks >= 10 && SheepBonus > 0)
	{
		--SheepBonus;
		Ticks -= 10;
		AddScore(10);
	}
	if (SheepBonus > 0)
		return;
	while (Ticks >= 10 && LifeBonus > 0)
	{
		--LifeBonus;
		Ticks -= 10;
		AddScore(25);
	}
	BonusFinished = LifeBonus == 0;
}

void GameSession::SkipBonuses()
{
	if (!Results || BonusFinished)
		return;
	const i32 remaining = static_cast<i32>(Time) + SheepBonus * 10 + LifeBonus * 25;
	Time = 0;
	SheepBonus = LifeBonus = 0;
	AddScore(remaining);
	BonusFinished = true;
}

const std::vector<SpriteDraw>& GameSession::Draws()
{
	DrawCommands = Scene->Draws();
	for (const LifeEffect& effect : LifeEffects)
		DrawCommands.push_back({ LifeIcon, static_cast<i32>(effect.X), static_cast<i32>(effect.Y), 0 });
	DrawCommands.push_back({ Clock, 4, 4, 0 });
	DrawCommands.push_back({ ScoreIcon, 654, 4, ScoreFrame });
	for (i32 life = 0; life < VisibleLives; ++life)
		DrawCommands.push_back({ LifeIcon, life * 32, 565, 0 });
	char text[40]{};
	std::snprintf(text, sizeof(text), "%5i", Score);
	Font.Append(DrawCommands, text, HudTheme == GameEdition::XXX ? 729 : 709, 19);
	const i32 remaining = std::max(0, static_cast<i32>(Time));
	std::snprintf(text, sizeof(text), "%2i:%02i", remaining / 60, remaining % 60);
	const BitmapFont& timerFont = remaining < 10 && static_cast<i32>(Ticks) % 2 ? GreenFont : Font;
	timerFont.Append(DrawCommands, text, HudTheme == GameEdition::XXX ? 22 : 12, HudTheme == GameEdition::XXX ? 34 : 36);
	return DrawCommands;
}

std::vector<SoundRequest> GameSession::TakeAudioEvents()
{
	std::vector<SoundRequest> result = std::exchange(Sounds, {});
	if (GetGameplayEdition() == GameEdition::XXX)
		for (SoundRequest& request : result)
			request.Asset = Engine.GetData().EditionAsset(GetGameplayEdition(), request.Asset);
	return result;
}

std::vector<str> GameSession::TakeSounds()
{
	std::vector<str> result;
	for (const SoundRequest& request : TakeAudioEvents())
		result.push_back(request.Asset);
	return result;
}

RoundResult GameSession::GetResult() const
{
	return { Score, LevelNumber(), TimeBonus, ResultSheep, ResultLives, static_cast<i32>(Time), SheepBonus, LifeBonus, Finished(), GameOver(), PreviousSheep + Scene->GetCompletedSheep(), ScoreFrame };
}
