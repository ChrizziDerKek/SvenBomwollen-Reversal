#pragma once
#include "scene/SceneTypes.h"

namespace sven2 {
class LevelScene;
class EnemyCharacters {
    using Object=WorldObject;
    using Interaction=InteractionAnimation;
public:
    explicit EnemyCharacters(LevelScene& scene);
    EnemyCharacters(const EnemyCharacters&)=delete;
    EnemyCharacters& operator=(const EnemyCharacters&)=delete;
    bool playerInRange(const Object& actor,float distance) const;
    bool canCatchPlayer(const Object& actor) const;
    void enemyIdle(Object& actor,bool retry=false);
    void enemyNotice(const Object& actor,bool item);
    bool chooseEnemyStep(size_t index);
    void drawEnemy(Object& actor);
    void enemyArrived(size_t index);
    void updateEnemies(float seconds);
    void beginBattle(size_t index);
    void finishBattle(Object& actor);
private:
    friend class LevelItems;
    friend class LevelScene;
    friend class PlayerCharacter;
    friend class SceneRenderer;
    friend class UfoController;
    LevelScene& scene_;
    int battleEnemy_=-1;
    float battleRemaining_=0;
};
}
