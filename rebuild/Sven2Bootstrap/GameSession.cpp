#include "GameSession.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace sven2 {
GameSession::GameSession(Engine& engine,const LevelPackInfo& pack,const LevelSetInfo& levelset)
    :engine_(engine),pack_(pack),levelset_(levelset),font_(engine,true),greenFont_(engine,true,true),
     clock_(engine.loadSprite("Sven2/gfx/timer.bmp",1,1)),
     scoreIcon_(engine.loadSprite("Sven2/gfx/score.bmp",4,1)),
     lifeIcon_(engine.loadSprite("Sven2/gfx/Life_icon.bmp",1,1)) {
    if(levelset_.levels.empty()) throw std::runtime_error("The selected levelset is empty.");
    load(0);
}

void GameSession::load(size_t index) {
    auto scene=std::make_unique<LevelScene>(engine_,pack_,levelset_.levels.at(index),randomState_,&randomState_);
    scene_=std::move(scene);
    levelIndex_=index;
    time_=scene_->initialTime();
    warningSecond_=-1;
    timedOut_=false;
    results_=false;
    episodeComplete_=false;
    completionScheduled_=false;
    completionRemaining_=0;
    bonusFinished_=false;
    timeBonus_=sheepBonus_=lifeBonus_=0;
    lifeEffects_.clear();
    visibleLives_=lives_;
    initialRefundUsed_=false;
    lifeInsuranceAge_=0;
    sounds_.push_back("sven2/sfx/start1.ogg");
}

void GameSession::advance() {
    if(!ready() || finished() || gameOver()) return;
    if(levelIndex_+1==levelset_.levels.size()) {
        episodeComplete_=true; // Original 0x410B38-0x410BC4, after final bonus acknowledgement.
        emitSound("SfxFinishPack");
        return;
    }
    const int total=previousSheep_+scene_->completedSheep();
    load(levelIndex_+1);
    previousSheep_=total;
}

void GameSession::addScore(int amount) {
    score_=std::max(0,score_+amount);
    scoreChanges_+=std::max(0,amount);
}

void GameSession::update(float seconds,int dx,int dy,bool actionPressed) {
    if(!std::isfinite(seconds) || seconds<=0) return;
    updateLifeEffects(seconds);
    if(results_) {
        convertBonuses(seconds);
        animateScore(seconds);
        return;
    }
    ticks_+=seconds;
    if(lifeInsuranceAge_>0) lifeInsuranceAge_+=seconds;
    // The original checks this during gameplay, before processing the next input.
    if(score_>nextLife_) {
        gainLife();
        nextLife_=nextLife_==750?1500:nextLife_+1500;
    }
    time_=std::max(0.0f,time_-seconds);
    const int second=static_cast<int>(time_);
    if(second<10 && second!=warningSecond_) {
        sounds_.push_back(second==0?"sven2/sfx/weckeralarm1.ogg":"sven2/sfx/wecker1.ogg");
        warningSecond_=second;
    }
    if(time_==0 && !timedOut_) {
        scene_->timeExpired();
        timedOut_=true;
    }
    scene_->update(seconds,dx,dy,actionPressed);
    addScore(scene_->takeScoreDelta());
    time_=std::max(0.0f,time_+scene_->takeTimeDelta());
    if(time_>0) timedOut_=false;
    for(int hit=scene_->takeLifeHits();hit>0 && lives_>0;--hit) applyLifeHit();
    for(int gained=scene_->takeLivesGained();gained>0;--gained) gainLife();
    auto events=scene_->takeSounds();
    sounds_.insert(sounds_.end(),events.begin(),events.end());
    animateScore(seconds);
    if(scene_->completed()) {
        if(!completionScheduled_) {
            completionScheduled_=true;
            completionRemaining_=1.5f; // Sven2_Level_ScheduleCompletionIfNoAnimals, 0x405668.
        } else {
            completionRemaining_-=seconds;
        }
    }
    if((completionScheduled_ && completionRemaining_<=0) || lives_==0) {
        results_=true;
        time_=std::floor(time_);
        timeBonus_=static_cast<int>(time_);
        sheepBonus_=scene_->completedSheep();
        lifeBonus_=lives_;
        resultSheep_=sheepBonus_;
        resultLives_=lifeBonus_;
        bonusFinished_=lives_==0;
        ticks_=0;
        if(lives_==0) sounds_.push_back("sven2/sfx/gameover.ogg");
    }
}

int GameSession::randomInt(int limit) {
    randomState_=randomState_*214013u+2531011u;
    return limit>0?static_cast<int>((randomState_>>16)&0x7fff)%limit:0;
}

void GameSession::emitSound(const char* name) {
    const auto path=engine_.data().soundPath(name);
    if(!path.empty()) sounds_.push_back(path);
}

