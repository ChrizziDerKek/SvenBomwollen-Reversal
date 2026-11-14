#pragma once
#include "scene/SceneTypes.h"

namespace sven2 {
class LevelScene;
class UfoController {
    using Object=WorldObject;
    using Interaction=InteractionAnimation;
public:
    explicit UfoController(LevelScene& scene);
    UfoController(const UfoController&)=delete;
    UfoController& operator=(const UfoController&)=delete;
    void requestUfo(int target);
    void beginBeam();
    void updateUfo(float seconds);
    bool active() const { return ufo_.state>=0; }
private:
    friend class LevelEffects;
    friend class LevelItems;
    friend class LevelScene;
    friend class SceneRenderer;
    LevelScene& scene_;
    bool hideUfo_=false;
    UfoFlight ufo_;
};
}
