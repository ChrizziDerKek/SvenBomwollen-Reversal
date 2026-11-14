#include "level/LevelItems.h"
#include "LevelScene.h"
#include "scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sven2 {
using namespace scene_data;
LevelItems::LevelItems(LevelScene& scene):scene_(scene) {}

int LevelItems::findDistraction(const Object& actor,int code) const {
    // 0x4051D0 scans the immediate ring, then the outer ring, in this order.
    static constexpr int neighbors[24][2]={
        {0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},
        {0,-2},{1,-2},{2,-2},{2,-1},{2,0},{2,1},{2,2},{1,2},
        {0,2},{-1,2},{-2,2},{-2,1},{-2,0},{-2,-1},{-2,-2},{-1,-2}};
    for(const auto& offset:neighbors) {
        int x=actor.tile%stride+offset[0],y=actor.tile/stride+offset[1];
        if(x<1 || x>columns || y<1 || y>rows) continue;
        int tile=y*stride+x;
        if(scene_.grid_.itemCodes_[tile]==code && scene_.grid_.itemObjects_[tile]>=0 && !scene_.world_[scene_.grid_.itemObjects_[tile]].freed)
            return scene_.grid_.itemObjects_[tile];
    }
    return -1;
}

void LevelItems::removeItem(int tile) {
    if(tile<0 || static_cast<size_t>(tile)>=scene_.grid_.itemCodes_.size()) return;
    const int index=scene_.grid_.itemObjects_[tile];
    if(index>=0 && static_cast<size_t>(index)<scene_.world_.size()) scene_.world_[index].freed=true;
    scene_.grid_.itemCodes_[tile]=0;
    scene_.grid_.itemObjects_[tile]=-1;
}

void LevelItems::spawnItem(int kind,int tile) {
    if(kind<7 || kind>18 || scene_.isAnimal(kind) || scene_.grid_.itemCodes_[tile]) return;
    size_t slot=scene_.world_.size();
    for(size_t i=0;i<scene_.world_.size();++i)
        if(scene_.world_[i].freed && i!=scene_.playerIndex_ && static_cast<int>(i)!=scene_.player_.activeSheep_ && static_cast<int>(i)!=scene_.ufoController_.ufo_.target) { slot=i; break; }
    if(slot>=256) return; // original object pool also has 256 slots
    Object object;
    object.kind=kind;
    object.tile=tile;
    object.x=object.targetX=static_cast<float>((tile%stride-1)*40-20);
    object.y=object.targetY=static_cast<float>((tile/stride-1)*40-20);
    object.centerX=object.centerY=40;
    object.committedY=object.y;
    object.depthOffset=20; // height 80, depth adjustment 60
    object.draw={scene_.resources_.itemSprites_[kind],static_cast<int>(object.x),static_cast<int>(object.y),kind>=14?kind-14:0};
    const char* duration=kind==7?"BeerTime":kind==8?"TeddyTime":kind==9?"BoneTime":kind==10?"ArtifactTime":kind==14?"FireworkTime":kind==15?"GiftTime":kind==16?"ImpotentTime":kind==17?"SmellTime":"FlowerTime";
    object.pulse=scene_.parameter(duration,kind>=14?10.0f:16.0f);
    if(slot==scene_.world_.size()) scene_.world_.push_back(object);
    else scene_.world_[slot]=object;
    static constexpr unsigned char codes[12]={130,131,129,128,0,0,0,132,133,134,135,136};
    scene_.grid_.itemCodes_[tile]=codes[kind-7];
    scene_.grid_.itemObjects_[tile]=static_cast<int>(slot);
}

void LevelItems::spawnItems() {
    // 0x411400 makes one attempt per present marker group, not one per marker.
    static constexpr int choices[9]={7,8,9,10,14,15,16,17,18};
    for(int group=0;group<3;++group) {
        if(scene_.grid_.spawners_[group].empty()) continue;
        int tile=scene_.grid_.spawners_[group][scene_.randomInt(static_cast<int>(scene_.grid_.spawners_[group].size()))];
        if(scene_.grid_.itemCodes_[tile]) continue;
        const int occupant=scene_.grid_.tileAt(scene_.player_.x_,scene_.player_.y_)==tile?100:scene_.grid_.tiles_[tile];
        if(group!=1 && occupant>=3 && occupant<=6) continue;
        if(group!=0 && occupant==100) continue;
        const int selection=group==0?scene_.randomInt(4):group==1?4+scene_.randomInt(5):scene_.randomInt(9);
        spawnItem(choices[selection],tile);
    }
}

