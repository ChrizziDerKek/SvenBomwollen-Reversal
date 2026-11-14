#include "characters/EnemyCharacters.h"
#include "LevelScene.h"
#include "scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sven2 {
using namespace scene_data;
EnemyCharacters::EnemyCharacters(LevelScene& scene):scene_(scene) {}

bool EnemyCharacters::playerInRange(const Object& actor,float distance) const {
    if(battleEnemy_>=0 || scene_.player_.playerBeamState_ || scene_.player_.playerShockAge_>=0 || scene_.effects_.splashPlayerAge_*scene_.player_.defaultRate_<16) return false;
    if(actor.tile<0) return false;
    const int playerTile=scene_.grid_.tileAt(scene_.player_.x_,scene_.player_.y_);
    const int dx=actor.tile%stride-playerTile%stride;
    const int dy=actor.tile/stride-playerTile/stride;
    const int bound=static_cast<int>(distance);
    return playerTile>=0 && dx>-bound && dx<bound && dy>-bound && dy<bound;
}

bool EnemyCharacters::canCatchPlayer(const Object& actor) const {
    if(battleEnemy_>=0 || scene_.player_.playerBeamState_ || scene_.player_.playerShockAge_>=0 || scene_.effects_.splashPlayerAge_*scene_.player_.defaultRate_<16) return false;
    const float dx=scene_.player_.x_+60-actor.x-actor.centerX;
    const float dy=scene_.player_.y_+60-actor.y-actor.centerY;
    if(dx*dx+dy*dy>55*55) return false;
    const int playerTile=scene_.world_[scene_.playerIndex_].tile;
    for(int offset=-1;offset<=1;++offset) {
        const auto& d=directions[(actor.facing+offset+8)%8];
        if(actor.tile+d[0]+stride*d[1]==playerTile) return true;
    }
    return false;
}

void EnemyCharacters::enemyIdle(Object& actor,bool retry) {
    const auto& rules=scene_.resources_.enemyRules_[actor.kind];
    actor.state=0;
    actor.age=0;
    actor.hunting=false;
    actor.itemTarget=-1;
    actor.retry=retry;
    if(actor.kind==3) { actor.idleAnimating=false; actor.idle=static_cast<float>(50+scene_.randomInt(100)); }
    actor.pulse=rules.idle+(actor.kind==5 || actor.kind==6?rules.idleRandom:scene_.randomInt(static_cast<int>(rules.idleRandom)));
    actor.targetX=actor.x;
    actor.targetY=actor.y;
}

void EnemyCharacters::enemyNotice(const Object& actor,bool item) {
    scene_.emitNamedSound(actor.kind==3?"SfxAlienFoundSven":
        actor.kind==4?(item?"SfxDogFoundBone":"SfxDogFoundSven"):
        actor.kind==5?(item?"SfxGuyFoundBeer":"SfxGuyFoundSven"):
        (item?"SfxBroemseFoundTeddy":"SfxBroemseFoundSven"));
}

bool EnemyCharacters::chooseEnemyStep(size_t index) {
    auto& actor=scene_.world_[index];
    int preferred=actor.facing;
    if(actor.hunting) {
        int target=scene_.grid_.tileAt(scene_.player_.x_,scene_.player_.y_);
        if(actor.itemTarget>=0 && !scene_.world_[actor.itemTarget].freed) target=scene_.world_[actor.itemTarget].tile;
        else if(battleEnemy_>=0 || scene_.player_.playerBeamState_ || scene_.effects_.splashPlayerAge_*scene_.player_.defaultRate_<16) actor.hunting=false;
        if(actor.hunting && target>=0) preferred=enemyDirection(actor.tile,target);
    }
    auto attempt=[&](int direction) {
        const auto& d=directions[direction];
        int x=actor.tile%stride+d[0],y=actor.tile/stride+d[1];
        // Original helpers allow column 21 / row 16 if their border cell is empty.
        if(x<1 || x>columns+1 || y<1 || y>rows+1) return false;
        const int tile=y*stride+x;
        if(scene_.grid_.tiles_[tile]!=empty || scene_.grid_.reservations_[tile]>=0) return false;
        // Rejected attempts have no facing, velocity, or reservation side effects.
        actor.facing=direction;
        actor.targetX=actor.x+40*d[0];
        actor.targetY=actor.y+40*d[1];
        scene_.grid_.reservations_[tile]=static_cast<int>(index);
        return true;
    };
    if(actor.hunting && actor.kind!=5 && attempt(preferred)) return true;
    for(int tries=0;tries<128;++tries)
        if(attempt(steering[actor.kind-3][preferred][scene_.randomInt(39)])) return true;
    return false;
}

