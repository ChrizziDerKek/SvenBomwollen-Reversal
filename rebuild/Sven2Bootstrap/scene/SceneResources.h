#pragma once
#include "scene/SceneTypes.h"
#include <map>

namespace sven2 {
class LevelScene;
class SceneResources {
    using Object=WorldObject;
    using Interaction=InteractionAnimation;
public:
    explicit SceneResources(LevelScene& scene);
    SceneResources(const SceneResources&)=delete;
    SceneResources& operator=(const SceneResources&)=delete;
    const Sprite* sprite(const std::string& path,int frames);
    void initializeActors();
    void initializePlayer();
private:
    friend class AnimalCharacters;
    friend class EnemyCharacters;
    friend class LevelEffects;
    friend class LevelItems;
    friend class LevelScene;
    friend class PlayerCharacter;
    friend class SceneRenderer;
    LevelScene& scene_;
    const Sprite *explosion_=nullptr,*hearts_=nullptr;
    const Sprite *digging_=nullptr,*sleepSymbols_=nullptr;
    std::array<const Sprite*,4> playerBeamSprites_{};
    std::array<ActorSprites,14> actorSprites_{};
    std::array<EnemyRules,7> enemyRules_{};
    std::array<std::array<Interaction,4>,3> animalActions_{};
    std::map<std::string,const Sprite*> spriteCache_;
    std::array<const Sprite*,19> itemSprites_{};
    std::array<const Sprite*,5> effectSprites_{};
    const Sprite* ufoSprite_=nullptr;
    const Sprite* ufoShadow_=nullptr;
    const Sprite* beamSprite_=nullptr;
    const Sprite* moodSprite_=nullptr;
    const Sprite* kettleSprite_=nullptr;
    const Sprite* stand_=nullptr;
    const Sprite* sheepStand_=nullptr;
    const Sprite* chewing_=nullptr;
    std::array<const Sprite*,4> demonSheep_{},demonWalks_{};
    std::array<const Sprite*,8> electric_{};
    const Sprite* flash_=nullptr;
    std::array<const Sprite*,4> turns_{};
    std::array<const Sprite*,8> whistles_{};
    std::array<const Sprite*,8> walks_{};
    std::array<const Sprite*,4> sheepWalks_{};
    std::vector<Interaction> interactions_;
    std::array<const Sprite*,8> blinks_{};
    std::array<const Sprite*,4> kicks_{}, frigida_{};
};
}
