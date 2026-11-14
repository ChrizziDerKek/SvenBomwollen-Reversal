#pragma once
#include "scene/SceneTypes.h"

namespace sven2 {
class LevelScene;
class AnimalCharacters {
    using Object=WorldObject;
    using Interaction=InteractionAnimation;
public:
    explicit AnimalCharacters(LevelScene& scene);
    AnimalCharacters(const AnimalCharacters&)=delete;
    AnimalCharacters& operator=(const AnimalCharacters&)=delete;
    void updateOtherAnimals(float seconds);
    void resolveSheep(Object& animal);
    bool ageSheepMood(Object& animal,float seconds);
    bool moveDemonSheep(size_t index);
    bool shockPlayer(Object& animal,int timeChange);
    void updateSheep(float seconds);
private:
    friend class LevelScene;
    friend class SceneResources;
    LevelScene& scene_;
    float sheepSpeed_=50;
};
}