void EnemyCharacters::drawEnemy(Object& actor) {
    const auto& images=scene_.resources_.actorSprites_[actor.kind];
    const Sprite* image=images.stand;
    int frame=actor.facing;
    int x=static_cast<int>(actor.x),y=static_cast<int>(actor.y+actor.lift);
    bool mirror=false;
    const int quadrant=actor.facing/2;
    if(actor.kind==3 && actor.state==0 && actor.idleAnimating) {
        image=images.idle[actor.facing]; frame=actor.draw.frame; mirror=enemyWalkMirror[0][actor.facing];
    } else if(actor.state==1) {
        image=images.walk[actor.facing];
        frame=actor.kind==6?actorFrameTables.at(AnimationSequence::girlWalk)[static_cast<size_t>(actor.age)%actorFrameTables.at(AnimationSequence::girlWalk).size()]:static_cast<int>(actor.age)%image->rows;
        mirror=enemyWalkMirror[actor.kind-3][actor.facing];
    } else if(actor.kind==5 && (actor.state==14 || actor.state==15)) {
        image=images.grumble[quadrant]; frame=actor.draw.frame; mirror=quadrant>=2;
    } else if(actor.kind==6 && (actor.state==22 || actor.state==23)) {
        image=images.dogPlay[quadrant]; frame=actor.draw.frame; mirror=quadrant<2;
    } else if(actor.state==10) {
        image=images.sleep[quadrant];
        if(actor.kind==5) {
            const float bodyAge=std::fmod(actor.age,28.0f);
            frame=bodyAge>14?14:13;
            x-=50; y-=20;
        } else {
            frame=0;
        }
        mirror=actor.kind==4?(quadrant==0 || quadrant==2):quadrant<2;
    } else if(actor.state==11 && actor.kind==5) {
        image=images.wake[quadrant];
        frame=actor.draw.frame;
        x-=50; y-=20;
        mirror=quadrant>=2;
    } else if(actor.state==13 && actor.kind==5) {
        image=images.sleep[quadrant];
        frame=actor.draw.frame;
        x-=50; y-=20;
        mirror=quadrant<2;
    } else if(actor.kind==4 && actor.state==12) {
        image=images.sleep[quadrant]; frame=actor.draw.frame; mirror=quadrant==0 || quadrant==2;
    } else if(actor.state==12 || actor.state==13) {
        if(images.distraction[quadrant]) {
            image=images.distraction[quadrant];
            frame=actor.draw.frame;
            mirror=actor.kind==6?(quadrant==0 || quadrant==1):quadrant>=2;
        }
    } else if(actor.state==17 && images.beam[quadrant]) {
        image=images.beam[quadrant];
        frame=std::min(image->rows-1,static_cast<int>(actor.age*(actor.kind==4 || actor.kind==6?4:15)));
        mirror=actor.kind==6?quadrant<2:quadrant>=2;
    } else if(actor.state==20 || actor.state==21) {
        if(images.battle[quadrant]) {
            image=images.battle[quadrant];
            frame=actor.draw.frame;
            mirror=actor.kind==6?quadrant<2:quadrant>=2;
            if(actor.kind==4 || actor.kind==5) { x-=30; y-=20; mirror=false; }
            if(actor.kind==6 && actor.state==21) x-=20;
        }
    }
    if((actor.kind==6 && actor.state==18) || (actor.kind==4 && actor.state==13)) { actor.draw={}; return; }
    if(image) actor.draw={image,x,y,std::clamp(frame,0,image->rows*image->columns-1),mirror};
}

