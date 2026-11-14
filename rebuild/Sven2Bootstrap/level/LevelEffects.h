#pragma once
#include "scene/SceneTypes.h"

namespace sven2 {
class LevelScene;
class LevelEffects {
    using Object=WorldObject;
    using Interaction=InteractionAnimation;
public:
    explicit LevelEffects(LevelScene& scene);
    LevelEffects(const LevelEffects&)=delete;
    LevelEffects& operator=(const LevelEffects&)=delete;
    void addTime(int amount,float x,float y);
    void spawnCompletionEffect(float x,float y,bool hearts);
    void updateEffects(float seconds);
    void spawnDigEffect(const Object& actor);
    bool takeSplashSound();
private:
    friend class AnimalCharacters;
    friend class EnemyCharacters;
    friend class LevelItems;
    friend class LevelScene;
    friend class PlayerCharacter;
    friend class SceneRenderer;
    LevelScene& scene_;
    std::vector<TimePopup> timePopups_;
    std::vector<Object> sceneEffects_;
    std::array<SpriteDraw,2> splashes_{};
    float splashAge_=100, splashPlayerAge_=100;
    bool splashSound_=false;
    float timeDelta_=0;
};
}
