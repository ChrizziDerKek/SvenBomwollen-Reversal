#pragma once
#include "scene/SceneTypes.h"

namespace sven2 {
class LevelScene;
class LevelFile;
class LevelGrid {
    using Object=WorldObject;
    using Interaction=InteractionAnimation;
public:
    explicit LevelGrid(LevelScene& scene);
    LevelGrid(const LevelGrid&)=delete;
    LevelGrid& operator=(const LevelGrid&)=delete;
    void load(const LevelFile& file);
    int tileAt(float x, float y) const;
    bool unoccupied(int tile, size_t except) const;
private:
    friend class AnimalCharacters;
    friend class EnemyCharacters;
    friend class LevelItems;
    friend class LevelScene;
    friend class PlayerCharacter;
    LevelScene& scene_;
    std::array<std::vector<int>,3> spawners_;
    std::array<int,22*17> tiles_{};
    std::array<unsigned char,22*17> itemCodes_{};
    std::array<int,22*17> itemObjects_{}, reservations_{};
    std::array<unsigned char,22*17> tileDirections_{};
    std::vector<int> puddles_;
};
}
