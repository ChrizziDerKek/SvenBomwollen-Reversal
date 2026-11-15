#include "scene/SceneRenderer.h"
#include "LevelScene.h"
#include "scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sven2 {
using namespace scene_data;
SceneRenderer::SceneRenderer(LevelScene& scene):scene_(scene),overlayFont_(scene.engine_,true) {}

const std::vector<SpriteDraw>& SceneRenderer::draws() {
    sorted_=scene_.world_;
    sorted_.insert(sorted_.end(),scene_.effects_.sceneEffects_.begin(),scene_.effects_.sceneEffects_.end());
    // Original 0x40D72C sorts rendered Y + height - adjustment. Player grid
    // ownership stays at the committed cell while the sprite moves between cells.
    auto depth=[&](const Object& object) {
        // These animations are placed at the animal's canvas, although the
        // command is stored on Sven. Keep that compound sprite at its visible
        // owner's depth instead of promoting it to Sven's approach position.
        if(object.kind==0 && scene_.player_.playerAction_==1 && scene_.player_.activeSheep_>=0) {
            const auto& animal=scene_.world_[scene_.player_.activeSheep_];
            if(animal.kind>=11 && animal.kind<=13)
                return static_cast<float>(animal.y+animal.depthOffset);
        }
        const float y=object.kind==0?scene_.player_.y_:object.y;
        return static_cast<float>(y+object.depthOffset);
    };
    // Start each frame in file slot order to preserve the original equal-depth behavior.
    for(size_t i=0;i<sorted_.size();++i)
        for(size_t j=i+1;j<sorted_.size();++j)
            if(depth(sorted_[i])>depth(sorted_[j]))
                std::swap(sorted_[i],sorted_[j]);
    draws_.clear();
    draws_.push_back(background_);
    auto indicator=[&](const Object& animal) {
        static constexpr int offsetX[4][4]={{40,40,20,20},{40,24,30,16},{50,50,5,5},{50,50,5,5}};
        static constexpr int offsetY[4][4]={{0,0,0,0},{-1,-1,-1,-1},{-10,10,10,-10},{-10,20,20,-10}};
        const int family=animal.kind==2?0:animal.kind-10;
        draws_.push_back({animal.kind==2 && animal.temperament==2?scene_.resources_.kettleSprite_:scene_.resources_.moodSprite_,
            static_cast<int>(animal.x)+offsetX[family][animal.facing],static_cast<int>(animal.y+animal.lift)+offsetY[family][animal.facing],
            std::clamp(animal.impatience+animal.moodBlink,0,11)});
    };
    for(const auto& object:sorted_) {
        if(object.freed) continue;
        const bool interacting=scene_.isAnimal(object.kind) && scene_.player_.activeSheep_>=0 &&
            (scene_.player_.playerAction_==1 || scene_.player_.playerAction_==3 || scene_.player_.playerAction_==4) && object.tile==scene_.world_[scene_.player_.activeSheep_].tile;
        if(object.kind==0 && scene_.enemies_.battleEnemy_>=0) continue;
        if(!interacting && object.draw.sprite) draws_.push_back(object.draw);
        if((object.kind==4 || object.kind==5) && object.state==10) {
            static constexpr int sleepX[2][4]={{50,50,0,0},{90,100,-10,0}};
            static constexpr int sleepY[2][4]={{-10,10,10,-10},{10,30,30,10}};
            const int family=object.kind-4,quadrant=object.facing/2;
            // Sleep's secondary counter advances 0..12, then loops 7..12.
            const int age=static_cast<int>(object.age);
            const int frame=age<13?age:7+(age-13)%6;
            draws_.push_back({scene_.resources_.sleepSymbols_,static_cast<int>(object.x)+sleepX[family][quadrant],
                static_cast<int>(object.y)+sleepY[family][quadrant],frame});
        }
        if(scene_.isAnimal(object.kind) && !interacting && object.state!=17 && !(object.kind==2 && object.state>=11 && object.state<=14)) indicator(object);
        // Original player render states 1/2 draw the indicator after the combined
        // interaction sprite (0x40D977-0x40D9E8), so it cannot be painted over.
        if(object.kind==0 && scene_.player_.activeSheep_>=0 &&
           (scene_.player_.playerAction_==1 || scene_.player_.playerAction_==3 || scene_.player_.playerAction_==4) && scene_.isAnimal(scene_.world_[scene_.player_.activeSheep_].kind))
            indicator(scene_.world_[scene_.player_.activeSheep_]);
    }
    const int frame=static_cast<int>(scene_.effects_.splashAge_*splashFrameRate);
    if(frame<splashFrames) {
        for(auto splash:scene_.effects_.splashes_) {
            if(!splash.sprite) continue;
            splash.frame=frame;
            draws_.push_back(splash);
        }
    }
    if(scene_.items_.playerEffect_) {
        const int effect=scene_.items_.playerEffect_==132?0:scene_.items_.playerEffect_==134?1:scene_.items_.playerEffect_==135?2:scene_.items_.playerEffect_==136?3:4;
        const auto* image=scene_.resources_.effectSprites_[effect];
        draws_.push_back({image,static_cast<int>(scene_.player_.x_)+28,static_cast<int>(scene_.player_.y_+scene_.player_.playerLift_)-4,static_cast<int>(scene_.items_.effectAge_*15)%image->rows});
    }
    if(scene_.ufoController_.ufo_.state>=0) {
        const float orbitX=std::sin(scene_.ufoController_.ufo_.angleX*57.295776f)*scene_.ufoController_.ufo_.radius+scene_.ufoController_.ufo_.x;
        const float orbitY=std::cos(scene_.ufoController_.ufo_.angleY*57.295776f)*scene_.ufoController_.ufo_.radius+scene_.ufoController_.ufo_.y;
        const int ufoFrame=static_cast<int>(scene_.ufoController_.ufo_.age)%13;
        if(scene_.ufoController_.ufo_.beam) {
            const auto& frames=actorFrameTables.at(AnimationSequence::ufoBeam);
            const int step=std::min(static_cast<int>(frames.size())-1,static_cast<int>(scene_.ufoController_.ufo_.beamAge));
            draws_.push_back({scene_.resources_.beamSprite_,static_cast<int>(scene_.ufoController_.ufo_.x)-150,static_cast<int>(scene_.ufoController_.ufo_.y)-93,frames[step]});
        }
        if(!scene_.ufoController_.hideUfo_) {
            draws_.push_back({scene_.resources_.ufoShadow_,static_cast<int>(orbitX)-240,static_cast<int>(orbitY)-160,ufoFrame});
            draws_.push_back({scene_.resources_.ufoSprite_,static_cast<int>(orbitX)-240,static_cast<int>(orbitY)-460,ufoFrame});
        }
    }
    draws_.insert(draws_.end(),overlays_.begin(),overlays_.end());
    for(const auto& popup:scene_.effects_.timePopups_) {
        const std::string label=(popup.amount>=0?"+":"")+std::to_string(popup.amount)+"sec";
        overlayFont_.append(draws_,label,static_cast<int>(popup.x),static_cast<int>(popup.y));
    }
    return draws_;
}
}
