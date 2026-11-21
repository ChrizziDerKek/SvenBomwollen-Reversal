#include "Scene/Level/LevelEffects.h"
#include "Scene/Objects/ObjectCatalog.h"
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
	const size_t slot = Scene.FindAvailableObjectSlot();
	if (slot >= 256)
		return;
	Object effect;
	const ::Sprite* image = Scene.IsBonusLevel() ? Scene.Resources.GetSprite(Scene.CoreAsset("Sven2/gfx/anim_clouds_with_flash1.bmp"), 16) : (hearts ? Scene.Resources.Hearts : Scene.Resources.Explosion);
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
		if (effect.Kind == ObjectKind::Beam)
		{
			effect.Freed = Scene.AdvanceActor(effect, AnimationSequence::UfoBeam, 15, seconds);
			continue;
		}
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
	SceneEffects.erase(std::remove_if(SceneEffects.begin(), SceneEffects.end(), [](const Object& effect) { return effect.Freed; }),
	                   SceneEffects.end());
	Scene.Objects.UpdatePhase(ObjectPhase::Effects, seconds);
	for (auto& popup : TimePopups)
	{
		popup.Y -= popup.Speed * seconds;
		popup.Life -= seconds;
	}
	TimePopups.erase(std::remove_if(TimePopups.begin(), TimePopups.end(), [](const auto& popup) { return popup.Life < 0; }),
	                 TimePopups.end());
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

void LevelEffects::SpawnBeam(f32 x, f32 y)
{
	Object effect;
	effect.Kind = ObjectKind::Beam;
	effect.Y = effect.CommittedY = y - 93;
	effect.DepthOffset = Scene.Resources.BeamSprite->FrameHeight;
	effect.Draw = { Scene.Resources.BeamSprite, static_cast<i32>(x) - 150, static_cast<i32>(y) - 93, Scene.Resources.AnimationFrames(AnimationSequence::UfoBeam).front() };
	SceneEffects.push_back(effect);
}

