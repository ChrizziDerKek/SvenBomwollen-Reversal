
#include "Scene/SceneConstants.h"
#include "ObjectCatalog.h"
#include "ObjectManager.h"
#include "Scene/LevelScene.h"
#include "Scene/Characters/Characters.h"
#include <stdexcept>

class SceneryObject final : public BaseObject
{
public:
	SceneryObject(LevelScene& scene, size_t index)
	    : BaseObject(scene, ObjectPhase::Items, index)
	{}
	void Update(f32 seconds) override;
};

class ItemObject final : public BaseObject
{
public:
	ItemObject(LevelScene& scene, size_t index)
	    : BaseObject(scene, ObjectPhase::Items, index)
	{}
	void Update(f32 seconds) override;
};

class EffectObject final : public BaseObject
{
public:
	EffectObject(LevelScene& scene, size_t index)
	    : BaseObject(scene, ObjectPhase::Effects, index)
	{}
	void Update(f32 seconds) override;
};

ObjectManager::ObjectManager(LevelScene& scene)
    : Scene(scene)
{
	for (size_t i = 0; i < PendingPlayers.size(); ++i)
	{
		PendingPlayers[i] = std::make_unique<PlayerCharacter>(scene, i);
		Players[i] = PendingPlayers[i].get();
	}
}

ObjectManager::~ObjectManager() = default;

PlayerCharacter& ObjectManager::GetPlayer(size_t number)
{
	return *Players.at(number);
}

const PlayerCharacter& ObjectManager::GetPlayer(size_t number) const
{
	return *Players.at(number);
}

std::unique_ptr<BaseObject> ObjectManager::Create(const WorldObject& state, size_t index)
{
	std::unique_ptr<BaseObject> object;
	const CharacterDefinition* definition = FindCharacterDefinition(state.Kind);
	if (definition && definition->Create)
		object = definition->Create(Scene, index);
	else
		switch (state.Kind)
		{
		case ObjectKind::Player:
			if (AdoptedPlayers >= Scene.GetPlayerCount())
				throw std::runtime_error("Duplicate player object.");
			object = std::move(PendingPlayers[AdoptedPlayers++]);
			break;

		case ObjectKind::Scenery:
			object = std::make_unique<SceneryObject>(Scene, index);
			break;
		case ObjectKind::CompletionEffect:
			object = std::make_unique<EffectObject>(Scene, index);
			break;
		default:
			object = std::make_unique<ItemObject>(Scene, index);
			break;
		}
	object->SlotIndex = index;
	object->State = state;
	return object;
}

void ObjectManager::Add(const WorldObject& state)
{
	Objects.push_back(Create(state, Size()));
	Objects.back()->Initialize();
}

void ObjectManager::Replace(size_t index, const WorldObject& state)
{
	if (dynamic_cast<PlayerCharacter*>(Objects.at(index).get()))
		throw std::runtime_error("Cannot replace the player instance.");
	if (!Objects.at(index)->GetState().Freed)
		throw std::runtime_error("Cannot reuse a live object slot.");
	Objects[index] = Create(state, index);
	Objects[index]->Initialize();
}

void ObjectManager::UpdatePhase(ObjectPhase phase, f32 seconds)
{
	for (size_t index = 0; index < Size(); ++index)
	{
		BaseObject& object = Get(index);
		// Freed sheep retain their age clock in the original simulation.
		if (object.GetPhase() == phase && (!object.GetState().Freed || phase == ObjectPhase::Sheep))
			object.Update(seconds);
	}
}

std::vector<WorldObject> ObjectManager::CopyStates() const
{
	std::vector<WorldObject> result;
	result.reserve(Size());
	for (size_t index = 0; index < Size(); ++index)
		result.push_back((*this)[index]);
	return result;
}

std::vector<size_t> ObjectManager::Find(ObjectKind kind, bool includeFreed) const
{
	std::vector<size_t> result;
	for (size_t index = 0; index < Size(); ++index)
		if ((*this)[index].Kind == kind && (includeFreed || !(*this)[index].Freed))
			result.push_back(index);
	return result;
}

void SceneryObject::Update(f32 seconds)
{
	Scene.GetItems().UpdateObject(GetIndex(), seconds);
}

void ItemObject::Update(f32 seconds)
{
	Scene.GetItems().UpdateObject(GetIndex(), seconds);
}

void EffectObject::Update(f32 seconds)
{
	Scene.GetEffects().UpdateWorldEffect(GetIndex(), seconds);
}

bool ObjectManager::PlayerInRange(const WorldObject& actor, f32 distance) const
{
	if (Scene.GetCombat().Enemy >= 0 || Scene.GetPlayer().PlayerBeamState || Scene.GetPlayer().PlayerShockAge >= 0 || Scene.GetPlayer().SplashAge * Scene.GetPlayer().DefaultRate < 16)
		return false;
	if (actor.Tile < 0)
		return false;
	const i32 playerTile = Scene.Grid.TileAt(Scene.GetPlayer().X, Scene.GetPlayer().Y);
	const i32 dx = actor.Tile % Stride - playerTile % Stride;
	const i32 dy = actor.Tile / Stride - playerTile / Stride;
	const i32 bound = static_cast<i32>(distance);
	return playerTile >= 0 && dx > -bound && dx < bound && dy > -bound && dy < bound;
}

bool ObjectManager::CanCatchPlayer(const WorldObject& actor) const
{
	if (Scene.GetCombat().Enemy >= 0 || Scene.GetPlayer().PlayerBeamState || Scene.GetPlayer().PlayerShockAge >= 0 || Scene.GetPlayer().SplashAge * Scene.GetPlayer().DefaultRate < 16)
		return false;
	const f32 dx = Scene.GetPlayer().X + 60 - actor.X - actor.CenterX;
	const f32 dy = Scene.GetPlayer().Y + 60 - actor.Y - actor.CenterY;
	if (dx * dx + dy * dy > 55 * 55)
		return false;
	const i32 playerTile = Scene.Objects[Scene.GetPlayerIndex()].Tile;
	for (i32 offset = -1; offset <= 1; ++offset)
	{
		const i32* d = Directions[(actor.Facing + offset + 8) % 8];
		if (actor.Tile + d[0] + Stride * d[1] == playerTile)
			return true;
	}
	return false;
}

bool ObjectManager::CanOccupy(i32 tile, size_t except) const
{
	return Scene.Grid.Unoccupied(tile, except);
}