void GameSession::spawnLifeEffect(int slot,bool incoming) {
    // Original object constructors 65/66, 0x40CF84-0x40D0FB.
    if(incoming) lifeEffects_.push_back({static_cast<float>(slot*32),-32,0,randomInt(100)*0.04f,true});
    else lifeEffects_.push_back({static_cast<float>(slot*32),565,
                                (50-randomInt(100))*0.04f,(20-randomInt(100))*0.04f,false});
}

void GameSession::updateLifeEffects(float seconds) {
    float remaining=seconds;
    while(remaining>0) {
        const float dt=std::min(remaining,1.0f/60);
        remaining-=dt;
        for(auto& effect:lifeEffects_) {
            effect.x+=effect.vx*dt*60;
            effect.y+=effect.vy*dt*60;
            effect.vy+=dt*5;
            if(!effect.incoming) effect.vx+=effect.vx<=0?dt:-dt;
            if(effect.incoming && effect.y>565) {
                ++visibleLives_; // Original kind66 increments the HUD count on arrival.
                effect.incoming=false;
                effect.y=601;
            }
        }
        lifeEffects_.erase(std::remove_if(lifeEffects_.begin(),lifeEffects_.end(),[&](const auto& effect) {
            return effect.y>600 || effect.x<-lifeIcon_->frameWidth;
        }),lifeEffects_.end());
    }
}

void GameSession::gainLife() {
    // 0x411AA0 changes the logical count now, the displayed count on landing.
    emitSound("SfxNewLife");
    spawnLifeEffect(lives_,true);
    ++lives_;
}

void GameSession::applyLifeHit() {
    // 0x412B00 compares the campaign score, not the current level's score.
    const bool initialRefund=score_==lastAttackScore_ && !initialRefundUsed_;
    if(initialRefund) initialRefundUsed_=true;
    else lastAttackScore_=score_;
    const bool refund=initialRefund || (lifeInsuranceAge_>0 && lifeInsuranceAge_<5);
    if(refund) {
        emitSound("SfxNewLife");
        spawnLifeEffect(lives_-1,false);
        --visibleLives_;
        spawnLifeEffect(lives_-1,true);
        if(!initialRefund) lifeInsuranceAge_=0.1f;
    } else {
        --lives_;
        --visibleLives_;
        lifeInsuranceAge_=0.1f;
        emitSound("SfxLoseLife");
        spawnLifeEffect(lives_,false);
    }
}

void GameSession::animateScore(float seconds) {
    if(scoreChanges_==0) return;
    scoreTick_-=seconds*60;
    while(scoreChanges_>0 && scoreTick_<=0) {
        --scoreChanges_;
        scoreTick_+=2;
        scoreFrame_=(scoreFrame_+1)%4;
        if(scoreFrame_==0) sounds_.push_back("sven2/sfx/points.ogg");
    }
}

void GameSession::convertBonuses(float seconds) {
    if(bonusFinished_) return;
    ticks_+=seconds*60;
    while(ticks_>=1 && time_>0) {
        --time_;
        --ticks_;
        addScore(1);
    }
    if(time_>0) return;
    while(ticks_>=10 && sheepBonus_>0) {
        --sheepBonus_;
        ticks_-=10;
        addScore(10);
    }
    if(sheepBonus_>0) return;
    while(ticks_>=10 && lifeBonus_>0) {
        --lifeBonus_;
        ticks_-=10;
        addScore(25);
    }
    bonusFinished_=lifeBonus_==0;
}

void GameSession::skipBonuses() {
    if(!results_ || bonusFinished_) return;
    const int remaining=static_cast<int>(time_)+sheepBonus_*10+lifeBonus_*25;
    time_=0;
    sheepBonus_=lifeBonus_=0;
    addScore(remaining);
    bonusFinished_=true;
}

const std::vector<SpriteDraw>& GameSession::draws() {
    draws_=scene_->draws();
    for(const auto& effect:lifeEffects_)
        draws_.push_back({lifeIcon_,static_cast<int>(effect.x),static_cast<int>(effect.y),0});
    draws_.push_back({clock_,4,4,0});
    draws_.push_back({scoreIcon_,654,4,scoreFrame_});
    for(int life=0;life<visibleLives_;++life) draws_.push_back({lifeIcon_,life*32,565,0});
    char text[40]{};
    std::snprintf(text,sizeof(text),"%5i",score_);
    font_.append(draws_,text,709,19);
    const int remaining=std::max(0,static_cast<int>(time_));
    std::snprintf(text,sizeof(text),"%2i:%02i",remaining/60,remaining%60);
    const auto& timerFont=remaining<10 && static_cast<int>(ticks_)%2 ? greenFont_:font_;
    timerFont.append(draws_,text,12,36);
    return draws_;
}

std::vector<std::string> GameSession::takeSounds() { return std::exchange(sounds_,{}); }

RoundResult GameSession::result() const {
    return {score_,levelNumber(),timeBonus_,resultSheep_,resultLives_,static_cast<int>(time_),
            sheepBonus_,lifeBonus_,finished(),gameOver(),previousSheep_+scene_->completedSheep(),scoreFrame_};
}
}
