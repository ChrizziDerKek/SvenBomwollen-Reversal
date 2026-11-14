#include "characters/PlayerCharacter.h"
#include "LevelScene.h"
#include "scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sven2 {
using namespace scene_data;
PlayerCharacter::PlayerCharacter(LevelScene& scene):scene_(scene) {}

void PlayerCharacter::markPlayerCell() {
    // 0x42C3F0 marks the committed base cell, not the interpolated center.
    // 0x42C470/0x42BF57 clear it only when the full 40-pixel step commits.
    if(x_!=targetX_ || y_!=targetY_) return;
    auto& actor=scene_.world_[scene_.playerIndex_];
    actor.x=x_; actor.y=y_;
    const int tile=scene_.grid_.tileAt(x_,y_);
    if(actor.tile!=tile) {
        if(actor.tile>=0 && scene_.grid_.tiles_[actor.tile]==100)
            scene_.grid_.tiles_[actor.tile]=scene_.grid_.tileDirections_[actor.tile]?wall:empty;
        actor.tile=tile;
    }
    if(tile>=0 && (scene_.grid_.tiles_[tile]==empty || scene_.grid_.tiles_[tile]==99 || scene_.grid_.tiles_[tile]==100)) scene_.grid_.tiles_[tile]=100;
}

void PlayerCharacter::cancelPlayerStep() {
    // Original 0x42C816 clears interpolation offsets, preserving the base cell.
    const int destination=scene_.grid_.tileAt(targetX_,targetY_);
    if(destination>=0 && scene_.grid_.tiles_[destination]==99) scene_.grid_.tiles_[destination]=empty;
    const auto& player=scene_.world_[scene_.playerIndex_];
    x_=targetX_=player.x;
    y_=targetY_=player.y;
    markPlayerCell();
}

void PlayerCharacter::teleport(int source) {
    const auto* sprite=scene_.effects_.splashes_[0].sprite;
    auto splashAt=[sprite](int tile) {
        return SpriteDraw{sprite, tile%stride*tileSize-70, tile/stride*tileSize-65, 0};
    };
    scene_.effects_.splashes_[0]=splashAt(source);
    scene_.effects_.splashes_[1]={};
    scene_.effects_.splashAge_=0;
    scene_.effects_.splashSound_=true;
    scene_.sounds_.push_back("Sven2/sfx/water_splash22.ogg");
    scene_.effects_.splashPlayerAge_=0;
    if(scene_.grid_.puddles_.empty()) return;
    std::array<int,16> destinations{};
    size_t count=0;
    for(int tile:scene_.grid_.puddles_)
        if(tile!=source || scene_.grid_.puddles_.size()==1) destinations[count++]=tile;
    int target=destinations[scene_.randomInt(static_cast<int>(count))];
    for(const auto& direction:directions) {
        int x=target%stride-1+direction[0], y=target/stride-1+direction[1];
        if(x<0 || y<0 || x>=columns || y>=rows || !scene_.grid_.unoccupied((y+1)*stride+x+1,scene_.playerIndex_)) continue;
        x_=static_cast<float>(x*tileSize+tileSize/2)-playerCenter;
        y_=static_cast<float>(y*tileSize+tileSize/2)-playerCenter;
        targetX_=x_;
        targetY_=y_;
        scene_.world_[scene_.playerIndex_].committedY=y_;
        scene_.effects_.splashes_[1]=splashAt(target);
        log(std::string(scene_.grid_.tiles_[source]==water?"Water entry: ":"Puddle teleport: ")+
            std::to_string(source)+" -> "+std::to_string(target));
        return;
    }
}

