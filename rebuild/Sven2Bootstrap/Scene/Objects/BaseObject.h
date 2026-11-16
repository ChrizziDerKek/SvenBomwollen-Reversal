#pragma once
#include "Types.h"
#include "Engine/Structures.h"
#include "ObjectKind.h"

class LevelScene;

class BaseObject
{
public:
	virtual ~BaseObject() = default;
	virtual void Update(f32 seconds) = 0;
	virtual void Initialize()
	{
	}
	WorldObject& GetState()
	{
		return State;
	}
	const WorldObject& GetState() const
	{
		return State;
	}
	size_t GetIndex() const
	{
		return SlotIndex;
	}
	ObjectPhase GetPhase() const
	{
		return Phase;
	}
	BaseObject(const BaseObject&) = delete;
	BaseObject& operator=(const BaseObject&) = delete;

protected:
	BaseObject(LevelScene& scene, ObjectPhase phase, size_t index = 0)
	    : Scene(scene), Phase(phase), SlotIndex(index)
	{
	}

	LevelScene& Scene;
	WorldObject State;

private:
	friend class ObjectManager;
	ObjectPhase Phase;
	size_t SlotIndex;
};
