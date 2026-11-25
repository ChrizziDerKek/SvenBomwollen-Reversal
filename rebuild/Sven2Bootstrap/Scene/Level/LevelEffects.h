#pragma once
#include "Types.h"
#include "Engine/Structures.h"
#include "Scene/Objects/BaseObject.h"
#include "LevelCodes.h"

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
	void UpdateWeather(f32 seconds);
	void UpdateWorldEffect(size_t index, f32 seconds);
	void SpawnDigEffect(const Object& actor);
	void SpawnBeam(f32 x, f32 y);
	bool TakeSplashSound();

private:
	friend class AnimalCharacter;
	friend class Mermaid;
	friend class EnemyCharacter;
	friend class LevelItems;
	friend class LevelScene;
	friend class ObjectManager;
	friend class PlayerCharacter;
	friend class SceneRenderer;
	LevelScene& Scene;
	WeatherMode Weather = WeatherMode::None;
	f32 WeatherCountdown = 10; // Reset at native level entry, 0x41D204.
	std::vector<TimePopup> TimePopups;
	std::vector<Object> SceneEffects;
	std::array<std::array<SpriteDraw, 2>, 2> Splashes{};
	bool SplashSound = false;
	f32 TimeDelta = 0;
};

class WeatherParticle final : public BaseObject
{
public:
	WeatherParticle(LevelScene& scene, size_t index);
	void Initialize() override;
	void Update(f32 seconds) override;

private:
	f32 VelocityX = 0;
	f32 VelocityY = 0;
	f32 Lifetime = 0;
	f32 SwayAngle = 0;
	i32 SwayWidth = 0;
};

class ParticleEmitter : public BaseObject
{
public:
	ParticleEmitter(LevelScene& scene, size_t index);
	void Initialize() override;
	void Update(f32 seconds) override;

private:
	f32 Countdown = 0;
	bool Mirror = false;
};

class BubbleEmitter final : public ParticleEmitter
{
public:
	using ParticleEmitter::ParticleEmitter;
};

class YellowEmitter final : public ParticleEmitter
{
public:
	using ParticleEmitter::ParticleEmitter;
};

class EmitterParticle final : public BaseObject
{
public:
	EmitterParticle(LevelScene& scene, size_t index);
	void Initialize() override;
	void Update(f32 seconds) override;
	static bool Spawn(LevelScene& scene, ObjectKind kind, f32 x, f32 y, bool mirror);

private:
	f32 VelocityX = 0;
	f32 VelocityY = 0;
	f32 Lifetime = 0;
	f32 NativeDepth = 0;
};

class PowerupEffect final : public BaseObject
{
public:
	PowerupEffect(LevelScene& scene, size_t index);
	void Initialize() override;
	void Update(f32 seconds) override;
	static void Spawn(LevelScene& scene, ObjectKind kind, size_t target);

private:
	const std::vector<i32>* Frames = nullptr;
	f32 FrameAge = 0;
	f32 VelocityX = 0;
	f32 VelocityY = 0;
	f32 Lifetime = 0;
	bool SoundPlayed = false;
};