void PlayerCharacter::restorePlayer() {
    auto& draw=scene_.world_[scene_.playerIndex_].draw;
    draw={scene_.resources_.stand_,static_cast<int>(x_),static_cast<int>(y_+playerLift_),facing_};
    if(playerShockAge_>=0) {
        const auto& frames=actorFrameTables.at(AnimationSequence::playerElectricShock);
        draw={scene_.resources_.electric_[facing_],static_cast<int>(x_),static_cast<int>(y_),frames[std::min(frames.size()-1,static_cast<size_t>(playerShockAge_))],svenMirror[facing_]};
        return;
    }
    if(playerBeamState_) {
        if(playerBeamState_==22 && playerBeamClock_<=2.5f) { draw={}; return; }
        const int frame=playerBeamState_==21?std::min(21,static_cast<int>(playerBeamClock_*10)):
            std::min(7,3+static_cast<int>((playerBeamClock_-2.5f)*10));
        draw={scene_.resources_.playerBeamSprites_[facing_/2],static_cast<int>(x_),static_cast<int>(y_+playerLift_),frame,facing_/2==1 || facing_/2==3};
    }
}

void PlayerCharacter::act(bool continuation) {
    const int tile=scene_.grid_.tileAt(x_+directions[facing_][0]*40,y_+directions[facing_][1]*40);
    for(size_t i=0;i<scene_.world_.size();++i) {
        auto& animal=scene_.world_[i];
        if(!scene_.isAnimal(animal.kind) || animal.freed || animal.tile!=tile || animal.state==4 || animal.state==3 || (animal.state>=11 && animal.state<=14)) continue;
        activeSheep_=static_cast<int>(i); actionAge_=0;
        // 0x42B730: each sheep direction accepts three of Sven's eight directions.
        const int difference=(facing_-2*animal.facing+8)%8;
        if(difference>2) {
            static constexpr int turn[8]={0,0,0,1,1,2,2,3}; // 0x44478C
            animal.turnTo=turn[facing_]; animal.state=3; animal.age=0; animal.idle=75;
            playerAction_=2;
            scene_.sounds_.push_back("Sven2/sfx/Sven_whistle.ogg");
            return;
        }
        if(animal.kind==2 && animal.impatience==10 && animal.temperament!=2) {
            playerAction_=3; scene_.effects_.addTime(-10,animal.x+60,animal.y+60);
            scene_.sounds_.push_back("Sven2/sfx/Sheep_kickSven.ogg"); return;
        }
        if(animal.kind==2 && animal.temperament==1) { animal.impatience=0; animal.temperament=2; }
        if(!continuation) ++animal.attempts;
        if(animal.kind==2 && animal.temperament==2) {
            playerAction_=4; scene_.sounds_.push_back("Sven2/sfx/Frigida_kettle.ogg");
            scene_.world_[scene_.playerIndex_].draw={scene_.resources_.frigida_[animal.facing],static_cast<int>(animal.x),static_cast<int>(animal.y),frigidaFrames[0],sheepMirror[animal.facing]};
            return;
        }
        if(animal.kind>=11) {
            interaction_=&scene_.resources_.animalActions_[animal.kind-11][animal.facing];
            playerAction_=1;
            scene_.sounds_.push_back(interaction_->sound);
            scene_.world_[scene_.playerIndex_].draw={interaction_->sprite,static_cast<int>(animal.x),static_cast<int>(animal.y),interaction_->frames.front(),interaction_->mirror};
            return;
        }
        std::vector<size_t> choices;
        for(size_t j=0;j<scene_.resources_.interactions_.size();++j) if(scene_.resources_.interactions_[j].facing==animal.facing) choices.push_back(j);
        if(choices.empty()) { activeSheep_=-1; return; }
        // 0x42B09C..0x42B0EE chooses only after movement invalidates the variant.
        if(selectedInteraction_<0 || scene_.resources_.interactions_[selectedInteraction_].facing!=animal.facing)
            selectedInteraction_=static_cast<int>(choices[scene_.randomInt(static_cast<int>(choices.size()))]);
        interaction_=&scene_.resources_.interactions_[selectedInteraction_];
        playerAction_=1;
        scene_.sounds_.push_back(interaction_->sound);
        scene_.world_[scene_.playerIndex_].draw={interaction_->sprite,static_cast<int>(animal.x),static_cast<int>(animal.y),interaction_->frames.front(),interaction_->mirror};
        return;
    }
}