void EnemyCharacters::enemyArrived(size_t index) {
    auto& actor=scene_.world_[index];
    const int code=actor.tile>=0?scene_.grid_.itemCodes_[actor.tile]:0;
    const int distraction=actor.kind==4?129:actor.kind==5?130:actor.kind==6?131:0;
    if(code==128 || (distraction && code==distraction)) {
        scene_.items_.removeItem(actor.tile);
        actor.age=0;
        actor.hunting=false;
        actor.itemTarget=-1;
        if(code==128) {
            actor.state=16;
            scene_.ufoController_.requestUfo(static_cast<int>(index));
        } else if(actor.kind==4) {
            actor.state=12;
            actor.pulse=scene_.parameter("DogDiggingTime",15);
            actor.idle=0; actor.draw.frame=0;
            scene_.effects_.spawnDigEffect(actor);
            scene_.emitNamedSound("SfxDogDig");
        } else if(actor.kind==5) {
            actor.state=12;
            actor.animRate=10;
            scene_.emitNamedSound("SfxLarsDrink");
        } else {
            actor.state=(actor.facing>=2 && actor.facing<=5)?13:12;
            actor.facing=2*(actor.facing/2)+1;
            actor.animRate=9;
            scene_.emitNamedSound("SfxBroemsePlaysTeddy");
        }
        return;
    }
    // 0x405C50 selects the last dog in facing, -1, +1 order and stuns it.
    if(actor.kind==6) {
        int dog=-1;
        for(int offset:{0,-1,1}) {
            const auto& d=directions[(actor.facing+offset+8)%8];
            const int tile=actor.tile+d[0]+stride*d[1];
            for(size_t j=0;j<scene_.world_.size();++j)
                if(!scene_.world_[j].freed && scene_.world_[j].kind==4 && scene_.world_[j].tile==tile) dog=static_cast<int>(j);
        }
        if(dog>=0) {
            auto& target=scene_.world_[dog];
            if(target.state!=10 && target.state!=12 && target.state!=13 && target.state!=16 && target.state!=17 && target.state!=20) {
                for(auto& reservation:scene_.grid_.reservations_) if(reservation==dog) reservation=-1;
                target.x=target.targetX=static_cast<float>((target.tile%stride-1)*40+20)-60;
                target.y=target.targetY=static_cast<float>((target.tile/stride-1)*40+20)-60;
                target.state=13; target.pulse=6;
                actor.state=actor.facing>=2 && actor.facing<=5?23:22;
                actor.age=0; actor.animRate=10;
                scene_.emitNamedSound("SfxBroemsePlaysWotan");
                return;
            }
        }
    }
    if(canCatchPlayer(actor)) { beginBattle(index); return; }
    const bool activity=scene_.player_.playerAction_==1 || scene_.player_.playerAction_==4;
    // Lars sometimes gives up with a directional grumble once Sven stops.
    if(actor.kind==5 && actor.hunting && actor.itemTarget<0 && !activity && scene_.randomInt(2)==0) {
        const int tile=scene_.grid_.tileAt(scene_.player_.x_,scene_.player_.y_);
        if(tile>=0) actor.facing=enemyDirection(actor.tile,tile);
        actor.steps=scene_.resources_.enemyRules_[5].walkSteps+scene_.randomInt(scene_.resources_.enemyRules_[5].walkRandom);
        actor.state=actor.facing>=2 && actor.facing<=5?15:14;
        actor.hunting=false; actor.age=0; actor.animRate=10;
        scene_.emitNamedSound("SfxLarsGroan");
        return;
    }
    if(actor.itemTarget<0 && distraction) {
        actor.itemTarget=scene_.items_.findDistraction(actor,distraction);
        if(actor.itemTarget>=0) {
            actor.hunting=true;
            actor.steps=scene_.resources_.enemyRules_[actor.kind].huntSteps;
            enemyNotice(actor,true);
        }
    }
    if(!actor.hunting && ((actor.kind==5 && activity) ||
       (actor.kind==4 && (activity || playerInRange(actor,scene_.resources_.enemyRules_[4].range))) ||
       ((actor.kind==3 || actor.kind==6) && playerInRange(actor,scene_.resources_.enemyRules_[actor.kind].range)))) {
        actor.hunting=true;
        actor.steps=scene_.resources_.enemyRules_[actor.kind].huntSteps;
        enemyNotice(actor,false);
    }
    if(--actor.steps==0 || !chooseEnemyStep(index)) {
        // 0x4022F0/0x4038F0/0x4063C0 draw the next walk budget before idle time.
        if(actor.kind!=3) actor.steps=scene_.resources_.enemyRules_[actor.kind].walkSteps+scene_.randomInt(scene_.resources_.enemyRules_[actor.kind].walkRandom);
        enemyIdle(actor);
    }
}

