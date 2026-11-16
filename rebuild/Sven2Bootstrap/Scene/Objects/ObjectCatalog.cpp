#include "ObjectCatalog.h"
#include "Scene/Characters/Characters.h"
#include "Scene/Characters/PlayerCharacter.h"
#include <array>
#include <stdexcept>

template <class T>
std::unique_ptr<BaseObject> Create(LevelScene& scene, size_t index)
{
	return std::make_unique<T>(scene, index);
}

const std::vector<CharacterDefinition> Definitions = {
	{ ObjectKind::Player, CharacterCategory::Player, "Sven", "Sven2/gfx/Sven/sven_stand.bmp", 8, OccupancyCode::Empty, nullptr },
	{ ObjectKind::Sheep, CharacterCategory::Animal, "Sheep", "Sven2/gfx/Sheep/Sheep_Stand.bmp", 4, OccupancyCode::Sheep, Create<Sheep> },
	{ ObjectKind::Alien, CharacterCategory::Enemy, "Alien", "Sven2/gfx/Alien/Alien_Stand.bmp", 8, OccupancyCode::Alien, Create<Alien> },
	{ ObjectKind::Dog, CharacterCategory::Enemy, "Dog", "Sven2/gfx/Dog/Dog_Stand.bmp", 8, OccupancyCode::Dog, Create<Dog> },
	{ ObjectKind::Farmer, CharacterCategory::Enemy, "Farmer", "Sven2/gfx/Guy/Guy_Stand.bmp", 8, OccupancyCode::Farmer, Create<Farmer> },
	{ ObjectKind::Girl, CharacterCategory::Enemy, "Girl", "Sven2/gfx/Broemse/Broemse_stand.bmp", 8, OccupancyCode::Girl, Create<Girl> },
	{ ObjectKind::Chicken, CharacterCategory::Animal, "Chicken", "Sven2/gfx/Animals/Chicken_Stand.bmp", 4, OccupancyCode::Chicken, Create<Chicken> },
	{ ObjectKind::Goat, CharacterCategory::Animal, "Goat", "Sven2/gfx/Animals/Goat_Stand.bmp", 4, OccupancyCode::Goat, Create<Goat> },
	{ ObjectKind::Pig, CharacterCategory::Animal, "Pig", "Sven2/gfx/Animals/Pig_Stand.bmp", 4, OccupancyCode::Pig, Create<Pig> }
};

const std::vector<CharacterDefinition>& GetCharacterDefinitions()
{
	return Definitions;
}

const CharacterDefinition* FindCharacterDefinition(ObjectKind serializedKind)
{
	for (const CharacterDefinition& definition : Definitions)
		if (definition.Kind == serializedKind)
			return &definition;
	return nullptr;
}

const std::vector<ItemDefinition> Items = {
	{ ObjectKind::Beer, ItemCode::Beer, "Sven2/gfx/Beer.bmp", 1, 0, "BeerTime", 16, nullptr, 0, nullptr, "Wine bottle" },
	{ ObjectKind::Teddy, ItemCode::Teddy, "Sven2/gfx/Teddybear.bmp", 1, 0, "TeddyTime", 16, nullptr, 0, nullptr, "Teddy bear" },
	{ ObjectKind::Bone, ItemCode::Bone, "Sven2/gfx/Bone.bmp", 1, 0, "BoneTime", 16, nullptr, 0, nullptr, "Bone" },
	{ ObjectKind::Artifact, ItemCode::Artifact, "Sven2/gfx/Artifact.bmp", 1, 0, "ArtifactTime", 16, nullptr, 0, nullptr, "Alien plant" },
	{ ObjectKind::Rocket, ItemCode::Rocket, "Sven2/gfx/icons.bmp", 5, 0, "FireworkTime", 10, "FireworkDuration", 10, "item_firework.ogg", "Rocket" },
	{ ObjectKind::Gift, ItemCode::Gift, "Sven2/gfx/icons.bmp", 5, 1, "GiftTime", 10, nullptr, 0, "item_gift.ogg", "Present" },
	{ ObjectKind::SlowMushroom, ItemCode::SlowMushroom, "Sven2/gfx/icons.bmp", 5, 2, "ImpotentTime", 10, "ImpotentDuration", 10, "item_impotenz.ogg", "Slow mushroom" },
	{ ObjectKind::StinkMushroom, ItemCode::StinkMushroom, "Sven2/gfx/icons.bmp", 5, 3, "SmellTime", 10, "SmellDuration", 15, "item_stinky.ogg", "Stinky mushroom" },
	{ ObjectKind::Flower, ItemCode::Flower, "Sven2/gfx/icons.bmp", 5, 4, "FlowerTime", 10, "FlowerDuration", 10, "item_rose.ogg", "Flower" }
};

const ItemDefinition* FindItemDefinition(ObjectKind kind)
{
	for (const ItemDefinition& item : Items)
		if (item.Kind == kind)
			return &item;
	return nullptr;
}

const std::vector<ItemDefinition>& GetItemDefinitions()
{
	return Items;
}

const ItemDefinition& GetItemDefinition(ObjectKind kind)
{
	const ItemDefinition* item = FindItemDefinition(kind);
	if (!item)
		throw std::invalid_argument("Object kind is not a registered item.");
	return *item;
}

const ItemDefinition& GetItemDefinition(ItemCode code)
{
	for (const ItemDefinition& item : Items)
		if (item.Code == code)
			return item;
	throw std::invalid_argument("Cell code is not a registered item.");
}

bool IsItem(ObjectKind kind)
{
	return FindItemDefinition(kind) != nullptr;
}

bool IsPowerup(ObjectKind kind)
{
	const ItemDefinition* item = FindItemDefinition(kind);
	return item && IsPlayerPowerup(item->Code);
}

ItemCode DistractionCode(ObjectKind kind)
{
	switch (kind)
	{
	case ObjectKind::Dog:
		return ItemCode::Bone;
	case ObjectKind::Farmer:
		return ItemCode::Beer;
	case ObjectKind::Girl:
		return ItemCode::Teddy;
	default:
		return ItemCode::None;
	}
}

size_t EnemyTableIndex(ObjectKind kind)
{
	switch (kind)
	{
	case ObjectKind::Alien:
		return 0;
	case ObjectKind::Dog:
		return 1;
	case ObjectKind::Farmer:
		return 2;
	case ObjectKind::Girl:
		return 3;
	default:
		throw std::invalid_argument("Object kind has no enemy steering table.");
	}
}

size_t AnimalIndicatorIndex(ObjectKind kind)
{
	switch (kind)
	{
	case ObjectKind::Sheep:
		return 0;
	case ObjectKind::Chicken:
		return 1;
	case ObjectKind::Goat:
		return 2;
	case ObjectKind::Pig:
		return 3;
	default:
		throw std::invalid_argument("Object kind has no animal indicator.");
	}
}
