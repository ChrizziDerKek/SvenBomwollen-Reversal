#pragma once
#include "Engine/Structures.h"
#include "Scene/ShagAnimation.h"
#include "BaseCharacter.h"

class EnemyCharacters final : public BaseCharacter
{
    using Object = WorldObject;
    using Interaction = ShagAnimation;

public:
    EnemyCharacters(LevelScene& scene);
    EnemyCharacters(const EnemyCharacters&) = delete;
    EnemyCharacters& operator=(const EnemyCharacters&) = delete;
    bool PlayerInRange(const Object& actor, f32 distance) const;
    bool CanCatchPlayer(const Object& actor) const;
    void EnemyIdle(Object& actor, bool retry = false);
    void EnemyNotice(const Object& actor, bool item);
    bool ChooseEnemyStep(size_t index);
    void DrawEnemy(Object& actor);
    void EnemyArrived(size_t index);
    void UpdateEnemies(f32 seconds);
    void Update(f32 seconds) override { UpdateEnemies(seconds); }
    void BeginBattle(size_t index);
    void FinishBattle(Object& actor);

private:
    friend class LevelItems;
    friend class LevelScene;
    friend class PlayerCharacter;
    friend class SceneRenderer;
    friend class UfoController;

    i32 BattleEnemy = -1;
    f32 BattleRemaining = 0;
};