#pragma once
#include "scene/SceneTypes.h"
#include "level/LevelGrid.h"
#include "scene/SceneResources.h"
#include "characters/PlayerCharacter.h"
#include "characters/EnemyCharacters.h"
#include "characters/AnimalCharacters.h"
#include "level/LevelItems.h"
#include "level/UfoController.h"
#include "level/LevelEffects.h"
#include "scene/SceneRenderer.h"

namespace sven2 {
class LevelScene {
public:
    explicit LevelScene(Engine& engine);
    LevelScene(const LevelScene&)=delete;
    LevelScene& operator=(const LevelScene&)=delete;
    LevelScene(Engine& engine,std::uint32_t seed);
    LevelScene(Engine& engine,const LevelPackInfo& pack,const LevelInfo& level,std::uint32_t seed=GetTickCount(),std::uint32_t* sharedRandomState=nullptr);
    void move(float seconds, int dx, int dy);
    // actionPressed is the current space-key state; edges are buffered during an interaction.
    void update(float seconds, int dx, int dy, bool actionPressed);
    void timeExpired();
    bool completed() const { return remainingTargets_ == 0; }
    int remainingTargets() const { return remainingTargets_; }
    int completedSheep() const { return completedSheep_; }
    float initialTime() const { return initialTime_; }
    bool ufoActive() const { return ufoController_.active(); }
    int takeScoreDelta();
    float takeTimeDelta();
    std::vector<std::string> takeSounds();
    int takeLifeHits(); // attack events; GameSession applies campaign life insurance
    int takeLivesGained(); // gift pickup; separate from the loss callback
    const std::vector<SpriteDraw>& draws();
    // Consumes the splash event from both APIs, so callers may safely use either.
    bool takeSplashSound();
    const SpriteDraw& player() const { return world_[playerIndex_].draw; }
private:
    friend class LevelGrid;
    friend class SceneResources;
    friend class PlayerCharacter;
    friend class EnemyCharacters;
    friend class AnimalCharacters;
    friend class LevelItems;
    friend class UfoController;
    friend class LevelEffects;
    friend class SceneRenderer;
    using Object=WorldObject;
    using Interaction=InteractionAnimation;
    static bool isAnimal(int kind);
    static bool isEnemy(int kind);
    static int occupancyKind(int kind);
    float parameter(const char* key,float fallback) const;
    int randomInt(int limit);
    bool advanceActor(Object& actor,AnimationSequence table,float rate,float seconds);
    void emitNamedSound(const char* name);
    void addScore(int amount);
    void recordLifeLoss();
    void removeActor(size_t index,bool playerResolved);
    Engine& engine_;
    int pendingLifeHits_=0;
    std::vector<Object> world_;
    bool expired_=false;
    int remainingTargets_=0, scoreDelta_=0;
    int completedSheep_=0;
    float initialTime_=0;
    std::vector<std::string> sounds_;
    size_t playerIndex_{};
    std::uint32_t randomState_=1;
    std::uint32_t* sharedRandomState_=nullptr;
    LevelGrid grid_;
    SceneResources resources_;
    PlayerCharacter player_;
    EnemyCharacters enemies_;
    AnimalCharacters animals_;
    LevelItems items_;
    UfoController ufoController_;
    LevelEffects effects_;
    SceneRenderer renderer_;
};
}
