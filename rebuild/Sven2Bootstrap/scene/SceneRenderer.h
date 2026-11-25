#pragma once
#include "Types.h"
#include "Engine/BitmapFont.h"
#include "ShagAnimation.h"
#include <array>
#include <memory>

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
	void AppendHelp(std::vector<SpriteDraw>& draws) const;

private:
	friend class LevelScene;

	LevelScene& Scene;
	BitmapFont OverlayFont;
	mutable std::array<std::unique_ptr<BitmapFont>, 4> HelpFonts;
	std::vector<Object> Sorted;
	std::vector<SpriteDraw> Overlays;
	std::vector<SpriteDraw> DrawCommands;
	SpriteDraw Background{};
};
