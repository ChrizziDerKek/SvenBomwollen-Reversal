#pragma once

namespace sven2 {
class LevelScene;

// Scene scheduling uses this interface without changing native world slots or
// allocating a new polymorphic object for each serialized actor record.
class CharacterController {
public:
    virtual ~CharacterController()=default;
    virtual void update(float seconds)=0;
    CharacterController(const CharacterController&)=delete;
    CharacterController& operator=(const CharacterController&)=delete;
protected:
    explicit CharacterController(LevelScene& scene):scene_(scene) {}
    LevelScene& scene_;
};
}
