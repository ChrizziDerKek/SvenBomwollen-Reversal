#include "level/UfoController.h"
#include "LevelScene.h"
#include "scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sven2 {
using namespace scene_data;
UfoController::UfoController(LevelScene& scene):scene_(scene) {}

void UfoController::requestUfo(int target) {
    if(ufo_.state>=0 || target<0 || static_cast<size_t>(target)>=scene_.world_.size() || scene_.world_[target].freed) return;
    const auto& actor=scene_.world_[target];
    ufo_=UfoFlight{};
    ufo_.state=0;
    ufo_.target=target;
    ufo_.targetTile=actor.tile;
    if(static_cast<size_t>(target)==scene_.playerIndex_) {
        ufo_.x=scene_.player_.x_+60;
        ufo_.y=scene_.player_.y_+60;
        ufo_.targetTile=scene_.grid_.tileAt(scene_.player_.x_,scene_.player_.y_);
    } else {
        ufo_.x=actor.x+(actor.kind==5?80:60);
        ufo_.y=actor.y+(actor.kind==5?120:actor.kind==6?80:60);
    }
    scene_.sounds_.push_back("Sven2/sfx/ufo2.ogg");
}

void UfoController::beginBeam() {
    ufo_.clock=250.0f/60;
    ufo_.beam=false;
    ufo_.collectAnimals=false;
    const int target=ufo_.target;
    if(target<0 || static_cast<size_t>(target)>=scene_.world_.size()) { ufo_.clock=0; return; }
    auto& actor=scene_.world_[target];
    if(actor.freed || (static_cast<size_t>(target)!=scene_.playerIndex_ && actor.tile!=ufo_.targetTile) ||
       (scene_.isAnimal(actor.kind) && (scene_.player_.activeSheep_==target || actor.state==4 || actor.state==3 ||
           (actor.kind==2 && actor.state>=11 && actor.state<=14)))) {
        ufo_.clock=0;
        return;
    }
    if(static_cast<size_t>(target)==scene_.playerIndex_) {
        if(scene_.player_.playerAction_ || scene_.player_.playerShockAge_>=0 || scene_.enemies_.battleEnemy_>=0 || scene_.grid_.tileAt(scene_.player_.x_,scene_.player_.y_)!=ufo_.targetTile) { ufo_.clock=0; return; }
        scene_.player_.playerBeamState_=21;
        scene_.player_.playerBeamClock_=0;
        scene_.player_.playerLift_=0;
        scene_.items_.playerEffect_=0;
        scene_.player_.cancelPlayerStep();
    } else {
        actor.state=17;
        actor.lift=0;
        actor.age=0;
        actor.pulse=175;
        if(scene_.isAnimal(actor.kind)) {
            ufo_.collectAnimals=true;
            scene_.sounds_.push_back("Sven2/sfx/Sheep_beamup.ogg");
        } else if(actor.kind==3) scene_.addScore(75);
        else if(actor.kind==4) { scene_.addScore(50); scene_.sounds_.push_back("Sven2/sfx/wotan_beamup.ogg"); }
        else if(actor.kind==6) {
            scene_.addScore(25);
            ufo_.clock=400.0f/60;
            scene_.sounds_.push_back("Sven2/sfx/Broemse_beamup.ogg");
        } else if(actor.kind==5) scene_.sounds_.push_back("Sven2/sfx/Lars_angry.ogg");
    }
    ufo_.beam=true;
    ufo_.beamAge=0;
    scene_.sounds_.push_back("Sven2/sfx/beam.ogg");
}

