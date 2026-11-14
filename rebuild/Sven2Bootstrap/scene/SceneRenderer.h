#pragma once
#include "scene/SceneTypes.h"
#include "BitmapFont.h"

namespace sven2 {
class LevelScene;
class SceneRenderer {
    using Object=WorldObject;
    using Interaction=InteractionAnimation;
public:
    explicit SceneRenderer(LevelScene& scene);
    SceneRenderer(const SceneRenderer&)=delete;
    SceneRenderer& operator=(const SceneRenderer&)=delete;
    const std::vector<SpriteDraw>& draws();
private:
    friend class LevelScene;

    LevelScene& scene_;
    BitmapFont overlayFont_;
    std::vector<Object> sorted_;
    std::vector<SpriteDraw> overlays_, draws_;
    SpriteDraw background_{};
};
}