void EnemyCharacters::updateEnemies(float seconds) {
    for(size_t i=0;i<scene_.world_.size();++i) {
        auto& actor=scene_.world_[i];
        if(actor.freed || !scene_.isEnemy(actor.kind)) continue;
        const auto& rules=scene_.resources_.enemyRules_[actor.kind];
        if(actor.state==10) {
            actor.age+=seconds*(actor.kind==5?actor.animRate:10);
            if(actor.kind==5) {
                actor.soundClock-=60*seconds;
                if(actor.soundClock<0) { actor.soundClock=300; scene_.emitNamedSound("SfxLarsSleep"); }
            }
            if(playerInRange(actor,rules.wakeRange)) actor.pulse=0;
            actor.pulse-=seconds;
            if(actor.pulse<0) {
                actor.age=0;
                actor.retry=false;
                if(actor.kind==5) {
                    actor.state=11;
                    actor.animRate=10;
                    actor.facing=std::min(7,actor.facing+1);
                } else enemyIdle(actor);
            }
        } else if(actor.state==11 && actor.kind==5) {
            if(scene_.advanceActor(actor,AnimationSequence::farmerStandUp,actor.animRate,seconds)) enemyIdle(actor);
        } else if(actor.state==0) {
            if(actor.kind==3) {
                if(!actor.idleAnimating) {
                    actor.idle-=60*seconds;
                    if(actor.idle<0) { actor.idleAnimating=true; actor.idleVariant=scene_.randomInt(2); actor.age=0; }
                }
                if(actor.idleAnimating) {
                    const AnimationSequence table=actor.idleVariant?(actor.facing==0 || actor.facing==4?AnimationSequence::alienVerticalIdle:AnimationSequence::alienDiagonalIdle):AnimationSequence::alienStandingIdle;
                    if(scene_.advanceActor(actor,table,5,seconds)) { actor.idleAnimating=false; actor.idle=static_cast<float>(50+scene_.randomInt(100)); }
                }
            }
            const int code=actor.kind==4?129:actor.kind==5?130:actor.kind==6?131:0;
            const bool activity=scene_.player_.playerAction_==1 || scene_.player_.playerAction_==4;
            if(!actor.retry) {
                if(code) actor.itemTarget=scene_.items_.findDistraction(actor,code);
                bool detect=actor.kind==5?activity:playerInRange(actor,rules.range);
                if(actor.itemTarget>=0 || detect) {
                    actor.hunting=true; actor.pulse=0;
                    enemyNotice(actor,actor.itemTarget>=0);
                }
            }
            actor.pulse-=seconds;
            if(actor.pulse<0) {
                actor.retry=false;
                // The chooser must consume its random draws BEFORE the walk budget.
                // A failed chooser consumes no walk-budget draw at all.
                if(chooseEnemyStep(i)) {
                    actor.steps=actor.hunting?rules.huntSteps:rules.walkSteps+scene_.randomInt(rules.walkRandom);
                    if(!actor.hunting) actor.itemTarget=-1;
                    actor.state=1; actor.age=0;
                }
                else {
                    // Failed idle chooser keeps pursuit/item link; byte65 gates notice.
                    actor.retry=true;
                    actor.pulse=rules.idle+(actor.kind==5 || actor.kind==6?rules.idleRandom:scene_.randomInt(static_cast<int>(rules.idleRandom)));
                }
            }
        } else if(actor.state==1) {
            const float vx=actor.targetX-actor.x,vy=actor.targetY-actor.y;
            const float distance=std::max(std::abs(vx),std::abs(vy));
            const float speed=rules.speed*(actor.hunting?rules.run:1)*(vx!=0 && vy!=0?rules.diagonal:1);
            const float fraction=distance?std::min(1.0f,seconds*speed/distance):1;
            actor.x+=vx*fraction;
            actor.y+=vy*fraction;
            actor.age+=seconds*(actor.hunting?rules.runRate:rules.walkRate);
            // 0x40B920/0x40B990 discard fractional overflow at sequence end;
            // a successful tile-to-tile chooser preserves the current phase.
            const size_t walkLength=actor.kind==6?actorFrameTables.at(AnimationSequence::girlWalk).size():
                static_cast<size_t>(scene_.resources_.actorSprites_[actor.kind].walk[actor.facing]->rows);
            if(actor.age>=walkLength) actor.age=0;
            if(fraction==1) {
                const auto& d=directions[actor.facing];
                int target=actor.tile+d[0]+stride*d[1];
                if(actor.tile>=0 && scene_.grid_.tiles_[actor.tile]==scene_.occupancyKind(actor.kind)) scene_.grid_.tiles_[actor.tile]=empty;
                actor.tile=target;
                actor.committedY=actor.y;
                scene_.grid_.tiles_[target]=scene_.occupancyKind(actor.kind);
                scene_.grid_.reservations_[target]=-1;
                enemyArrived(i);
            }
        } else if(actor.kind==5 && (actor.state==14 || actor.state==15)) {
            if(scene_.advanceActor(actor,actor.state==14?AnimationSequence::farmerGrumbleBack:AnimationSequence::farmerGrumbleFront,actor.animRate,seconds)) enemyIdle(actor);
        } else if(actor.kind==6 && (actor.state==22 || actor.state==23)) {
            if(scene_.advanceActor(actor,actor.state==22?AnimationSequence::girlDogBack:AnimationSequence::girlDogFront,actor.animRate,seconds)) enemyIdle(actor,true);
        } else if(actor.state==12 || actor.state==13) {
            bool done=false;
            if(actor.kind==4) {
                actor.draw.frame=0;
                actor.idle-=60*seconds;
                if(actor.idle<0) { actor.idle=10; actor.draw.frame=2; }
                actor.pulse-=seconds; done=actor.pulse<0;
            }
            else if(actor.kind==5) done=scene_.advanceActor(actor,actor.state==12?AnimationSequence::farmerDrink:AnimationSequence::farmerLieDown,actor.animRate,seconds);
            else if(actor.kind==6) done=scene_.advanceActor(actor,actor.state==12?AnimationSequence::girlTeddyBack:AnimationSequence::girlTeddyFront,actor.animRate,seconds);
            if(done) {
                if(actor.kind==5 && actor.state==12) { actor.state=13; actor.age=0; actor.animRate=10; }
                else if(actor.kind==5) {
                    actor.state=10;
                    actor.pulse=rules.sleep;
                    actor.facing=2*(actor.facing/2);
                    actor.age=0; actor.soundClock=0;
                    scene_.emitNamedSound("SfxLarsFall");
                } else enemyIdle(actor,actor.kind==6 && actor.state==13);
            }
        } else if(actor.state==16) {
            scene_.ufoController_.requestUfo(static_cast<int>(i));
        } else if(actor.state==17) {
            actor.age+=seconds;
            if(actor.kind==6) {
                actor.lift-=20*seconds;
                if(actor.lift<-27) {
                    actor.lift=0; actor.state=18; actor.pulse=4.5f; actor.age=0;
                    actor.retry=true; actor.hunting=false;
                    scene_.emitNamedSound("SfxBroemsePlayAliens");
                }
            } else if(actor.kind==5) {
                actor.pulse-=60*seconds;
                if(actor.pulse<0) enemyIdle(actor,true);
            } else {
                actor.lift-=seconds*20;
                if(actor.lift<-27) { scene_.removeActor(i,false); continue; }
            }
        } else if(actor.kind==6 && actor.state==18) {
            actor.pulse-=seconds;
            if(actor.pulse<0) {
                actor.state=19; actor.lift=-27; actor.age=0;
                actor.idle=0; actor.pulse=rules.idle+rules.idleRandom;
                actor.retry=true; actor.hunting=false;
            }
        } else if(actor.kind==6 && actor.state==19) {
            actor.lift+=20*seconds;
            if(actor.lift>0) { actor.lift=0; enemyIdle(actor,true); scene_.emitNamedSound("SfxSvenFall"); }
        } else if(actor.state==20 || actor.state==21) {
            bool done=false;
            if(actor.kind==3) done=scene_.advanceActor(actor,AnimationSequence::alienProbe,13,seconds);
            else if(actor.kind==6) done=scene_.advanceActor(actor,actor.state==20?AnimationSequence::girlSvenBack:AnimationSequence::girlSvenFront,actor.animRate,seconds);
            else {
                actor.age+=seconds*15;
                actor.draw.frame=static_cast<int>(actor.age)%scene_.resources_.actorSprites_[actor.kind].battle[actor.facing/2]->rows;
                actor.pulse-=seconds; done=actor.pulse<0;
            }
            if(done) finishBattle(actor);
        }
        drawEnemy(actor);
    }
}