void PlayerCharacter::finishInteraction() {
    auto& animal=scene_.world_[activeSheep_];
    scene_.addScore(scene_.items_.playerEffect_==132?3:scene_.items_.playerEffect_==134?0:scene_.items_.playerEffect_==137?10:1); // 0x429B7E
    animal.impatience-=2;
    if(animal.impatience<0) scene_.animals_.resolveSheep(animal);
    playerAction_=0; activeSheep_=-1; interaction_=nullptr; restorePlayer();
    if(actionQueued_ && !animal.freed) { actionQueued_=false; act(true); }
}

void PlayerCharacter::advancePlayer(float dt,int dx,int dy) {
    if(playerShockAge_>=0) {
        playerShockAge_+=dt*defaultRate_;
        if(playerShockAge_>=actorFrameTables.at(AnimationSequence::playerElectricShock).size()) playerShockAge_=-1;
        restorePlayer();
        return;
    }
    if(playerBeamState_) { restorePlayer(); return; }
    if(scene_.enemies_.battleEnemy_>=0) return;
    scene_.items_.pickupPlayerItem();
    if(playerAction_) {
        actionAge_+=dt;
        if(playerAction_==1 && interaction_) {
            const float actionRate=scene_.items_.playerEffect_==132?scene_.parameter("SvenShagFireworkFrameRate",30):scene_.items_.playerEffect_==134?scene_.parameter("SvenShagImpotentFrameRate",12):scene_.items_.playerEffect_==137?scene_.parameter("SvenShagAlienFrameRate",40):actionRate_;
            int frame=static_cast<int>(actionAge_*actionRate);
            if(frame>=static_cast<int>(interaction_->frames.size())) finishInteraction();
            else {
                auto& animal=scene_.world_[activeSheep_];
                scene_.world_[scene_.playerIndex_].draw={interaction_->sprite,static_cast<int>(animal.x),static_cast<int>(animal.y),interaction_->frames[frame],interaction_->mirror};
            }
        } else if(playerAction_==3 && actionAge_*defaultRate_<scene_.resources_.kicks_[scene_.world_[activeSheep_].facing]->rows) {
            const auto& animal=scene_.world_[activeSheep_];
            scene_.world_[scene_.playerIndex_].draw={scene_.resources_.kicks_[animal.facing],static_cast<int>(animal.x),static_cast<int>(animal.y),static_cast<int>(actionAge_*defaultRate_),sheepMirror[animal.facing]};
        } else if(playerAction_==4 && actionAge_*frigidaRate_<7) {
            const auto& animal=scene_.world_[activeSheep_];
            scene_.world_[scene_.playerIndex_].draw={scene_.resources_.frigida_[animal.facing],static_cast<int>(animal.x),static_cast<int>(animal.y),frigidaFrames[static_cast<int>(actionAge_*frigidaRate_)],sheepMirror[animal.facing]};
        } else if(playerAction_==2 && actionAge_*defaultRate_<22) {
            int frame=std::min(21,static_cast<int>(actionAge_*defaultRate_));
            scene_.world_[scene_.playerIndex_].draw={scene_.resources_.whistles_[facing_],static_cast<int>(x_),static_cast<int>(y_),facing_%2?whistleDiagonal[frame]:whistleStraight[frame],svenMirror[facing_]};
        } else {
            if(playerAction_==3) {
                // Player state 13 at 0x42A283 releases the sheep at mood 6.
                scene_.world_[activeSheep_].impatience=6;
                actionQueued_=false;
            }
            if(playerAction_==4) {
                auto& animal=scene_.world_[activeSheep_];
                ++animal.impatience;
                if(animal.impatience==6 || animal.impatience==10) scene_.sounds_.push_back("Sven2/sfx/Frigida.ogg");
                if(animal.impatience==11) scene_.animals_.resolveSheep(animal);
            }
            playerAction_=0; activeSheep_=-1; restorePlayer();
            if(actionQueued_) { actionQueued_=false; act(true); }
        }
        return;
    }
    if(scene_.effects_.splashPlayerAge_*defaultRate_<16) {
        scene_.world_[scene_.playerIndex_].draw={scene_.resources_.blinks_[facing_],static_cast<int>(x_),static_cast<int>(y_),splashPlayerFrames[static_cast<int>(scene_.effects_.splashPlayerAge_*defaultRate_)],svenMirror[facing_]};
        return;
    } // 0x409A4C / 0x440158, independently of the splash effect lifetime.
    restorePlayer();
    if(scene_.completed()) { actionQueued_=false; return; }
    const bool centered=x_==targetX_ && y_==targetY_;
    int facing=0;
    while(facing<8 && (dx!=directions[facing][0] || dy!=directions[facing][1])) ++facing;
    if(centered && facing<8) { facing_=facing; selectedInteraction_=-1; }
    if(centered && actionQueued_) { actionQueued_=false; act(); if(playerAction_) return; }
    if(centered) {
        if(facing==8) { animationAge_=0; restorePlayer(); return; }
        const int current=scene_.grid_.tileAt(x_,y_);
        if(current<0) return;
        auto direction=tileDirection[facing_];
        auto sourceDirection=scene_.grid_.tileDirections_[current];
        if(sourceDirection && sourceDirection!=direction) return;
        const float nextX=x_+dx*tileSize,nextY=y_+dy*tileSize;
        int destination=scene_.grid_.tileAt(nextX,nextY);
        if(destination<0) return;
        int kind=scene_.grid_.tiles_[destination];
        if(kind==puddle || kind==water) { teleport(destination); restorePlayer(); return; }
        if(!scene_.grid_.unoccupied(destination,scene_.playerIndex_) && !(kind==wall && scene_.grid_.tileDirections_[destination]==direction)) return;
        if(kind==empty) scene_.grid_.tiles_[destination]=99; // 0x42A94B: reserve the proposed destination.
        targetX_=nextX; targetY_=nextY;
    }
    const float vx=targetX_-x_,vy=targetY_-y_;
    const float distance=std::max(std::abs(vx),std::abs(vy));
    const float currentSpeed=scene_.items_.playerEffect_==132 || scene_.items_.playerEffect_==137?scene_.parameter("SvenRunSpeed",220):scene_.items_.playerEffect_==134?scene_.parameter("SvenSlowSpeed",95):speed_;
    const float rate=currentSpeed*(vx!=0 && vy!=0?diagonal_:1.0f);
    const float fraction=distance>0?std::min(1.0f,dt*rate/distance):1;
    x_+=vx*fraction; y_+=vy*fraction;
    if(x_==targetX_ && y_==targetY_) {
        markPlayerCell();
        scene_.world_[scene_.playerIndex_].committedY=y_;
        scene_.items_.affectNearbySheep(); // 0x42BF9E applies smell/flowers at tile arrival.
    }
    scene_.items_.pickupPlayerItem();
    animationAge_+=dt;
    auto& draw=scene_.world_[scene_.playerIndex_].draw;
    draw={scene_.resources_.walks_[facing_],static_cast<int>(x_),static_cast<int>(y_),static_cast<int>(animationAge_*(scene_.items_.playerEffect_==132?scene_.parameter("SvenWalkFireworkFrameRate",36):scene_.items_.playerEffect_==134?scene_.parameter("SvenWalkImpotentFrameRate",16):scene_.items_.playerEffect_==137?scene_.parameter("SvenWalkAlienFrameRate",36):walkRate_))%14,svenMirror[facing_]};
}
}
