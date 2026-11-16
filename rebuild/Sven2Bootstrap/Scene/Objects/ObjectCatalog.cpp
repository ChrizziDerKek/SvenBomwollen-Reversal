#include "ObjectCatalog.h"
#include "Scene/Characters/Characters.h"
#include "Scene/Characters/PlayerCharacter.h"
#include <array>

namespace
{
template <class T>
std::unique_ptr<BaseObject> Create(LevelScene& scene, size_t index)
{
	return std::make_unique<T>(scene, index);
}
const CharacterDefinition Definitions[] = {{ObjectKind::Player, CharacterCategory::Player, "Sven", "Sven2/gfx/Sven/sven_stand.bmp", 8, 0, nullptr},
                                           {ObjectKind::Sheep, CharacterCategory::Animal, "Sheep", "Sven2/gfx/Sheep/Sheep_Stand.bmp", 4, 2, Create<Sheep>},
                                           {ObjectKind::Alien, CharacterCategory::Enemy, "Alien", "Sven2/gfx/Alien/Alien_Stand.bmp", 8, 3, Create<Alien>},
                                           {ObjectKind::Dog, CharacterCategory::Enemy, "Dog", "Sven2/gfx/Dog/Dog_Stand.bmp", 8, 4, Create<Dog>},
                                           {ObjectKind::Farmer, CharacterCategory::Enemy, "Farmer", "Sven2/gfx/Guy/Guy_Stand.bmp", 8, 5, Create<Farmer>},
                                           {ObjectKind::Girl, CharacterCategory::Enemy, "Girl", "Sven2/gfx/Broemse/Broemse_stand.bmp", 8, 6, Create<Girl>},
                                           {ObjectKind::Chicken, CharacterCategory::Animal, "Chicken", "Sven2/gfx/Animals/Chicken_Stand.bmp", 4, 7, Create<Chicken>},
                                           {ObjectKind::Goat, CharacterCategory::Animal, "Goat", "Sven2/gfx/Animals/Goat_Stand.bmp", 4, 8, Create<Goat>},
                                           {ObjectKind::Pig, CharacterCategory::Animal, "Pig", "Sven2/gfx/Animals/Pig_Stand.bmp", 4, 9, Create<Pig>}};
} // namespace

const CharacterDefinition* FindCharacterDefinition(i32 serializedKind)
{
	for (const CharacterDefinition& definition : Definitions)
		if (static_cast<i32>(definition.Kind) == serializedKind)
			return &definition;
	return nullptr;
}
