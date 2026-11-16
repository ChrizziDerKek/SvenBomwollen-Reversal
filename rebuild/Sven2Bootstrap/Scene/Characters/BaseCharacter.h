#pragma once
#include "Types.h"

class LevelScene;
class BaseCharacter
{
public:
	virtual ~BaseCharacter() = default;
	virtual void Update(f32 seconds) = 0;

	BaseCharacter(const BaseCharacter&) = delete;
	BaseCharacter& operator=(const BaseCharacter&) = delete;

protected:
	BaseCharacter(LevelScene& scene)
		: Scene(scene)
	{ }

	LevelScene& Scene;
};