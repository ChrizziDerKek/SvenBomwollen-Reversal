#include "Scene/Objects/ObjectCatalog.h"
#include "Scene/Level/LevelItems.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

LevelItems::LevelItems(LevelScene& scene)
    : Scene(scene)
{}

i32 LevelItems::FindDistraction(const Object& actor, ItemCode code) const
{
	// 0x4051D0 scans the immediate ring, then the outer ring, in this order.
	static constexpr i32 neighbors[24][2] = {
		{ 0, -1 },
		{ 1, -1 },
		{ 1, 0 },
		{ 1, 1 },
		{ 0, 1 },
		{ -1, 1 },
		{ -1, 0 },
		{ -1, -1 },
		{ 0, -2 },
		{ 1, -2 },
		{ 2, -2 },
		{ 2, -1 },
		{ 2, 0 },
		{ 2, 1 },
		{ 2, 2 },
		{ 1, 2 },
		{ 0, 2 },
		{ -1, 2 },
		{ -2, 2 },
		{ -2, 1 },
		{ -2, 0 },
		{ -2, -1 },
		{ -2, -2 },
		{ -1, -2 }
	};
	for (const i32* offset : neighbors)
	{
		i32 x = actor.Tile % Stride + offset[0], y = actor.Tile / Stride + offset[1];
		if (x < 1 || x > Columns || y < 1 || y > Rows)
			continue;
		i32 tile = y * Stride + x;
		if (Scene.Grid.ItemCodes[tile] == code && Scene.Grid.ItemObjects[tile] >= 0 && !Scene.Objects[Scene.Grid.ItemObjects[tile]].Freed)
			return Scene.Grid.ItemObjects[tile];
	}
	return -1;
}

void LevelItems::RemoveItem(i32 tile)
{
	if (tile < 0 || static_cast<size_t>(tile) >= Scene.Grid.ItemCodes.size())
		return;
	const i32 index = Scene.Grid.ItemObjects[tile];
	if (index >= 0 && static_cast<size_t>(index) < Scene.Objects.Size())
		Scene.Objects[index].Freed = true;
	Scene.Grid.ItemCodes[tile] = ItemCode::None;
	Scene.Grid.ItemObjects[tile] = -1;
}

void LevelItems::SpawnItem(ObjectKind kind, i32 tile)
{
	if (!IsItem(kind) || Scene.Grid.ItemCodes[tile] != ItemCode::None)
		return;
	size_t slot = Scene.Objects.Size();
	for (size_t i = 0; i < Scene.Objects.Size(); ++i)
	{
		if (Scene.Objects[i].Freed && i != Scene.PlayerIndex && static_cast<i32>(i) != Scene.Player.ActiveSheep && static_cast<i32>(i) != Scene.UfoController.Ufo.Target)
		{
			slot = i;
			break;
		}
	}
	if (slot >= 256)
		return; // original object pool also has 256 slots
	Object object;
	object.Kind = kind;
	object.Tile = tile;
	object.X = object.TargetX = static_cast<f32>((tile % Stride - 1) * 40 - 20);
	object.Y = object.TargetY = static_cast<f32>((tile / Stride - 1) * 40 - 20);
	object.CenterX = object.CenterY = 40;
	object.CommittedY = object.Y;
	object.DepthOffset = 20; // height 80, depth adjustment 60
	object.Draw = { Scene.Resources.ItemSprites[kind], static_cast<i32>(object.X), static_cast<i32>(object.Y), GetItemDefinition(kind).Frame };
	const ItemDefinition& definition = GetItemDefinition(kind);
	object.Pulse = Scene.Parameter(definition.LifetimeKey, definition.DefaultLifetime);
	if (slot == Scene.Objects.Size())
		Scene.Objects.Add(object);
	else
		Scene.Objects.Replace(slot, object);
	Scene.Grid.ItemCodes[tile] = definition.Code;
	Scene.Grid.ItemObjects[tile] = static_cast<i32>(slot);
}

void LevelItems::SpawnItems()
{
	// 0x411400 makes one attempt per present marker group, not one per marker.
	for (const auto& [group, tiles] : Scene.Grid.Spawners)
	{
		if (tiles.empty())
			continue;
		const i32 tile = tiles[Scene.RandomInt(static_cast<i32>(tiles.size()))];
		if (Scene.Grid.ItemCodes[tile] != ItemCode::None)
			continue;
		const OccupancyCode occupant = Scene.Grid.TileAt(Scene.Player.X, Scene.Player.Y) == tile ? OccupancyCode::Player : Scene.Grid.Tiles[tile];
		if (group != SpawnerCode::Powerup && IsEnemyOccupancy(occupant))
			continue;
		if (group != SpawnerCode::Distraction && occupant == OccupancyCode::Player)
			continue;
		std::vector<ObjectKind> choices;
		for (const ItemDefinition& item : GetItemDefinitions())
			if (group == SpawnerCode::Mixed || (group == SpawnerCode::Powerup ? IsPowerup(item.Kind) : !IsPowerup(item.Kind)))
				choices.push_back(item.Kind);
		SpawnItem(choices[Scene.RandomInt(static_cast<i32>(choices.size()))], tile);
	}
}