bool LevelEffects::TakeSplashSound()
{
	const bool pending = SplashSound;
	SplashSound = false;
	Scene.Sounds.erase(std::remove_if(Scene.Sounds.begin(), Scene.Sounds.end(), [](const SoundRequest& request) { return request.Asset == "Sven2/sfx/water_splash22.ogg"; }),
	                   Scene.Sounds.end());
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

ParticleEmitter::ParticleEmitter(LevelScene& scene, size_t index)
    : BaseObject(scene, ObjectPhase::Animals, index)
{}

void ParticleEmitter::Initialize()
{
	if (Scene.GetGameplayEdition() != GameEdition::XXX)
		throw std::logic_error("XXX particle emitters require the XXX edition profile.");
	const i32 interval = State.Kind == ObjectKind::BubbleEmitter ? 15 : 25;
	Mirror = State.Draw.Mirror;
	// Loaded records retain the countdown stored by the original editor.
	Countdown = State.Original[11] == static_cast<u32>(State.Kind) ? State.Pulse : static_cast<f32>(Scene.RandomInt(interval) + interval);
	State.Draw = {};
}

void ParticleEmitter::Update(f32 seconds)
{
	Countdown -= seconds * 60;
	if (Countdown < 0)
	{
		const bool bubbles = State.Kind == ObjectKind::BubbleEmitter;
		EmitterParticle::Spawn(Scene, bubbles ? ObjectKind::BubbleParticle : ObjectKind::YellowParticle, State.X, State.Y, Mirror);
		const i32 interval = bubbles ? 15 : 25;
		Countdown = static_cast<f32>(Scene.RandomInt(interval) + interval);
	}
	State.Pulse = Countdown;
	State.Draw = {};
}

EmitterParticle::EmitterParticle(LevelScene& scene, size_t index)
    : BaseObject(scene, ObjectPhase::Effects, index)
{}

bool EmitterParticle::Spawn(LevelScene& scene, ObjectKind kind, f32 x, f32 y, bool mirror)
{
	if (kind != ObjectKind::BubbleParticle && kind != ObjectKind::YellowParticle)
		throw std::invalid_argument("Not a native XXX emitter particle.");
	WorldObject particle;
	particle.Kind = kind;
	particle.X = x;
	particle.Y = y;
	particle.Draw.Mirror = mirror;
	return scene.GetObjects().AddTransient(particle);
}

void EmitterParticle::Initialize()
{
	const bool bubbles = State.Kind == ObjectKind::BubbleParticle;
	const Sprite* sprite = Scene.Resources.GetSprite(Scene.CoreAsset(bubbles ? "Sven2/gfx/pfx_bubbles.bmp" : "Sven2/gfx/pfx_yellow.bmp"), 16);
	State.X -= sprite->FrameWidth / 2;
	State.Y -= sprite->FrameHeight / 2;
	State.Draw = { sprite, static_cast<i32>(State.X), static_cast<i32>(State.Y), Scene.RandomInt(bubbles ? 16 : 3), State.Draw.Mirror };
	const i32 frame = State.Draw.Frame;
	VelocityX = static_cast<f32>(bubbles ? 30 - Scene.RandomInt(60) : 20 - Scene.RandomInt(40));
	VelocityY = static_cast<f32>(bubbles ? frame - Scene.RandomInt(30) - 50 : frame - Scene.RandomInt(15) - 25);
	Lifetime = static_cast<f32>(bubbles ? Scene.RandomInt(75) + 2 * (50 - frame) : Scene.RandomInt(25) - frame + 25);
	NativeDepth = 0;
	State.CommittedY = State.Y;
}

void EmitterParticle::Update(f32 seconds)
{
	State.X += VelocityX * seconds;
	State.Y += VelocityY * seconds;
	// 0x40D134..157 / 0x40D243..266: compare vx to zero, then add for vx<0,
	// subtract otherwise. Native code intentionally does not clamp at zero.
	const f32 drag = seconds * (State.Kind == ObjectKind::BubbleParticle ? 10 : 5);
	VelocityX += VelocityX < 0 ? drag : -drag;
	NativeDepth += VelocityY * seconds;
	State.DepthOffset = static_cast<i32>(NativeDepth);
	Lifetime -= seconds * 60;
	State.Freed = Lifetime < 0;
	State.CommittedY = State.Y;
	State.Draw.X = static_cast<i32>(State.X);
	State.Draw.Y = static_cast<i32>(State.Y);
}

PowerupEffect::PowerupEffect(LevelScene& scene, size_t index)
    : BaseObject(scene, ObjectPhase::Effects, index)
{}

void PowerupEffect::Spawn(LevelScene& scene, ObjectKind kind, size_t target)
{
	WorldObject effect;
	effect.Kind = kind;
	effect.ItemTarget = static_cast<i32>(target);
	if (kind != ObjectKind::SlowNotice)
	{
		const WorldObject& actor = scene.GetObjects()[target];
		effect.X = actor.Kind == ObjectKind::Player ? scene.GetObjects().GetPlayer(actor.TargetPlayer).GetX() + actor.CenterX : actor.X + actor.CenterX;
		effect.Y = actor.Kind == ObjectKind::Player ? scene.GetObjects().GetPlayer(actor.TargetPlayer).GetY() + actor.CenterY : actor.Y + actor.CenterY;
	}
	scene.GetObjects().AddTransient(effect);
}

void PowerupEffect::Initialize()
{
	const CharacterDefinition* definition = FindCharacterDefinition(State.Kind);
	State.Draw.Sprite = Scene.Resources.GetSprite(Scene.CoreAsset(definition->SpriteAsset), definition->Frames);
	State.X -= State.Draw.Sprite->FrameWidth / 2;
	State.Y -= State.Draw.Sprite->FrameHeight / 2;
	State.DepthOffset = State.Draw.Sprite->FrameHeight;
	switch (State.Kind)
	{
	case ObjectKind::RocketBurst:
		VelocityX = static_cast<f32>(Scene.RandomInt(25) + 5);
		VelocityY = -100;
		break;
	case ObjectKind::SlowBubble:
		Frames = &Scene.Resources.FrameList("aMushroom1");
		VelocityX = static_cast<f32>(50 - Scene.RandomInt(100));
		VelocityY = static_cast<f32>(-Scene.RandomInt(100));
		Lifetime = static_cast<f32>(Scene.RandomInt(50) + 25);
		break;
	case ObjectKind::SmellBurst:
		Frames = &Scene.Resources.FrameList("aMushroom2");
		Lifetime = static_cast<f32>(Scene.RandomInt(150) + 50);
		break;
	case ObjectKind::FlowerBurst:
		Frames = &Scene.Resources.FrameList("aRose");
		Lifetime = static_cast<f32>(Scene.RandomInt(150) + 50);
		break;
	case ObjectKind::SlowNotice:
		Lifetime = static_cast<f32>(Scene.RandomInt(150) + 50);
		break;
	default:
		throw std::logic_error("Unsupported repeated-pickup effect.");
	}
	State.Draw.X = static_cast<i32>(State.X);
	State.Draw.Y = static_cast<i32>(State.Y);
}

void PowerupEffect::Update(f32 seconds)
{
	if (Frames)
	{
		FrameAge += seconds * (State.Kind == ObjectKind::RocketBurst ? 30 : 5);
		if (FrameAge >= Frames->size())
			FrameAge = 0;
		State.Draw.Frame = Frames->at(static_cast<size_t>(FrameAge));
	}
	if (State.Kind == ObjectKind::RocketBurst || State.Kind == ObjectKind::SlowBubble)
	{
		State.X += VelocityX * seconds;
		State.Y += VelocityY * seconds;
		if (State.Kind == ObjectKind::RocketBurst)
		{
			VelocityX += seconds * 50;
			VelocityY -= seconds * 600;
			State.Freed = State.Y < -128;
		}
		else
		{
			VelocityX += VelocityX < 0 ? seconds * 5 : -seconds * 5;
			VelocityY += seconds * 60;
		}
	}
	if (State.Kind != ObjectKind::RocketBurst)
	{
		Lifetime -= seconds * 60;
		State.Freed = Lifetime < 0;
	}
	if (State.ItemTarget >= 0 && static_cast<size_t>(State.ItemTarget) < Scene.Objects.Size())
	{
		const WorldObject& target = Scene.Objects[State.ItemTarget];
		if (State.Kind == ObjectKind::SmellBurst || State.Kind == ObjectKind::FlowerBurst)
		{
			State.X = target.X + target.CenterX - State.Draw.Sprite->FrameWidth / 2;
			State.Y = target.Y + target.Lift + target.CenterY - State.Draw.Sprite->FrameHeight / 2;
		}
		if (!SoundPlayed && Lifetime < 75 && State.Kind == ObjectKind::SlowNotice && !target.Freed && Scene.IsEnemy(target.Kind))
		{
			SoundPlayed = true;
			Scene.EmitNamedSound(target.Kind == ObjectKind::Dog ? "SfxDogFoundSven" : (target.Kind == ObjectKind::Farmer ? "SfxGuyFoundSven" : (target.Kind == ObjectKind::Girl ? "SfxBroemseFoundSven" : "SfxAlienFoundSven")), 35000);
		}
		else if (!SoundPlayed && Lifetime < 75 && (State.Kind == ObjectKind::SmellBurst || State.Kind == ObjectKind::FlowerBurst))
		{
			SoundPlayed = true;
			if (target.Kind == ObjectKind::Sheep && Scene.RandomInt(3))
				Scene.EmitNamedSound(State.Kind == ObjectKind::FlowerBurst ? (Scene.RandomInt(2) ? "SfxBlaat21" : "SfxBlaat20") : (Scene.RandomInt(2) ? "SfxBlaat23" : "SfxBlaat22"));
		}
	}
	State.CommittedY = State.Y;
	State.Draw.X = static_cast<i32>(State.X);
	State.Draw.Y = static_cast<i32>(State.Y);
}