void EnemyCharacters::beginBattle(size_t index) {
    auto& actor=scene_.world_[index];
    battleEnemy_=static_cast<int>(index);
    battleRemaining_=scene_.resources_.enemyRules_[actor.kind].fightTime;
    actor.state=20;
    actor.pulse=battleRemaining_;
    actor.age=0;
    actor.hunting=false;
    actor.draw.frame=0;
    if(actor.kind==6) actor.animRate=20;
    scene_.player_.playerAction_=0;
    scene_.player_.activeSheep_=-1;
    scene_.player_.interaction_=nullptr;
    scene_.player_.actionQueued_=false;
    scene_.player_.cancelPlayerStep();
    scene_.items_.playerEffect_=0; scene_.items_.effectRemaining_=0;
    const char* sound=actor.kind==3?"alien_analprobe_mix2.ogg":actor.kind==4?"fightdog.ogg":actor.kind==5?"fightguy.ogg":"broemse_plays_sven.ogg";
    scene_.sounds_.push_back(std::string("Sven2/sfx/")+sound);
    if(actor.kind==6) {
        actor.state=(actor.facing>=2 && actor.facing<=5)?21:20;
        scene_.effects_.addTime(-20,actor.x+actor.centerX,actor.y+actor.centerY);
    }
}

void EnemyCharacters::finishBattle(Object& actor) {
    if(actor.kind!=6) scene_.recordLifeLoss();
    battleEnemy_=-1;
    enemyIdle(actor,true);
    scene_.player_.restorePlayer();
}
}
