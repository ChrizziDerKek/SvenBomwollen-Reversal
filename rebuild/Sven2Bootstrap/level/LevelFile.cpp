#include "level/LevelFile.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sven2 {
namespace {
template<class T> T readWord(const unsigned char* bytes,size_t offset) {
    static_assert(sizeof(T)==4);
    T value;std::memcpy(&value,bytes+offset,4);return value;
}
}
LevelFile::LevelFile(const std::vector<unsigned char>& bytes,const std::string& path) {
    constexpr size_t headerSize=16,gridAndStateSize=9048,objectBytes=100;
    if(bytes.size()<headerSize || readWord<int>(bytes.data(),0)!=6)
        throw std::runtime_error("Expected a Sven2 v6 level.");
    hidesUfo_=readWord<int>(bytes.data(),8)!=0;
    const auto nameSize=readWord<std::uint32_t>(bytes.data(),12);
    if(nameSize>bytes.size()-headerSize || bytes.size()-headerSize-nameSize!=gridAndStateSize+objectCount*objectBytes)
        throw std::runtime_error("Unexpected v6 level length: "+path);
    const auto* grid=bytes.data()+headerSize+nameSize;
    for(size_t index=0;index<cells_.size();++index) {
        const auto* cell=grid+index*24;
        cells_[index]={readWord<int>(cell,0),cell[4],cell[12],readWord<int>(cell,16),readWord<int>(cell,20)};
    }
    timeLimit_=readWord<float>(grid,9044);
    if(!std::isfinite(timeLimit_) || timeLimit_<0) throw std::runtime_error("Invalid level timer.");
    const auto* destinations=grid+cellCount*24;
    const int count=std::clamp(readWord<int>(destinations,0),0,16);
    for(size_t slot=1;slot<=16 && puddles_.size()<static_cast<size_t>(count);++slot) {
        const int tile=readWord<int>(destinations,slot*4);
        if(tile>=0 && static_cast<size_t>(tile)<cells_.size() && cells_[tile].terrain==10 &&
           std::find(puddles_.begin(),puddles_.end(),tile)==puddles_.end()) puddles_.push_back(tile);
    }
    for(size_t slot=0;slot<objects_.size();++slot)
        std::memcpy(objects_[slot].words_.data(),grid+gridAndStateSize+slot*objectBytes,objectBytes);
}
}
