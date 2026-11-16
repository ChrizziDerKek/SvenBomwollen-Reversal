#pragma once
#include "Types.h"
#include "Engine/Structures.h"
#include "Scene/ShagAnimation.h"
#include "BaseCharacter.h"

class AnimalCharacter : public BaseCharacter
{
	using Object = WorldObject;

public:
	AnimalCharacter(LevelScene& scene, size_t index, ObjectPhase phase = ObjectPhase::Animals);
	void Update(f32 seconds) override;
	void Resolve();
	virtual const ShagAnimation* GetInteraction(i32 facing) const;
	AnimalCharacter(const AnimalCharacter&) = delete;
	AnimalCharacter& operator=(const AnimalCharacter&) = delete;

protected:
	void UpdateOtherAnimal(f32 seconds);
	void ResolveSheep(Object& animal);
	bool AgeSheepMood(Object& animal, f32 seconds);
	bool MoveDemonSheep();
	bool ShockPlayer(Object& animal, i32 timeChange);
	void UpdateSheep(f32 seconds);

private:
	f32 SheepSpeed = 50;
};
