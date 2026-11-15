#include "LevelScene.h"
#include "scene/SceneConstants.h"
#include "level/LevelFile.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>

namespace sven2 {
using namespace scene_data;
namespace {
const LevelPackInfo& firstPack(const Engine& engine) {
    for(const auto& pack:engine.data().packs())
        if(pack.name=="XS") return pack;
    throw std::runtime_error("The XS level pack is missing.");
}
const LevelInfo& firstLevel(const Engine& engine) {
    const auto& pack=firstPack(engine);
    for(const auto& set:pack.levelsets) if(!set.levels.empty()) return set.levels.front();
    throw std::runtime_error("The XS level pack has no levels.");
}
}
LevelScene::LevelScene(Engine& engine):LevelScene(engine,GetTickCount()) {}
LevelScene::LevelScene(Engine& engine,std::uint32_t seed):LevelScene(engine,firstPack(engine),firstLevel(engine),seed) {}
LevelScene::LevelScene(Engine& engine,const LevelPackInfo& pack,const LevelInfo& level,std::uint32_t seed,std::uint32_t* sharedRandomState)
    :engine_(engine),randomState_(seed),sharedRandomState_(sharedRandomState),
     grid_(*this), resources_(*this), player_(*this), enemies_(*this), animals_(*this), items_(*this), ufoController_(*this), effects_(*this), renderer_(*this) {
    const auto& descriptor=engine.data().tileset(pack,level);
    log("Read level map: "+level.map);
    const LevelFile file(engine.readAsset(level.map),level.map);
    ufoController_.hideUfo_=file.hidesUfo();
    initialTime_=file.timeLimit();
    world_.reserve(LevelFile::objectCount);
    std::array<int,LevelFile::objectCount> slots;
    slots.fill(-1);
    grid_.load(file);
    renderer_.background_={engine.loadSprite(descriptor.background,1,1,true),0,0,0};
    effects_.splashes_[0].sprite=engine.loadSprite("Sven2/gfx/FX_all_splash.bmp",splashFrames,1);
    resources_.explosion_=engine.loadSprite("Sven2/gfx/explode.bmp",14,1);
    resources_.hearts_=engine.loadSprite("Sven2/gfx/anim_clouds_with_hearts.bmp",21,1);
    resources_.initializeActors();
    std::map<std::string,const Sprite*> sprites;
    unsigned scenery=0, actors=0;
    bool foundPlayer=false;
    for(size_t slot=0;slot<file.objects().size();++slot) {
        const auto& record=file.objects()[slot];
        int kind=record.kind();
        if(kind==-1) continue;
        float x=record.x(), y=record.y();
        if(!std::isfinite(x) || !std::isfinite(y) || std::abs(x)>32767 || std::abs(y)>32767)
            throw std::runtime_error("Invalid coordinates in level object "+std::to_string(slot));
        int pass=record.renderPass();
        if(pass!=0 && pass!=1) continue;
        int texture=record.textureOrPermanent(), facing=record.facing();
        TileInfo image;
        switch(kind) {
        case 0:
            if(foundPlayer || pass!=0) throw std::runtime_error("Expected one world player per level.");
            foundPlayer=true;
            playerIndex_=world_.size();
            player_.x_=x;
            player_.y_=y;
            image={"Sven2/gfx/Sven/sven_stand.bmp",8};
            ++actors;
            break;
        case 1:
            if(texture<0 || static_cast<size_t>(texture)>=descriptor.tiles.size())
                throw std::runtime_error("Level object references a missing tileset texture.");
            image=descriptor.tiles[texture];
            ++scenery;
            break;
        case 2: image={"Sven2/gfx/Sheep/Sheep_Stand.bmp",4}; ++actors; break;
        case 3: image={"Sven2/gfx/Alien/Alien_Stand.bmp",8}; ++actors; break;
        case 4: image={"Sven2/gfx/Dog/Dog_Stand.bmp",8}; ++actors; break;
        case 5: image={"Sven2/gfx/Guy/Guy_Stand.bmp",8}; ++actors; break;
        case 6: image={"Sven2/gfx/Broemse/Broemse_stand.bmp",8}; ++actors; break;
        case 7: image={"Sven2/gfx/Beer.bmp",1}; break;
        case 8: image={"Sven2/gfx/Teddybear.bmp",1}; break;
        case 9: image={"Sven2/gfx/Bone.bmp",1}; break;
        case 10: image={"Sven2/gfx/Artifact.bmp",1}; break;
        case 11: image={"Sven2/gfx/Animals/Chicken_Stand.bmp",4}; ++actors; break;
        case 12: image={"Sven2/gfx/Animals/Goat_Stand.bmp",4}; ++actors; break;
        case 13: image={"Sven2/gfx/Animals/Pig_Stand.bmp",4}; ++actors; break;
        default: throw std::runtime_error("Unsupported object kind in LevelScene: "+std::to_string(kind));
        }
        auto [entry, inserted]=sprites.try_emplace(image.file,nullptr);
        if(inserted) entry->second=engine.loadSprite(image.file,image.frames,1);
        int frame=kind==1?std::clamp(record.frame(),0,static_cast<int>(image.frames)-1):std::clamp(facing,0,static_cast<int>(image.frames)-1);
        SpriteDraw draw{entry->second,static_cast<int>(x),static_cast<int>(y),frame};
        if(pass==0) {
            Object object;
            object.draw=draw;
            object.depthOffset=record.height()-record.depthAdjustment();
            object.committedY=y;
            object.kind=kind;
            object.tile=kind>=2?record.tile():grid_.tileAt(x,y);
            object.facing=isAnimal(kind)?std::clamp(facing,0,3):std::clamp(facing,0,7);
            object.temperament=record.temperament();
            object.angerTicks=record.angerTicks();
            object.attempts=record.attempts();
            object.impatience=record.impatience();
            object.idle=record.idle();
            object.pulse=record.pulse();
            object.x=object.targetX=x;
            object.y=object.targetY=y;
            object.original=record.words();
            object.state=record.state();
            object.permanent=record.textureOrPermanent();
            object.centerX=record.width()*0.5f;
            object.centerY=record.height()*0.5f;
            object.animRate=record.frameRate();
            object.hunting=record.hunting();
            object.retry=record.temperament()!=0;
            object.moodBlink=record.moodBlink()&1;
            object.attempts=0; // 0x422420 resets interaction counts at level entry.
            if(isAnimal(kind)) { object.impatience=2*(object.impatience/2); ++remainingTargets_; }
            // 0x422AF6 sets the stored adjustment (+92), not the final sort offset.
            if(kind>=11 && kind<=13) { object.pulse=26; object.state=0; object.depthOffset=record.height()-40; }
            if(kind==3) object.idle=static_cast<float>(50+randomInt(100));
            if(kind==3 || kind==6) {
                const auto& rules=resources_.enemyRules_[kind];
                object.pulse=rules.idle+(kind==6?rules.idleRandom:randomInt(static_cast<int>(rules.idleRandom)));
            }
            if(isEnemy(kind)) enemies_.drawEnemy(object);
            slots[slot]=static_cast<int>(world_.size());
            world_.push_back(object);
        }
        else renderer_.overlays_.push_back(draw);
    }
    for(size_t tile=0;tile<grid_.itemCodes_.size();++tile) {
        const int slot=grid_.itemObjects_[tile];
        grid_.itemObjects_[tile]=slot>=0 && slot<256?slots[slot]:-1;
        if(grid_.itemCodes_[tile]==0) grid_.itemObjects_[tile]=-1;
    }
    std::vector<size_t> sheep;
    for(size_t i=0;i<world_.size();++i) if(world_[i].kind==2) sheep.push_back(i);
    if(!sheep.empty()) world_[sheep[randomInt(static_cast<int>(sheep.size()))]].temperament=1; // 0x422C00
    items_.spawnPeriod_=parameter("WaitTime",10);
    items_.spawnClock_=items_.spawnPeriod_;
    if(!foundPlayer || grid_.tileAt(player_.x_,player_.y_)<0) throw std::runtime_error("Missing or invalid player spawn.");
    player_.targetX_=player_.x_;
    player_.targetY_=player_.y_;
    resources_.initializePlayer();
    world_[playerIndex_].committedY=player_.y_;
    renderer_.sorted_.reserve(world_.size());
    renderer_.draws_.reserve(1+world_.size()+renderer_.overlays_.size());
    log("Loaded "+level.map+" with "+std::to_string(scenery)+" scenery objects and "+
        std::to_string(actors)+" actors from folder data.");
}

bool LevelScene::isAnimal(int kind) { return kind==2 || (kind>=11 && kind<=13); }

bool LevelScene::isEnemy(int kind) { return kind>=3 && kind<=6; }

int LevelScene::occupancyKind(int kind) { return kind>=11 && kind<=13?kind-4:kind; }

float LevelScene::parameter(const char* key,float fallback) const {
    double value=engine_.data().parameter(key,fallback);
    return std::isfinite(value) && value>=0?static_cast<float>(value):fallback;
}

int LevelScene::randomInt(int limit) {
    auto& state=sharedRandomState_?*sharedRandomState_:randomState_;
    state=state*214013u+2531011u;
    const int value=static_cast<int>((state>>16)&0x7fffu);
    return limit>0?value%limit:0;
}

bool LevelScene::advanceActor(Object& actor,AnimationSequence table,float rate,float seconds) {
    const auto& frames=actorFrameTables.at(table);
    actor.age+=seconds*rate;
    if(static_cast<size_t>(actor.age)>=frames.size()) {
        actor.age=0;
        actor.draw.frame=0;
        return true;
    }
    actor.draw.frame=frames[static_cast<size_t>(actor.age)];
    return false;
}

void LevelScene::emitNamedSound(const char* name) {
    auto path=engine_.data().soundPath(name);
    if(!path.empty()) sounds_.push_back(path);
}

void LevelScene::addScore(int amount) { scoreDelta_+=amount; }

void LevelScene::recordLifeLoss() {
    // 0x412B00 insurance depends on campaign score and belongs to GameSession.
    ++pendingLifeHits_;
}

int LevelScene::takeLivesGained() {
    const int result=items_.livesGained_;
    items_.livesGained_=0;
    return result;
}

void LevelScene::removeActor(size_t index,bool playerResolved) {
    auto& actor=world_[index];
    if(actor.freed) return;
    if(isAnimal(actor.kind)) {
        --remainingTargets_;
        if(playerResolved && actor.kind==2) ++completedSheep_;
    }
    if(actor.tile>=0 && static_cast<size_t>(actor.tile)<grid_.tiles_.size() && grid_.tiles_[actor.tile]==occupancyKind(actor.kind))
        grid_.tiles_[actor.tile]=empty;
    for(auto& reservation:grid_.reservations_) if(reservation==static_cast<int>(index)) reservation=-1;
    actor.freed=true;
}

void LevelScene::move(float seconds,int dx,int dy) { update(seconds,dx,dy,false); }

void LevelScene::update(float seconds,int dx,int dy,bool actionPressed) {
    if(!std::isfinite(seconds) || seconds<0) return;
    if(!player_.playerAction_) player_.actionQueued_=false; // source clears the edge flag in idle/walking state 0
    if(actionPressed && !player_.actionDown_ && (player_.playerAction_==0 || player_.playerAction_==1 || player_.playerAction_==4)) player_.actionQueued_=true;
    player_.actionDown_=actionPressed;
    dx=std::clamp(dx,-1,1); dy=std::clamp(dy,-1,1);
    player_.setMovement(dx,dy);
    const std::array<CharacterController*,2> autonomousCharacters={&enemies_,&animals_};
    CharacterController& controlledPlayer=player_;
    // Substeps preserve occupancy and sequence boundaries across slow frames.
    float left=seconds;
    do {
        float dt=std::min(left,1.0f/60.0f); left-=dt;
        effects_.splashAge_+=dt; effects_.splashPlayerAge_+=dt;
        effects_.updateEffects(dt);
        player_.markPlayerCell();
        items_.updateItems(dt);
        for(auto* characters:autonomousCharacters) characters->update(dt);
        ufoController_.updateUfo(dt);
        controlledPlayer.update(dt);
    } while(left>0);
}

void LevelScene::timeExpired() {
    if(expired_) return;
    expired_=true;
    ufoController_.updateUfo(0); // 0x408B80 requests a remaining animal; no life penalty.

}

int LevelScene::takeScoreDelta() { int result=scoreDelta_; scoreDelta_=0; return result; }

float LevelScene::takeTimeDelta() { float result=effects_.timeDelta_; effects_.timeDelta_=0; return result; }

int LevelScene::takeLifeHits() { int result=pendingLifeHits_; pendingLifeHits_=0; return result; }

std::vector<std::string> LevelScene::takeSounds() {
    std::vector<std::string> result; result.swap(sounds_); effects_.splashSound_=false; return result;
}
const std::vector<SpriteDraw>& LevelScene::draws() { return renderer_.draws(); }
bool LevelScene::takeSplashSound() { return effects_.takeSplashSound(); }
}
