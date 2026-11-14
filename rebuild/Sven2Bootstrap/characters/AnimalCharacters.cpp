#include "characters/AnimalCharacters.h"
#include "LevelScene.h"
#include "scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sven2 {
using namespace scene_data;
AnimalCharacters::AnimalCharacters(LevelScene& scene):CharacterController(scene) {}

void AnimalCharacters::updateOtherAnimals(float seconds) {
    for(size_t i=0;i<scene_.world_.size();++i) {
        auto& actor=scene_.world_[i];
        if(actor.freed || actor.kind<11 || actor.kind>13) continue;
        if(static_cast<int>(i)==scene_.player_.activeSheep_ && scene_.player_.playerAction_!=2) continue;
        if(actor.state==17) {
            actor.lift-=20*seconds;
            if(actor.lift<-27) { scene_.removeActor(i,false); continue; }
        } else if(actor.state==3) {
            actor.idle-=60*seconds;
            if(actor.idle<0) { actor.facing=actor.turnTo; actor.turnTo=-1; actor.state=0; }
        }
        if(actor.state==0 || actor.state==3) {
            actor.pulse-=60*seconds;
            if(actor.pulse<0) { actor.pulse=26; actor.moodBlink^=1; }
        }
        actor.draw={scene_.resources_.actorSprites_[actor.kind].stand,static_cast<int>(actor.x),static_cast<int>(actor.y+actor.lift),actor.facing};
    }
}

void AnimalCharacters::resolveSheep(Object& animal) {
    // 0x407911..0x407B08: first resolved Frigida is special; otherwise first
    // attempt earns 15, a later attempt 10. Time bonuses are not score awards.
    const bool special=animal.kind==2 && scene_.completedSheep_==0 && animal.temperament==2;
    if(animal.kind!=2) { scene_.addScore(10); scene_.effects_.addTime(10,animal.x+60,animal.y+60); }
    else if(special) { scene_.addScore(20); scene_.effects_.addTime(10,animal.x+60,animal.y+60); }
    else { scene_.addScore(animal.attempts==1?15:10); scene_.effects_.addTime(5,animal.x+60,animal.y+60); }
    if(special) {
        for(int i=0;i<4;++i)
            scene_.effects_.spawnCompletionEffect(animal.x+60+scene_.randomInt(160)-80,animal.y+60+scene_.randomInt(160)-80,true);
    } else {
        scene_.effects_.spawnCompletionEffect(animal.x+60,animal.y+60,animal.kind!=2 || animal.attempts==1);
    }
    if(animal.kind==2) ++scene_.completedSheep_;
    scene_.sounds_.push_back("Sven2/sfx/sheep_explode3.ogg");
    int oldTile=scene_.grid_.tileAt(scene_.player_.x_,scene_.player_.y_);
    if(oldTile>=0) scene_.grid_.tiles_[oldTile]=empty;
    if(animal.tile>=0) { scene_.grid_.tiles_[animal.tile]=100; scene_.grid_.tileDirections_[animal.tile]=0; }
    animal.freed=true; animal.state=10; animal.age=0;
    --scene_.remainingTargets_;
    scene_.player_.x_=scene_.player_.targetX_=animal.x; scene_.player_.y_=scene_.player_.targetY_=animal.y;
    scene_.player_.facing_=2*animal.facing+1;
    scene_.world_[scene_.playerIndex_].committedY=scene_.player_.y_;
    scene_.player_.actionQueued_=false;
}

bool AnimalCharacters::ageSheepMood(Object& animal,float seconds) {
    // 0x408326..0x408436: the pulse also advances during chewing and turning.
    animal.pulse-=seconds*60;
    if(animal.pulse>=0) return false;
    animal.pulse=26;
    animal.moodBlink^=1;
    if(animal.temperament==2 || ++animal.angerTicks<=40) return false;
    animal.angerTicks=0;
    animal.impatience+=2;
    if(animal.impatience<12) return false;
    if(scene_.randomInt(5)!=0) { animal.impatience=10; return false; }
    animal.state=11;
    animal.age=animal.pulse=0;
    animal.draw={scene_.resources_.demonSheep_[animal.facing],static_cast<int>(animal.x),static_cast<int>(animal.y),0,sheepMirror[animal.facing]};
    scene_.emitNamedSound("SfxDevil");
    return true;
}

