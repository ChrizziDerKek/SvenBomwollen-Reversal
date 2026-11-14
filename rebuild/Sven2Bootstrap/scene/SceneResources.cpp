#include "scene/SceneResources.h"
#include "LevelScene.h"
#include "scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sven2 {
using namespace scene_data;
SceneResources::SceneResources(LevelScene& scene):scene_(scene) {}

const Sprite* SceneResources::sprite(const std::string& path,int frames) {
    auto found=spriteCache_.find(path);
    if(found!=spriteCache_.end()) return found->second;
    return spriteCache_[path]=scene_.engine_.loadSprite(path,static_cast<short>(frames),1);
}

void SceneResources::initializeActors() {
    const char* families[4]={"Alien","Dog","Guy","Broemse"};
    const char* lower[4]={"alien","dog","guy","broemse"};
    const char* headings[8]={"back","diagonal_back","side","diagonal_front","front","diagonal_front","side","diagonal_back"};
    const int walkFrames[4]={12,16,16,8};
    const float speeds[4]={60,55,35,50};
    const float idleRandom[4]={1.5f,2,1.5f,2};
    const float ranges[4]={4,3,2,4};
    const float runRates[4]={30,30,30,50};
    const float walkRates[4]={15,15,15,25};
    for(int i=0;i<4;++i) {
        const int kind=i+3;
        auto& images=actorSprites_[kind];
        auto& rules=enemyRules_[kind];
        std::string folder=std::string("Sven2/gfx/")+families[i]+"/";
        images.stand=sprite(folder+families[i]+(kind==6?"_stand.bmp":"_Stand.bmp"),8);
        for(int d=0;d<8;++d) {
            images.walk[d]=sprite(folder+"anim_"+lower[i]+"_walk_"+headings[d]+".bmp",walkFrames[i]);
            if(kind==3) images.idle[d]=sprite(folder+"anim_alien_zero_"+headings[d]+".bmp",d==0 || d==4?5:4);
        }
        auto setting=[&](const char* suffix,float fallback) {
            const std::string key=std::string(families[i])+suffix;
            return scene_.parameter(key.c_str(),fallback);
        };
        rules.speed=setting("Speed",speeds[i]);
        rules.run=setting("RunMultiplier",2);
        rules.diagonal=setting("AdjustXY",0.75f);
        rules.walkRate=setting("WalkFrameRate",walkRates[i]);
        rules.runRate=setting("RunFrameRate",runRates[i]);
        rules.idle=setting("StandStillTime",kind==6?3.0f:4.0f);
        rules.idleRandom=setting("StandStillTimeRandom",idleRandom[i]);
        rules.sleep=setting("SleepTime",15);
        rules.range=setting("HuntSvenDistance",ranges[i]);
        rules.wakeRange=setting(kind==4?"WakeUpDistance":"HuntSvenDistance",ranges[i]);
        rules.huntSteps=static_cast<int>(setting("HuntTileLength",kind==5?10.0f:20.0f));
        rules.walkSteps=static_cast<int>(setting("TileLength",kind==3?3.0f:kind==6?6.0f:5.0f));
        rules.walkRandom=static_cast<int>(setting("TileLengthRandom",kind==4?3.0f:2.0f));
        rules.fightTime=setting("FightTime",1.5f);
    }
    for(int d=0;d<4;++d) {
        const bool front=d==1 || d==2;
        playerBeamSprites_[d]=sprite(std::string("Sven2/gfx/Sven/anim_sven_beam_up_")+(front?"front":"back")+".bmp",22);
        actorSprites_[4].sleep[d]=sprite(std::string("Sven2/gfx/Dog/anim_dog_sleep_")+(front?"front":"back")+".bmp",4);
        actorSprites_[4].beam[d]=sprite(std::string("Sven2/gfx/Dog/anim_dog_beamup_")+(front?"front":"back")+".bmp",6);
        actorSprites_[5].sleep[d]=sprite(std::string("Sven2/gfx/Guy/anim_guy_laydown_")+(front?"front":"back")+".bmp",15);
        actorSprites_[5].wake[d]=sprite(std::string("Sven2/gfx/Guy/anim_guy_standup_")+(front?"front":"back")+".bmp",14);
        actorSprites_[5].beam[d]=sprite(std::string("Sven2/gfx/Guy/anim_guy_beam_up_diagonal_")+(front?"front":"back")+".bmp",6);
        actorSprites_[5].grumble[d]=sprite(std::string("Sven2/gfx/Guy/anim_guy_grumble_diagonal_")+(front?"front":"back")+".bmp",front?19:11);
        actorSprites_[6].dogPlay[d]=sprite(std::string("Sven2/gfx/Broemse/anim_broemse_beating_wotan_")+(front?"front":"back")+".bmp",front?11:10);
        actorSprites_[5].distraction[d]=sprite(std::string("Sven2/gfx/Guy/anim_guy_drinking_diagonal_")+(front?"front":"back")+".bmp",15);
        actorSprites_[6].beam[d]=sprite(std::string("Sven2/gfx/Broemse/anim_broemse_beam_up_diagonal_")+(front?"front":"back")+".bmp",5);
        actorSprites_[6].distraction[d]=sprite(std::string("Sven2/gfx/Broemse/anim_broemse_beating_teddybear_diagonal_")+(front?"front":"back")+".bmp",front?14:9);
        actorSprites_[6].battle[d]=sprite(std::string("Sven2/gfx/Broemse/anim_broemse_beating_sven_")+(front?"front":"back")+".bmp",front?35:23);
        actorSprites_[3].battle[d]=sprite("Sven2/gfx/Alien/anim_alien_probe2.bmp",55);
        actorSprites_[4].battle[d]=actorSprites_[5].battle[d]=sprite("Sven2/gfx/anim_cloud.bmp",10);
    }
    const char* animals[3]={"Chicken","Goat","Pig"};
    const char* animalFiles[3]={"chicken","Goat","pig"};
    for(int a=0;a<3;++a) {
        actorSprites_[a+11].stand=sprite(std::string("Sven2/gfx/Animals/")+animals[a]+"_Stand.bmp",4);
        for(int d=0;d<4;++d) {
            const bool back=d==0 || d==3;
            const AnimationSequence table=a==0?AnimationSequence::chickenInteraction:a==1?(back?AnimationSequence::goatInteractionBack:AnimationSequence::goatInteractionFront):(back?AnimationSequence::pigInteractionBack:AnimationSequence::pigInteractionFront);
            const int frames=a==0?5:a==1?(back?7:6):(back?4:3);
            auto& action=animalActions_[a][d];
            action.sprite=sprite(std::string("Sven2/gfx/Animals/anim_NF_")+animalFiles[a]+(back?"_back.bmp":"_front.bmp"),frames);
            action.frames=actorFrameTables.at(table);
            action.sound=std::string("Sven2/sfx/shag_")+(a==0?"chick":a==1?"goat":"pig")+".ogg";
            action.facing=d;
            action.mirror=a==0?sheepMirror[d]:d>=2;
        }
    }
    const char* itemFiles[4]={"Beer.bmp","Teddybear.bmp","Bone.bmp","Artifact.bmp"};
    for(int i=0;i<4;++i) itemSprites_[i+7]=sprite(std::string("Sven2/gfx/")+itemFiles[i],1);
    for(int i=14;i<=18;++i) itemSprites_[i]=sprite("Sven2/gfx/icons.bmp",5);
    const char* effects[5]={"symbol_power.bmp","symbol_bubbles_good.bmp","symbol_smell.bmp","symbol_flowers.bmp","icon_alien.bmp"};
    const int effectFrames[5]={13,6,14,8,13};
    for(int i=0;i<5;++i) effectSprites_[i]=sprite(std::string("Sven2/gfx/")+effects[i],effectFrames[i]);
    ufoSprite_=sprite("Sven2/gfx/ufo.bmp",13);
    ufoShadow_=sprite("Sven2/gfx/ufo_shadow.bmp",13);
    beamSprite_=sprite("Sven2/gfx/anim_beam.bmp",44);
    moodSprite_=sprite("Sven2/gfx/symbols.bmp",12);
    kettleSprite_=sprite("Sven2/gfx/symbol_kettle.bmp",12);
    digging_=sprite("Sven2/gfx/Digging.bmp",18);
    sleepSymbols_=sprite("Sven2/gfx/Guy/anim_guy_gfx_zzz.bmp",13);
}

