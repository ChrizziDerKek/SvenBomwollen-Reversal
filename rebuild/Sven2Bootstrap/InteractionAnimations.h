#pragma once
#include "EngineAbi.h"

namespace sven2 {
struct InteractionAnimation {
    const Sprite* sprite=nullptr;
    std::vector<int> frames;
    std::string sound;
    int facing=0;
    bool mirror=false;
};
std::vector<InteractionAnimation> loadInteractionAnimations(Engine& engine);
}
