#pragma once
#include "BitmapFont.h"
#include "LevelScene.h"
#include "RoundResult.h"
#include <memory>

namespace sven2 {

class GameSession {
public:
    GameSession(Engine& engine,const LevelPackInfo& pack,const LevelSetInfo& levelset);
    void update(float seconds,int dx,int dy,bool actionPressed);
    void advance();
    void skipBonuses();
    const std::vector<SpriteDraw>& draws();
    std::vector<std::string> takeSounds();
    bool results() const { return results_; }
    bool ready() const { return results_ && bonusFinished_; }
    bool finished() const { return episodeComplete_; }
    bool gameOver() const { return lives_==0; }
    int score() const { return score_; }
    int lives() const { return lives_; }
    bool ufoActive() const { return scene_->ufoActive(); }
    int levelNumber() const { return static_cast<int>(levelIndex_)+1; }
    int timeBonus() const { return timeBonus_; }
    int remainingTargets() const { return scene_->remainingTargets(); }
    RoundResult result() const;
    const LevelInfo& level() const { return levelset_.levels.at(levelIndex_); }

private:
    void load(size_t index);
    void addScore(int amount);
    void convertBonuses(float seconds);
    void animateScore(float seconds);
    struct LifeEffect { float x,y,vx,vy; bool incoming; };
    void updateLifeEffects(float seconds);
    void spawnLifeEffect(int slot,bool incoming);
    void gainLife();
    void applyLifeHit();
    void emitSound(const char* name);
    int randomInt(int limit);
    Engine& engine_;
    const LevelPackInfo& pack_;
    const LevelSetInfo& levelset_;
    std::unique_ptr<LevelScene> scene_;
    BitmapFont font_,greenFont_;
    const Sprite *clock_,*scoreIcon_,*lifeIcon_;
    std::vector<SpriteDraw> draws_;
    std::vector<std::string> sounds_;
    std::vector<LifeEffect> lifeEffects_;
    size_t levelIndex_=0;
    float time_=0,ticks_=0,scoreTick_=0;
    int scoreChanges_=0;
    int score_=0,lives_=3,nextLife_=750,warningSecond_=-1,scoreFrame_=0;
    int visibleLives_=3,lastAttackScore_=0;
    float lifeInsuranceAge_=0;
    bool initialRefundUsed_=false;
    std::uint32_t randomState_=GetTickCount();
    int timeBonus_=0,sheepBonus_=0,lifeBonus_=0;
    int resultSheep_=0,resultLives_=0;
    int previousSheep_=0;
    bool results_=false,bonusFinished_=false,timedOut_=false;
    bool episodeComplete_=false;
    bool completionScheduled_=false;
    float completionRemaining_=0;
};
}