bool AnimalCharacters::moveDemonSheep(size_t index) {
    auto& animal=scene_.world_[index];
    const int playerTile=scene_.grid_.tileAt(scene_.player_.x_,scene_.player_.y_);
    const int preferred=animal.tile%stride>playerTile%stride?
        (animal.tile/stride>playerTile/stride?3:2):(animal.tile/stride<=playerTile/stride?1:0);
    // Literal fallback order in 0x408880. Despite the old analyst name, it
    // chooses a diagonal toward Sven, then tries the other directions.
    static constexpr int order[4][4]={{0,3,1,2},{1,0,2,3},{2,3,1,0},{3,2,0,1}};
    static constexpr int steps[4][2]={{1,-1},{1,1},{-1,1},{-1,-1}};
    for(int facing:order[preferred]) {
        const float nx=animal.x+40*steps[facing][0],ny=animal.y+40*steps[facing][1];
        const int tile=scene_.grid_.tileAt(nx,ny);
        if(!scene_.grid_.unoccupied(tile,index)) continue;
        animal.facing=facing;
        animal.targetX=nx; animal.targetY=ny;
        animal.age=0;
        scene_.grid_.reservations_[tile]=static_cast<int>(index);
        return true;
    }
    return false;
}

bool AnimalCharacters::shockPlayer(Object& animal,int timeChange) {
    Object facing=animal;
    facing.facing=2*animal.facing+1;
    if(!scene_.enemies_.canCatchPlayer(facing)) return false;
    // 0x42C990 cancels interpolation, the current interaction and powerup.
    scene_.player_.cancelPlayerStep();
    scene_.player_.activeSheep_=-1; scene_.player_.playerAction_=0; scene_.player_.interaction_=nullptr;
    scene_.player_.actionQueued_=false; scene_.player_.selectedInteraction_=-1;
    scene_.items_.playerEffect_=0; scene_.items_.effectRemaining_=scene_.items_.effectAge_=0;
    scene_.player_.playerShockAge_=0;
    animal.state=13; animal.age=0; animal.steps=0;
    scene_.effects_.addTime(timeChange,animal.x+animal.centerX,animal.y+animal.centerY);
    scene_.emitNamedSound("SfxDevilElectro");
    Object flash;
    flash.kind=34; flash.x=animal.x; flash.y=animal.y;
    flash.committedY=animal.y; flash.depthOffset=scene_.resources_.flash_->frameHeight;
    flash.draw={scene_.resources_.flash_,static_cast<int>(animal.x),static_cast<int>(animal.y),0};
    scene_.effects_.sceneEffects_.push_back(flash);
    return true;
}

