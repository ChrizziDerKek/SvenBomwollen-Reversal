#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace sven2 {
// Serialized v6 records stay separate from mutable simulation state. Unknown
// words are retained for the few original behaviors that still consume them.
class LevelObjectRecord {
public:
    int kind() const { return integer(11); }
    int renderPass() const { return integer(4); }
    int width() const { return integer(5); }
    int height() const { return integer(6); }
    int frame() const { return integer(12); }
    int textureOrPermanent() const { return integer(17); }
    int state() const { return integer(18); }
    int facing() const { return integer(19); }
    int tile() const { return integer(20); }
    int impatience() const { return integer(21); }
    int depthAdjustment() const { return integer(23); }
    float x() const { return real(0); }
    float y() const { return real(1); }
    float idle() const { return real(13); }
    float frameRate() const { return real(14); }
    float pulse() const { return real(22); }
    unsigned char angerTicks() const { return byte(64); }
    unsigned char temperament() const { return byte(65); }
    unsigned char attempts() const { return byte(66); }
    unsigned char moodBlink() const { return byte(67); }
    bool hunting() const { return byte(96)!=0; }
    const std::array<std::uint32_t,25>& words() const { return words_; }
private:
    friend class LevelFile;
    int integer(size_t index) const { int value;std::memcpy(&value,&words_.at(index),4);return value; }
    float real(size_t index) const { float value;std::memcpy(&value,&words_.at(index),4);return value; }
    unsigned char byte(size_t offset) const { return static_cast<unsigned char>(words_.at(offset/4)>>(8*(offset%4))); }
    std::array<std::uint32_t,25> words_{};
};

struct LevelCell {
    int terrain=0;
    unsigned char direction=0,itemCode=0;
    int itemObject=-1,spawner=0;
};

class LevelFile {
public:
    static constexpr size_t cellCount=22*17,objectCount=256;
    LevelFile(const std::vector<unsigned char>& bytes,const std::string& path);
    bool hidesUfo() const { return hidesUfo_; }
    float timeLimit() const { return timeLimit_; }
    const std::array<LevelCell,cellCount>& cells() const { return cells_; }
    const std::array<LevelObjectRecord,objectCount>& objects() const { return objects_; }
    const std::vector<int>& puddles() const { return puddles_; }
private:
    bool hidesUfo_=false;
    float timeLimit_=0;
    std::array<LevelCell,cellCount> cells_{};
    std::array<LevelObjectRecord,objectCount> objects_{};
    std::vector<int> puddles_;
};
}
