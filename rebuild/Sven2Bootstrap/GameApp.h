#pragma once
#include "EngineAbi.h"

namespace sven2 {
void runGame(Engine& engine,const std::filesystem::path& saves,bool audio,unsigned seconds,bool directTutorial);
}
