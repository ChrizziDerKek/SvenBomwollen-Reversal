#pragma once
#include "scene/SceneTypes.h"
#include "characters/CharacterController.h"

namespace sven2 {
class LevelScene;
class AnimalCharacters final:public CharacterController {
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
    void update(float seconds) override { updateOtherAnimals(seconds);updateSheep(seconds); }
private:
    friend class LevelScene;
    friend class SceneResources;
    float sheepSpeed_=50;
};
}