void LevelItems::UpdateItems(f32 seconds)
{
	EffectAge += seconds;
	if (PlayerEffect != ItemCode::None)
	{
		EffectRemaining -= seconds;
		if (EffectRemaining < 0)
		{
			PlayerEffect = ItemCode::None;
			EffectRemaining = 0;
		}
	}
	SpawnClock -= seconds;
	if (SpawnClock < 0)
	{
		SpawnClock = std::max(0.01f, SpawnPeriod);
		SpawnItems();
	}
	Scene.Objects.UpdatePhase(ObjectPhase::Items, seconds);
}

void LevelItems::AffectNearbySheep()
{
	if (PlayerEffect != ItemCode::StinkMushroom && PlayerEffect != ItemCode::Flower)
		return;
	const i32 tile = Scene.Grid.TileAt(Scene.Player.X, Scene.Player.Y);
	if (tile < 0)
		return;
	const i32 mood = PlayerEffect == ItemCode::StinkMushroom ? 10 : 0;
	bool changed = false;
	for (WorldObject& animal : Scene.Objects)
	{
		if (animal.Freed || animal.Kind != ObjectKind::Sheep || animal.Temperament == 2 || animal.Impatience == mood || animal.Tile < 0)
			continue;
		const i32 dx = std::abs(animal.Tile % Stride - tile % Stride);
		const i32 dy = std::abs(animal.Tile / Stride - tile / Stride);
		if (dx <= 1 && dy <= 1 && (dx || dy))
		{
			animal.Impatience = mood;
			changed = true;
		}
	}
	if (changed)
	{
		cstr sound = mood == 10 ? (Scene.RandomInt(2) == 0 ? "Sheep_beamup.ogg" : "Sheep_angry2.ogg") : (Scene.RandomInt(2) == 0 ? "Sheep_happy1.ogg" : "Sheep_happy2.ogg");
		Scene.Sounds.push_back(str("Sven2/sfx/") + sound);
	}
}

void LevelItems::PickupPlayerItem()
{
	if (Scene.Objects.Combat.Enemy >= 0 || Scene.Player.PlayerBeamState || Scene.Effects.SplashPlayerAge * Scene.Player.DefaultRate < 16)
		return;
	const i32 tile = Scene.Grid.TileAt(Scene.Player.X, Scene.Player.Y);
	if (tile < 0)
		return;
	const ItemCode code = Scene.Grid.ItemCodes[tile];
	if (code == ItemCode::Artifact)
	{
		if (PlayerEffect == ItemCode::AlienPower || Scene.UfoController.Ufo.State != UfoState::Inactive)
			return;
		RemoveItem(tile);
		Scene.UfoController.RequestUfo(static_cast<i32>(Scene.PlayerIndex));
		return;
	}
	if (!IsPlayerPowerup(code) || (code != ItemCode::Gift && PlayerEffect == ItemCode::AlienPower))
		return;
	RemoveItem(tile);
	if (code == ItemCode::Gift)
	{
		// Nested independent rand tests at 0x42AA71, not a flat weighted draw.
		if (Scene.RandomInt(50) == 0)
		{
			++LivesGained;
		}
		else if (Scene.RandomInt(25) == 0)
			Scene.Effects.AddTime(25, Scene.Player.X + 60, Scene.Player.Y + 60);
		else if (Scene.RandomInt(15) == 0)
			Scene.Effects.AddTime(15, Scene.Player.X + 60, Scene.Player.Y + 60);
		else if (Scene.RandomInt(10) == 0)
			Scene.Effects.AddTime(10, Scene.Player.X + 60, Scene.Player.Y + 60);
		else
			Scene.Effects.AddTime(5, Scene.Player.X + 60, Scene.Player.Y + 60);
		Scene.Sounds.push_back("Sven2/sfx/item_gift.ogg");
		return;
	}
	PlayerEffect = code;
	EffectAge = 0;
	const ItemDefinition& definition = GetItemDefinition(code);
	EffectRemaining = Scene.Parameter(definition.EffectDurationKey, definition.DefaultEffectDuration);
	Scene.Sounds.push_back(str("Sven2/sfx/") + definition.PickupSound);
}

void LevelItems::UpdateObject(size_t i, f32 seconds)
{
	WorldObject& object = Scene.Objects[i];
	if (object.Freed)
		return;
	if (object.Kind == ObjectKind::Scenery && object.State == ActorState::SceneryMoving)
	{
		f32 velocity;
		std::memcpy(&velocity, &object.Original[7], 4);
		object.X += velocity * seconds;
		if (object.X < -400)
		{
			object.X = 800;
			velocity = static_cast<f32>(-15 - Scene.RandomInt(35));
			std::memcpy(&object.Original[7], &velocity, 4);
		}
		object.Draw.X = static_cast<i32>(object.X);
	}
	if (IsItem(object.Kind) && (!object.Permanent || IsPowerup(object.Kind)))
	{
		object.Pulse -= seconds;
		if (object.Pulse < 0)
			RemoveItem(object.Tile);
	}
}