void SceneResources::initializePlayer() {
    stand_=scene_.player().sprite;
    sheepStand_=scene_.engine_.loadSprite("Sven2/gfx/Sheep/Sheep_Stand.bmp",4,1);
    chewing_=scene_.engine_.loadSprite("Sven2/gfx/Sheep/Sheep_Chewing.bmp",20,1);
    const char* walkFiles[8]={"Sven_walk-back.bmp","Sven_walk02.bmp","Sven_walk-side.bmp","Sven_walk01.bmp","Sven_walk-front.bmp","Sven_walk01.bmp","Sven_walk-side.bmp","Sven_walk02.bmp"};
    for(int i=0;i<8;++i) walks_[i]=scene_.engine_.loadSprite(std::string("Sven2/gfx/Sven/")+walkFiles[i],14,1);
    auto away=scene_.engine_.loadSprite("Sven2/gfx/Sheep/Sheep_walkaway_120x120.bmp",15,1);
    auto up=scene_.engine_.loadSprite("Sven2/gfx/Sheep/Sheep_walkup_120x120.bmp",15,1);
    sheepWalks_={away,up,up,away};
    auto demonBack=sprite("Sven2/gfx/Sheep/anim_sheep_demon_back.bmp",23);
    auto demonFront=sprite("Sven2/gfx/Sheep/anim_sheep_demon_front.bmp",23);
    demonSheep_={demonBack,demonFront,demonFront,demonBack};
    auto demonRunBack=sprite("Sven2/gfx/Sheep/anim_sheep_demon_run_back.bmp",8);
    auto demonRunFront=sprite("Sven2/gfx/Sheep/anim_sheep_demon_run_front.bmp",8);
    demonWalks_={demonRunBack,demonRunFront,demonRunFront,demonRunBack};
    const char* electricFiles[]={"back","diagonal_back","side","diagonal_front","front","diagonal_front","side","diagonal_back"};
    for(int i=0;i<8;++i) electric_[i]=sprite(std::string("Sven2/gfx/Sven/anim_sven_electric_")+electricFiles[i]+".bmp",8);
    flash_=sprite("Sven2/gfx/flash.bmp",2);
    auto back=scene_.engine_.loadSprite("Sven2/gfx/Sheep/sheep_sb.bmp",11,1);
    auto front=scene_.engine_.loadSprite("Sven2/gfx/Sheep/sheep_sf.bmp",11,1);
    turns_={back,front,front,back};
    const char* whistleFiles[8]={"back","diagonal_back","side","diagonal_front","front","diagonal_front","side","diagonal_back"};
    for(int i=0;i<8;++i) whistles_[i]=scene_.engine_.loadSprite(std::string("Sven2/gfx/Sven/anim_sven_whistle_")+whistleFiles[i]+".bmp",i%2?16:19,1);
    auto positiveParameter=[&](const char* key,float fallback) {
        double value=scene_.engine_.data().parameter(key,fallback);
        return std::isfinite(value) && value>0?static_cast<float>(value):fallback;
    };
    scene_.player_.speed_=positiveParameter("SvenSpeed",145);
    scene_.player_.diagonal_=positiveParameter("SvenDiagonalMultiplier",0.75f);
    scene_.player_.walkRate_=positiveParameter("SvenWalkDefaultFrameRate",24);
    scene_.player_.defaultRate_=positiveParameter("SvenDefaultFrameRate",15);
    scene_.player_.actionRate_=positiveParameter("SvenShagDefaultFrameRate",20);
    scene_.animals_.sheepSpeed_=positiveParameter("SheepSpeed",50);
    scene_.player_.frigidaRate_=positiveParameter("SvenShagFrigidaFrameRate",16);
    for(int i=0;i<8;++i) blinks_[i]=scene_.engine_.loadSprite(std::string("Sven2/gfx/Sven/anim_sven_blink_")+whistleFiles[i]+".bmp",2,1);
    auto kickFront=scene_.engine_.loadSprite("Sven2/gfx/Sheep/Anim_Sven_kick_01.bmp",20,1);
    auto kickBack=scene_.engine_.loadSprite("Sven2/gfx/Sheep/Anim_Sven_kick_02.bmp",18,1);
    kicks_={kickBack,kickFront,kickFront,kickBack};
    auto frigidaFront=scene_.engine_.loadSprite("Sven2/gfx/Frigida/anim_frigida_final_normal_front.bmp",3,1);
    auto frigidaBack=scene_.engine_.loadSprite("Sven2/gfx/Frigida/anim_frigida_final_normal_back.bmp",3,1);
    frigida_={frigidaBack,frigidaFront,frigidaFront,frigidaBack};



    // 0x40B040 installs two variants per animation; 0x4413DC supplies all 66 directions.
    interactions_=loadInteractionAnimations(scene_.engine_);


}
}
