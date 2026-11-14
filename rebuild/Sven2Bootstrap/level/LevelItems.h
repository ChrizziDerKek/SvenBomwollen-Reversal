#pragma once
#include "scene/SceneTypes.h"

namespace sven2 {
class LevelScene;
class LevelItems {
    using Object=WorldObject;
    using Interaction=InteractionAnimation;
public:
    explicit LevelItems(LevelScene& scene);
    LevelItems(const LevelItems&)=delete;
    LevelItems& operator=(const LevelItems&)=delete;
    int findDistraction(const Object& actor,int code) const;
    void removeItem(int tile);
    void spawnItem(int kind,int tile);
    void spawnItems();
    void updateItems(float seconds);
    void affectNearbySheep();
    void pickupPlayerItem();
private:
    friend class AnimalCharacters;
    friend class EnemyCharacters;
    friend class LevelScene;
    friend class PlayerCharacter;
    friend class SceneRenderer;
    friend class UfoController;
    LevelScene& scene_;
    float spawnClock_=0,spawnPeriod_=10,effectRemaining_=0,effectAge_=0;
    int playerEffect_=0, livesGained_=0;
};
}
