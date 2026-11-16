#pragma once
#include "Engine/BitmapFont.h"
#include "ShagAnimation.h"

class LevelScene;

class SceneRenderer
{
	using Object = WorldObject;
	using Animation = ShagAnimation;

public:
	SceneRenderer(LevelScene& scene);
	SceneRenderer(const SceneRenderer&) = delete;
	SceneRenderer& operator=(const SceneRenderer&) = delete;
	const std::vector<SpriteDraw>& Draws();

private:
	friend class LevelScene;

	LevelScene& Scene;
	BitmapFont OverlayFont;
	std::vector<Object> Sorted;
	std::vector<SpriteDraw> Overlays;
	std::vector<SpriteDraw> Draws;
	SpriteDraw Background{};
};