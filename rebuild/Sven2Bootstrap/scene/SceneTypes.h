#pragma once
#include "EngineAbi.h"
#include "InteractionAnimations.h"
#include <array>

namespace sven2 {
enum class AnimationSequence:unsigned;

struct WorldObject {
    SpriteDraw draw{};
    double depthOffset=0;
    int kind=1, tile=-1, facing=0, impatience=6, temperament=0;
    int state=0, turnTo=-1, angerTicks=0, attempts=0;
    float committedY=0, age=0, idle=0, pulse=30, x=0, y=0, targetX=0, targetY=0;
    bool freed=false;
    int moodBlink=0, idleVariant=0;
    bool idleAnimating=false;
    int itemTarget=-1, steps=0, permanent=0;
    float soundClock=0;
    float centerX=60, centerY=60, animRate=20, lift=0;
    bool hunting=false, retry=false;
    std::array<std::uint32_t,25> original{};
};
struct TimePopup { float x,y,speed,life; int amount; };
struct ActorSprites {
    const Sprite* stand=nullptr;
    std::array<const Sprite*,8> walk{}, idle{};
    std::array<const Sprite*,4> sleep{}, wake{}, beam{}, distraction{}, battle{}, grumble{}, dogPlay{};
};
struct EnemyRules {
    float speed=0, run=1, diagonal=0.75f, walkRate=15, runRate=30;
    float idle=4, idleRandom=1, sleep=15, wakeRange=2, range=3, fightTime=1.5f;
    int huntSteps=20, walkSteps=5, walkRandom=2;
};
struct UfoFlight {
    int state=-1, target=-1, targetTile=-1;
    float x=0,y=0,targetX=0,targetY=0,vx=0,vy=0;
    float radius=900,angleX=0,angleY=0,angularSpeed=0.01f,clock=0,age=0,beamAge=0;
    bool collectAnimals=false, beam=false;
};
}
