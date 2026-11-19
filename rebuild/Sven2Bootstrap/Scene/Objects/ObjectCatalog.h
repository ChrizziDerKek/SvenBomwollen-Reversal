#pragma once
#include "ObjectKind.h"
#include "Engine/GameEdition.h"
#include "Scene/Level/LevelCodes.h"
#include <memory>

class BaseObject;
class LevelScene;
class GameData;

enum class CharacterCategory
{
	Player,
	Enemy,
	Animal,
	Mermaid,
	Emitter,
	Effect
};

struct CharacterDefinition
{
	ObjectKind Kind;
	CharacterCategory Category;
	cstr Name;
	cstr SpriteAsset;
	u16 Frames;
	::OccupancyCode OccupancyCode;
	std::unique_ptr<BaseObject> (*Create)(LevelScene&, size_t);
	cstr SpriteName = nullptr;
	bool XxxOnly = false;
	bool DebugSpawnable = true;
	u16 GetFrames(const GameData& data, GameEdition edition) const;
};

// The registration table is the single mapping from serialized character IDs
// to their class, classification and standing resources.
const CharacterDefinition* FindCharacterDefinition(ObjectKind serializedKind);
const std::vector<CharacterDefinition>& GetCharacterDefinitions();

struct ItemDefinition
{
	ObjectKind Kind;
	ItemCode Code;
	cstr SpriteAsset;
	u16 Frames;
	i32 Frame;
	cstr LifetimeKey;
	f32 DefaultLifetime;
	cstr EffectDurationKey;
	f32 DefaultEffectDuration;
	cstr PickupSound;
	cstr Name;
};

const ItemDefinition* FindItemDefinition(ObjectKind kind);
const std::vector<ItemDefinition>& GetItemDefinitions();
const ItemDefinition& GetItemDefinition(ObjectKind kind);
const ItemDefinition& GetItemDefinition(ItemCode code);
bool IsItem(ObjectKind kind);
bool IsPowerup(ObjectKind kind);
ItemCode DistractionCode(ObjectKind kind);
size_t EnemyTableIndex(ObjectKind kind);
size_t AnimalIndicatorIndex(ObjectKind kind);
