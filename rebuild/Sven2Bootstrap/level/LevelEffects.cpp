#include "level/LevelEffects.h"
#include "LevelScene.h"
#include "scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sven2 {
using namespace scene_data;
LevelEffects::LevelEffects(LevelScene& scene):scene_(scene) {}

void LevelEffects::addTime(int amount,float x,float y) {
    timeDelta_+=static_cast<float>(amount);
    const int offset=amount>=0?(amount<10?-10:amount<100?-20:-30):
                              (amount>-10?-20:amount>-100?-30:-40);
    timePopups_.push_back({x+offset,y,static_cast<float>(30+scene_.randomInt(40)),1.5f,amount});
}

void LevelEffects::spawnCompletionEffect(float x,float y,bool hearts) {
    size_t slot=scene_.world_.size();
    for(size_t i=0;i<scene_.world_.size();++i)
        if(scene_.world_[i].freed && i!=scene_.playerIndex_ && static_cast<int>(i)!=scene_.player_.activeSheep_ && static_cast<int>(i)!=scene_.ufoController_.ufo_.target) {
            slot=i;
            break;
        }
    if(slot>=256) return;
    Object effect;
    const auto* image=hearts?scene_.resources_.hearts_:scene_.resources_.explosion_;
    effect.kind=32;
    effect.x=x-image->frameWidth/2;
    effect.y=y-image->frameHeight/2;
    effect.committedY=effect.y;
    effect.depthOffset=image->frameHeight;
    effect.draw={image,static_cast<int>(effect.x),static_cast<int>(effect.y),0};
    if(slot==scene_.world_.size()) scene_.world_.push_back(effect);
    else scene_.world_[slot]=effect;
}

void LevelEffects::updateEffects(float seconds) {
    for(auto& effect:sceneEffects_) {
        if(effect.kind==34) {
            effect.freed=scene_.advanceActor(effect,AnimationSequence::electricFlash,15,seconds);
            continue;
        }
        if(effect.state==1) {
            effect.pulse-=seconds*60;
            if(effect.pulse<0) { effect.state=2; effect.age=0; }
        } else {
            effect.age+=seconds*5;
            if(effect.age>=18) {
                if(effect.state==0) { effect.state=1; effect.pulse=400; effect.age=17; }
                else effect.freed=true;
            }
        }
        effect.draw.frame=std::min(17,static_cast<int>(effect.age));
    }
    sceneEffects_.erase(std::remove_if(sceneEffects_.begin(),sceneEffects_.end(),
        [](const Object& effect) { return effect.freed; }),sceneEffects_.end());
    for(auto& effect:scene_.world_) {
        if(effect.kind!=32 || effect.freed) continue;
        effect.age+=seconds*15;
        effect.draw.frame=static_cast<int>(effect.age);
        effect.freed=effect.draw.frame>=effect.draw.sprite->rows;
    }
    for(auto& popup:timePopups_) {
        popup.y-=popup.speed*seconds;
        popup.life-=seconds;
    }
    timePopups_.erase(std::remove_if(timePopups_.begin(),timePopups_.end(),
        [](const auto& popup) { return popup.life<0; }),timePopups_.end());
}

void LevelEffects::spawnDigEffect(const Object& actor) {
    // Kind37 constructor 0x40C355 and updater 0x40A0CE: independent of dig timer.
    static constexpr int offsetX[4]={33,37,0,0},offsetY[4]={6,30,28,6};
    Object effect;
    effect.kind=37;
    effect.x=actor.x+offsetX[actor.facing/2];
    effect.y=actor.y+offsetY[actor.facing/2];
    effect.committedY=effect.y;
    effect.depthOffset=scene_.resources_.digging_->frameHeight-30;
    effect.draw={scene_.resources_.digging_,static_cast<int>(effect.x),static_cast<int>(effect.y),0};
    sceneEffects_.push_back(effect);
}

bool LevelEffects::takeSplashSound() {
    const bool pending=splashSound_;
    splashSound_=false;
    scene_.sounds_.erase(std::remove(scene_.sounds_.begin(),scene_.sounds_.end(),"Sven2/sfx/water_splash22.ogg"),scene_.sounds_.end());
    return pending;
}
}
