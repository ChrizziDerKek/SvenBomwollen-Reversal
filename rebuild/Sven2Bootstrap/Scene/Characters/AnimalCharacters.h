#pragma once
#include "Engine/Structures.h"
#include "Scene/ShagAnimation.h"
#include "BaseCharacter.h"

class AnimalCharacters final : public BaseCharacter
{
    using Object = WorldObject;
    using Interaction = ShagAnimation;

public:
    AnimalCharacters(LevelScene& scene);
    AnimalCharacters(const AnimalCharacters&) = delete;
    AnimalCharacters& operator=(const AnimalCharacters&) = delete;
    void UpdateOtherAnimals(f32 seconds);
    void ResolveSheep(Object& animal);
    bool AgeSheepMood(Object& animal, f32 seconds);
    bool MoveDemonSheep(size_t index);
    bool ShockPlayer(Object& animal, i32 timeChange);
    void UpdateSheep(f32 seconds);
    void Update(f32 seconds) override { UpdateOtherAnimals(seconds); UpdateSheep(seconds); }

private:
    friend class LevelScene;
    friend class SceneResources;

    f32 SheepSpeed = 50;
};