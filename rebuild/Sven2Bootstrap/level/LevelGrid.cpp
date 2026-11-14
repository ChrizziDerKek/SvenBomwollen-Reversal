#include "level/LevelGrid.h"
#include "LevelScene.h"
#include "level/LevelFile.h"
#include "scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sven2 {
using namespace scene_data;
LevelGrid::LevelGrid(LevelScene& scene):scene_(scene) {}

void LevelGrid::load(const LevelFile& file) {
    reservations_.fill(-1);
    for(auto& group:spawners_) group.clear();
    for(size_t tile=0;tile<tiles_.size();++tile) {
        const auto& cell=file.cells()[tile];
        tiles_[tile]=cell.terrain;
        tileDirections_[tile]=cell.direction;
        itemCodes_[tile]=cell.itemCode;
        itemObjects_[tile]=cell.itemObject;
        if(cell.spawner>=140 && cell.spawner<=142) spawners_[cell.spawner-140].push_back(static_cast<int>(tile));
    }
    puddles_=file.puddles();
}

int LevelGrid::tileAt(float x, float y) const {
    x+=playerCenter;
    y+=playerCenter;
    if(x<0 || y<0 || x>=columns*tileSize || y>=rows*tileSize) return -1;
    return (static_cast<int>(y)/tileSize+1)*stride+static_cast<int>(x)/tileSize+1;
}

bool LevelGrid::unoccupied(int tile,size_t except) const {
    if(tile<0 || static_cast<size_t>(tile)>=tiles_.size()) return false;
    if(tiles_[tile]!=empty && tiles_[tile]!=100) return false;
    if(reservations_[tile]>=0 && reservations_[tile]!=static_cast<int>(except)) return false;
    for(size_t i=0;i<scene_.world_.size();++i) {
        if(i==except || scene_.world_[i].freed || (!scene_.isAnimal(scene_.world_[i].kind) && !scene_.isEnemy(scene_.world_[i].kind))) continue;
        if(scene_.world_[i].tile==tile || tileAt(scene_.world_[i].targetX,scene_.world_[i].targetY)==tile) return false;
    }
    if(except!=scene_.playerIndex_ && (tileAt(scene_.player_.x_,scene_.player_.y_)==tile || tileAt(scene_.player_.targetX_,scene_.player_.targetY_)==tile)) return false;
    return true;
}
}