void AnimalCharacters::updateSheep(float seconds) {
    for(size_t i=0;i<scene_.world_.size();++i) {
        auto& animal=scene_.world_[i];
        if(animal.kind!=2) continue;
        if(animal.freed) { animal.age+=seconds; continue; }
        if(animal.state==17) {
            animal.lift-=20*seconds;
            animal.age+=seconds;
            animal.draw={scene_.resources_.sheepStand_,static_cast<int>(animal.x),static_cast<int>(animal.y+animal.lift),animal.facing};
            if(animal.lift<-27) scene_.removeActor(i,false);
            continue;
        }
        if(animal.state>=11 && animal.state<=14) {
            if(animal.state==12) {
                animal.age+=seconds*20;
                if(animal.age>=8) animal.age=0;
                const float dx=animal.targetX-animal.x,dy=animal.targetY-animal.y;
                const float distance=std::max(std::abs(dx),std::abs(dy));
                const float fraction=distance?std::min(1.0f,seconds*sheepSpeed_*scene_.parameter("SheepRunMultiplier",1)/distance):1;
                animal.x+=dx*fraction; animal.y+=dy*fraction;
                animal.draw={scene_.resources_.demonWalks_[animal.facing],static_cast<int>(animal.x),static_cast<int>(animal.y),static_cast<int>(animal.age)%8,sheepMirror[animal.facing]};
                if(fraction<1) continue;
                if(animal.tile>=0 && scene_.grid_.tiles_[animal.tile]==2) scene_.grid_.tiles_[animal.tile]=empty;
                animal.tile=scene_.grid_.tileAt(animal.x,animal.y);
                animal.committedY=animal.y;
                if(animal.tile>=0) { scene_.grid_.tiles_[animal.tile]=2; scene_.grid_.reservations_[animal.tile]=-1; }
                if(++animal.steps==3) { animal.state=14; animal.age=0; animal.draw.frame=8; }
                else if(!shockPlayer(animal,-15) && !moveDemonSheep(i)) { animal.state=14; animal.age=0; animal.draw.frame=0; }
            } else {
                const int state=animal.state;
                if(scene_.advanceActor(animal,state==11?AnimationSequence::demonTransform:state==13?AnimationSequence::demonShock:AnimationSequence::demonRecover,20,seconds)) {
                    if(state==14) {
                        animal.state=0; animal.impatience=8;
                        animal.pulse=0; animal.idle=static_cast<float>(250+scene_.randomInt(1000));
                        animal.draw={scene_.resources_.sheepStand_,static_cast<int>(animal.x),static_cast<int>(animal.y),animal.facing};
                        continue;
                    }
                    if(state==13 || !shockPlayer(animal,10)) {
                        if(state==13 || !moveDemonSheep(i)) { animal.state=14; animal.age=0; animal.draw.frame=state==13?8:0; }
                        else { animal.state=12; animal.steps=0; }
                    }
                }
            }
            animal.draw.sprite=animal.state==12?scene_.resources_.demonWalks_[animal.facing]:scene_.resources_.demonSheep_[animal.facing];
            animal.draw.x=static_cast<int>(animal.x); animal.draw.y=static_cast<int>(animal.y);
            animal.draw.mirror=sheepMirror[animal.facing];
            continue;
        }
        if(static_cast<int>(i)==scene_.player_.activeSheep_ && scene_.player_.playerAction_!=2) continue;
        animal.age+=seconds;
        if(animal.state==3) {
            int turnFrame=static_cast<int>(animal.age*20);
            if(turnFrame<42) { animal.draw={scene_.resources_.turns_[animal.facing],static_cast<int>(animal.x),static_cast<int>(animal.y),turnFrames[turnFrame],sheepMirror[animal.facing]}; ageSheepMood(animal,seconds); continue; }
            if(turnFrame>=42) {
                animal.facing=animal.turnTo; animal.turnTo=-1; animal.state=0;
                animal.idle=static_cast<float>(450+scene_.randomInt(1000));
                if(animal.temperament==0 && animal.impatience>1) animal.impatience-=2;
            }
        } else if(animal.state==4) {
            float vx=animal.targetX-animal.x,vy=animal.targetY-animal.y;
            float distance=std::max(std::abs(vx),std::abs(vy));
            float step=distance?std::min(1.0f,seconds*sheepSpeed_/distance):1;
            animal.x+=vx*step; animal.y+=vy*step;
            animal.draw={scene_.resources_.sheepWalks_[animal.facing],static_cast<int>(animal.x),static_cast<int>(animal.y),static_cast<int>(animal.age*20)%15,sheepMirror[animal.facing]};
            if(step<1) continue;
            if(animal.tile>=0 && scene_.grid_.tiles_[animal.tile]==2) scene_.grid_.tiles_[animal.tile]=empty;
            animal.tile=scene_.grid_.tileAt(animal.x,animal.y);
            animal.committedY=animal.y;
            if(animal.tile>=0) { scene_.grid_.tiles_[animal.tile]=2; scene_.grid_.reservations_[animal.tile]=-1; }
            animal.state=0;
        } else if(animal.state==2) {
            if(animal.age*20<42) { animal.draw={scene_.resources_.chewing_,static_cast<int>(animal.x),static_cast<int>(animal.y),chewFrames[static_cast<int>(animal.age*20)],animal.facing==1}; ageSheepMood(animal,seconds); continue; }
            animal.state=0;
            animal.idle=static_cast<float>(250+scene_.randomInt(1000));
        } else {
            animal.idle-=seconds*60;
            if(animal.idle<0 && !scene_.enemies_.playerInRange(animal,5)) {
                animal.idle=static_cast<float>(450+scene_.randomInt(1000));
                int choice=scene_.randomInt(5);
                if(choice==1 && (animal.facing==1 || animal.facing==2)) { animal.state=2; animal.age=0; }
                else if(choice==2) animal.facing=scene_.randomInt(4);
                else if(choice==3) {
                    // Sheep move one diagonal cell along their four authored facings.
                    static constexpr int steps[4][2]={{1,-1},{1,1},{-1,1},{-1,-1}};
                    float nx=animal.x+40*steps[animal.facing][0],ny=animal.y+40*steps[animal.facing][1];
                    int target=scene_.grid_.tileAt(nx,ny);
                    if(scene_.grid_.unoccupied(target,i)) { animal.targetX=nx; animal.targetY=ny; animal.state=4; animal.age=0; scene_.grid_.reservations_[target]=static_cast<int>(i); }
                    else animal.facing=scene_.randomInt(4);
                }
            }
        }
        if(animal.state!=4 && ageSheepMood(animal,seconds)) continue;
        animal.draw={scene_.resources_.sheepStand_,static_cast<int>(animal.x),static_cast<int>(animal.y),animal.facing};
    }
}
}
