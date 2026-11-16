#pragma once
#include "ObjectKind.h"
#include <memory>

class BaseObject;
class LevelScene;

enum class CharacterCategory
{
	Player,
	Enemy,
	Animal
};

struct CharacterDefinition
{
	ObjectKind Kind;
	CharacterCategory Category;
	cstr Name;
	cstr SpriteAsset;
	u16 Frames;
	i32 OccupancyCode;
	std::unique_ptr<BaseObject> (*Create)(LevelScene&, size_t);
};

// The registration table is the single mapping from serialized character IDs
// to their class, classification and standing resources.
const CharacterDefinition* FindCharacterDefinition(i32 serializedKind);