void UfoController::updateUfo(float seconds) {
    if(scene_.player_.playerBeamState_) {
        scene_.player_.playerBeamClock_+=seconds;
        if(scene_.player_.playerBeamState_==21) {
            scene_.player_.playerLift_-=seconds*20;
            if(scene_.player_.playerLift_<-27) {
                scene_.player_.playerLift_=-27;
                scene_.player_.playerBeamState_=22;
                scene_.player_.playerBeamClock_=0;
            }
        } else if(scene_.player_.playerBeamClock_>2.5f) {
            scene_.player_.playerLift_+=seconds*60;
            if(scene_.player_.playerLift_>=0) {
                scene_.player_.playerLift_=0;
                scene_.player_.playerBeamState_=0;
                scene_.items_.playerEffect_=137;
                scene_.items_.effectRemaining_=scene_.parameter("AlienDuration",15);
                scene_.items_.effectAge_=0;
                scene_.sounds_.push_back("Sven2/sfx/sven_hits_ground.ogg");
            }
        }
    }
    if(ufo_.state<0) {
        if(scene_.expired_) {
            std::vector<int> candidates;
            for(size_t i=0;i<scene_.world_.size();++i) if(scene_.isAnimal(scene_.world_[i].kind) && !scene_.world_[i].freed) candidates.push_back(static_cast<int>(i));
            if(!candidates.empty()) requestUfo(candidates[scene_.randomInt(static_cast<int>(candidates.size()))]);
        }
        return;
    }
    ufo_.age+=seconds*20;
    if(ufo_.beam) {
        ufo_.beamAge+=seconds*15;
        if(static_cast<size_t>(ufo_.beamAge)>=actorFrameTables.at(AnimationSequence::ufoBeam).size()) ufo_.beam=false;
    }
    if(ufo_.state==0) {
        ufo_.angleX-=seconds*ufo_.angularSpeed;
        ufo_.angleY-=seconds*ufo_.angularSpeed;
        ufo_.angularSpeed=std::max(0.0f,ufo_.angularSpeed-seconds*0.0001f);
        ufo_.radius-=seconds*125;
        if(ufo_.radius<0) { ufo_.radius=0; ufo_.state=1; beginBeam(); }
    } else if(ufo_.state==1) {
        ufo_.clock-=seconds;
        if(ufo_.clock<0) {
            ufo_.angularSpeed=0.005f;
            ufo_.state=ufo_.collectAnimals?3:2;
        }
    } else if(ufo_.state==2) {
        ufo_.angleX+=seconds*ufo_.angularSpeed;
        ufo_.angleY+=seconds*ufo_.angularSpeed;
        ufo_.angularSpeed+=seconds*0.0002f;
        ufo_.radius+=seconds*175;
        if(ufo_.radius>1000) ufo_.state=-1;
    } else if(ufo_.state==3) {
        std::vector<int> candidates;
        for(size_t i=0;i<scene_.world_.size();++i) if(scene_.isAnimal(scene_.world_[i].kind) && !scene_.world_[i].freed) candidates.push_back(static_cast<int>(i));
        if(candidates.empty()) { ufo_.state=2; return; }
        ufo_.target=candidates[scene_.randomInt(static_cast<int>(candidates.size()))];
        const auto& actor=scene_.world_[ufo_.target];
        ufo_.targetTile=actor.tile;
        ufo_.targetX=actor.x+60;
        ufo_.targetY=actor.y+60;
        ufo_.vx=(ufo_.targetX-ufo_.x)*0.5f;
        ufo_.vy=(ufo_.targetY-ufo_.y)*0.5f;
        ufo_.state=4;
    } else if(ufo_.state==4) {
        auto advance=[seconds](float current,float target,float velocity) {
            const float next=current+velocity*seconds;
            return velocity>=0?std::min(next,target):std::max(next,target);
        };
        ufo_.x=advance(ufo_.x,ufo_.targetX,ufo_.vx);
        ufo_.y=advance(ufo_.y,ufo_.targetY,ufo_.vy);
        if(ufo_.x==ufo_.targetX && ufo_.y==ufo_.targetY) {
            ufo_.state=1;
            if(scene_.expired_) beginBeam();
            else { ufo_.collectAnimals=false; ufo_.clock=0; }
        }
    }
}
}
