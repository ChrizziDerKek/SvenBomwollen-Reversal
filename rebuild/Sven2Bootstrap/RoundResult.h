#pragma once

namespace sven2 {
struct RoundResult {
    int score=0,level=1;
    int time=0,sheep=0,lives=0;
    int timeRemaining=0,sheepRemaining=0,livesRemaining=0;
    bool episodeComplete=false,gameOver=false;
    int totalSheep=0;
    int scoreFrame=0;
};
}
