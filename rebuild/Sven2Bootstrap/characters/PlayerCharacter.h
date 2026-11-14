#pragma once
#include "scene/SceneTypes.h"

namespace sven2 {
class LevelScene;
class PlayerCharacter {
    using Object=WorldObject;
    using Interaction=InteractionAnimation;
public:
    explicit PlayerCharacter(LevelScene& scene);
    PlayerCharacter(const PlayerCharacter&)=delete;
    PlayerCharacter& operator=(const PlayerCharacter&)=delete;
    void markPlayerCell();
    void cancelPlayerStep();
    void teleport(int source);
    void restorePlayer();
    void act(bool continuation=false);
    void finishInteraction();
    void advancePlayer(float dt,int dx,int dy);
private:
    friend class AnimalCharacters;
    friend class EnemyCharacters;
    friend class LevelEffects;
    friend class LevelGrid;
    friend class LevelItems;
    friend class LevelScene;
    friend class SceneRenderer;
    friend class SceneResources;
    friend class UfoController;
    LevelScene& scene_;
    int playerBeamState_=0;
    float playerLift_=0, playerBeamClock_=0;
    float playerShockAge_=-1;
    float frigidaRate_=16;
    const Interaction* interaction_=nullptr;
    float animationAge_=0, actionAge_=0;
    float speed_=145, diagonal_=0.75f, walkRate_=24, defaultRate_=15, actionRate_=20;
    bool actionDown_=false, actionQueued_=false;
    int facing_=0;
    int activeSheep_=-1, playerAction_=0, selectedInteraction_=-1;
    float x_{},y_{},targetX_{},targetY_{};
};
}
