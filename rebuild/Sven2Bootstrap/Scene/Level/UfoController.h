#pragma once
#include "Types.h"
#include "Engine/Structures.h"

class LevelScene;
class UfoController
{
	using Object = WorldObject;

public:
	explicit UfoController(LevelScene& scene);
	UfoController(const UfoController&) = delete;
	UfoController& operator=(const UfoController&) = delete;
	void RequestUfo(i32 target);
	void BeginBeam();
	void UpdateUfo(f32 seconds);
	bool Active() const
	{
		return Ufo.State >= 0;
	}

private:
	friend class LevelEffects;
	friend class LevelItems;
	friend class LevelScene;
	friend class SceneRenderer;
	LevelScene& Scene;
	bool HideUfo = false;
	UfoFlight Ufo;
};