void LevelItems::updateItems(float seconds) {
    effectAge_+=seconds;
    if(playerEffect_) {
        effectRemaining_-=seconds;
        if(effectRemaining_<0) { playerEffect_=0; effectRemaining_=0; }
    }
    spawnClock_-=seconds;
    if(spawnClock_<0) { spawnClock_=std::max(0.01f,spawnPeriod_); spawnItems(); }
    for(size_t i=0;i<scene_.world_.size();++i) {
        auto& object=scene_.world_[i];
        if(object.freed) continue;
        if(object.kind==1 && object.state==1) {
            float velocity;
            std::memcpy(&velocity,&object.original[7],4);
            object.x+=velocity*seconds;
            if(object.x<-400) {
                object.x=800;
                velocity=static_cast<float>(-15-scene_.randomInt(35));
                std::memcpy(&object.original[7],&velocity,4);
            }
            object.draw.x=static_cast<int>(object.x);
        }
        if((object.kind>=7 && object.kind<=10 && !object.permanent) || (object.kind>=14 && object.kind<=18)) {
            object.pulse-=seconds;
            if(object.pulse<0) removeItem(object.tile);
        }
    }
}

void LevelItems::affectNearbySheep() {
    if(playerEffect_!=135 && playerEffect_!=136) return;
    const int tile=scene_.grid_.tileAt(scene_.player_.x_,scene_.player_.y_);
    if(tile<0) return;
    const int mood=playerEffect_==135?10:0;
    bool changed=false;
    for(auto& animal:scene_.world_) {
        if(animal.freed || animal.kind!=2 || animal.temperament==2 || animal.impatience==mood || animal.tile<0) continue;
        const int dx=std::abs(animal.tile%stride-tile%stride);
        const int dy=std::abs(animal.tile/stride-tile/stride);
        if(dx<=1 && dy<=1 && (dx || dy)) { animal.impatience=mood; changed=true; }
    }
    if(changed) {
        const char* sound=mood==10?(scene_.randomInt(2)==0?"Sheep_beamup.ogg":"Sheep_angry2.ogg"):
                                   (scene_.randomInt(2)==0?"Sheep_happy1.ogg":"Sheep_happy2.ogg");
        scene_.sounds_.push_back(std::string("Sven2/sfx/")+sound);
    }
}

void LevelItems::pickupPlayerItem() {
    if(scene_.enemies_.battleEnemy_>=0 || scene_.player_.playerBeamState_ || scene_.effects_.splashPlayerAge_*scene_.player_.defaultRate_<16) return;
    const int tile=scene_.grid_.tileAt(scene_.player_.x_,scene_.player_.y_);
    if(tile<0) return;
    const int code=scene_.grid_.itemCodes_[tile];
    if(code==128) {
        if(playerEffect_==137 || scene_.ufoController_.ufo_.state>=0) return;
        removeItem(tile);
        scene_.ufoController_.requestUfo(static_cast<int>(scene_.playerIndex_));
        return;
    }
    if(code<132 || code>136 || (code!=133 && playerEffect_==137)) return;
    removeItem(tile);
    if(code==133) {
        // Nested independent rand tests at 0x42AA71, not a flat weighted draw.
        if(scene_.randomInt(50)==0) {
            ++livesGained_;
        } else if(scene_.randomInt(25)==0) scene_.effects_.addTime(25,scene_.player_.x_+60,scene_.player_.y_+60);
        else if(scene_.randomInt(15)==0) scene_.effects_.addTime(15,scene_.player_.x_+60,scene_.player_.y_+60);
        else if(scene_.randomInt(10)==0) scene_.effects_.addTime(10,scene_.player_.x_+60,scene_.player_.y_+60);
        else scene_.effects_.addTime(5,scene_.player_.x_+60,scene_.player_.y_+60);
        scene_.sounds_.push_back("Sven2/sfx/item_gift.ogg");
        return;
    }
    playerEffect_=code;
    effectAge_=0;
    const char* duration=code==132?"FireworkDuration":code==134?"ImpotentDuration":code==135?"SmellDuration":"FlowerDuration";
    effectRemaining_=scene_.parameter(duration,code==135?15.0f:10.0f);
    const char* sound=code==132?"item_firework.ogg":code==134?"item_impotenz.ogg":code==135?"item_stinky.ogg":"item_rose.ogg";
    scene_.sounds_.push_back(std::string("Sven2/sfx/")+sound);
}
}
