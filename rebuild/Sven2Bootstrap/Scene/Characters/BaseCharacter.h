#pragma once
#include "Types.h"
#include "Scene/Objects/BaseObject.h"

class LevelScene;
class BaseCharacter : public BaseObject
{
public:
	virtual ~BaseCharacter() = default;
	virtual void Update(f32 seconds) = 0;

	BaseCharacter(const BaseCharacter&) = delete;
	BaseCharacter& operator=(const BaseCharacter&) = delete;

protected:
	BaseCharacter(LevelScene& scene, ObjectPhase phase = ObjectPhase::Player, size_t index = 0)
	    : BaseObject(scene, phase, index)
	{
	}
};
