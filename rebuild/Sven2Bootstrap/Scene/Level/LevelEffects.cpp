#include "Scene/Level/LevelEffects.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

LevelEffects::LevelEffects(LevelScene& scene)
    : Scene(scene)
{}

void LevelEffects::AddTime(i32 amount, f32 x, f32 y)
{
	TimeDelta += static_cast<f32>(amount);
	const i32 offset = amount >= 0 ? (amount < 10 ? -10 : (amount < 100 ? -20 : -30)) : (amount > -10 ? -20 : (amount > -100 ? -30 : -40));
	TimePopups.push_back({ x + offset, y, static_cast<f32>(30 + Scene.RandomInt(40)), 1.5f, amount });
}

void LevelEffects::SpawnCompletionEffect(f32 x, f32 y, bool hearts)
{
	size_t slot = Scene.Objects.Size();
	for (size_t i = 0; i < Scene.Objects.Size(); ++i)
	{
		if (Scene.Objects[i].Freed && i != Scene.PlayerIndex && static_cast<i32>(i) != Scene.Player.ActiveSheep && static_cast<i32>(i) != Scene.UfoController.Ufo.Target)
		{
			slot = i;
			break;
		}
	}
	if (slot >= 256)
		return;
	Object effect;
	const ::Sprite* image = hearts ? Scene.Resources.Hearts : Scene.Resources.Explosion;
	effect.Kind = ObjectKind::CompletionEffect;
	// Native centering truncates odd sprite dimensions before subtraction.
	// A floating midpoint shifts the effect and changes its depth ordering.
	effect.X = x - image->FrameWidth / 2;
	effect.Y = y - image->FrameHeight / 2;
	effect.CommittedY = effect.Y;
	effect.DepthOffset = image->FrameHeight;
	effect.Draw = { image, static_cast<i32>(effect.X), static_cast<i32>(effect.Y), 0 };
	if (slot == Scene.Objects.Size())
		Scene.Objects.Add(effect);
	else
		Scene.Objects.Replace(slot, effect);
}

void LevelEffects::UpdateEffects(f32 seconds)
{
	for (auto& effect : SceneEffects)
	{
		if (effect.Kind == ObjectKind::ElectricFlash)
		{
			effect.Freed = Scene.AdvanceActor(effect, AnimationSequence::ElectricFlash, 15, seconds);
			continue;
		}
		if (effect.State == ActorState::DigHolding)
		{
			effect.Pulse -= seconds * 60;
			if (effect.Pulse < 0)
			{
				effect.State = ActorState::DigDisappearing;
				effect.Age = 0;
			}
		}
		else
		{
			effect.Age += seconds * 5;
			if (effect.Age >= 18)
			{
				if (effect.State == ActorState::DigAppearing)
				{
					effect.State = ActorState::DigHolding;
					effect.Pulse = 400;
					effect.Age = 17;
				}
				else
					effect.Freed = true;
			}
		}
		effect.Draw.Frame = std::min(17, static_cast<i32>(effect.Age));
	}
	SceneEffects.erase(std::remove_if(SceneEffects.begin(), SceneEffects.end(), [](const Object& effect) { return effect.Freed; }), SceneEffects.end());
	Scene.Objects.UpdatePhase(ObjectPhase::Effects, seconds);
	for (auto& popup : TimePopups)
	{
		popup.Y -= popup.Speed * seconds;
		popup.Life -= seconds;
	}
	TimePopups.erase(std::remove_if(TimePopups.begin(), TimePopups.end(), [](const auto& popup) { return popup.Life < 0; }), TimePopups.end());
}

void LevelEffects::SpawnDigEffect(const Object& actor)
{
	// Kind37 constructor 0x40C355 and updater 0x40A0CE: independent of dig timer.
	static constexpr i32 offsetX[4] = { 33, 37, 0, 0 }, offsetY[4] = { 6, 30, 28, 6 };
	Object effect;
	effect.Kind = ObjectKind::Particle;
	effect.X = actor.X + static_cast<f32>(offsetX[actor.Facing / 2]);
	effect.Y = actor.Y + static_cast<f32>(offsetY[actor.Facing / 2]);
	effect.CommittedY = effect.Y;
	effect.DepthOffset = Scene.Resources.Digging->FrameHeight - 30;
	effect.Draw = { Scene.Resources.Digging, static_cast<i32>(effect.X), static_cast<i32>(effect.Y), 0 };
	SceneEffects.push_back(effect);
}

bool LevelEffects::TakeSplashSound()
{
	const bool pending = SplashSound;
	SplashSound = false;
	Scene.Sounds.erase(std::remove_if(Scene.Sounds.begin(), Scene.Sounds.end(), [](const SoundRequest& request) { return request.Asset == "Sven2/sfx/water_splash22.ogg"; }), Scene.Sounds.end());
	return pending;
}

void LevelEffects::UpdateWorldEffect(size_t index, f32 seconds)
{
	WorldObject& effect = Scene.Objects[index];
	if (effect.Kind != ObjectKind::CompletionEffect || effect.Freed)
		return;
	effect.Age += seconds * 15;
	effect.Draw.Frame = static_cast<i32>(effect.Age);
	effect.Freed = effect.Draw.Frame >= effect.Draw.Sprite->Rows;
}
