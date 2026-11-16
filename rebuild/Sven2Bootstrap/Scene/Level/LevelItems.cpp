#include "Scene/Level/LevelItems.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

LevelItems::LevelItems(LevelScene& scene)
    : Scene(scene)
{
}

i32 LevelItems::FindDistraction(const Object& actor, i32 code) const
{
	// 0x4051D0 scans the immediate ring, then the outer ring, in this order.
	static constexpr i32 neighbors[24][2] = {
	    {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -2}, {1, -2}, {2, -2}, {2, -1}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {-1, 2}, {-2, 2}, {-2, 1}, {-2, 0}, {-2, -1}, {-2, -2}, {-1, -2}};
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
	Scene.Grid.ItemCodes[tile] = 0;
	Scene.Grid.ItemObjects[tile] = -1;
}

void LevelItems::SpawnItem(i32 kind, i32 tile)
{
	if (kind < Kinds::Beer || kind > Kinds::Flower || Scene.IsAnimal(kind) || Scene.Grid.ItemCodes[tile])
		return;
	size_t slot = Scene.Objects.Size();
	for (size_t i = 0; i < Scene.Objects.Size(); ++i)
		if (Scene.Objects[i].Freed && i != Scene.PlayerIndex && static_cast<i32>(i) != Scene.Player.ActiveSheep && static_cast<i32>(i) != Scene.UfoController.Ufo.Target)
		{
			slot = i;
			break;
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
	object.Draw = {Scene.Resources.ItemSprites[kind], static_cast<i32>(object.X), static_cast<i32>(object.Y), kind >= Kinds::Rocket ? kind - 14 : 0};
	cstr duration = kind == Kinds::Beer ? "BeerTime" : kind == Kinds::Teddy       ? "TeddyTime"
	                                               : kind == Kinds::Bone          ? "BoneTime"
	                                               : kind == Kinds::Artifact      ? "ArtifactTime"
	                                               : kind == Kinds::Rocket        ? "FireworkTime"
	                                               : kind == Kinds::Gift          ? "GiftTime"
	                                               : kind == Kinds::SlowMushroom  ? "ImpotentTime"
	                                               : kind == Kinds::StinkMushroom ? "SmellTime"
	                                                                              : "FlowerTime";
	object.Pulse = Scene.Parameter(duration, kind >= Kinds::Rocket ? 10.0f : 16.0f);
	if (slot == Scene.Objects.Size())
		Scene.Objects.Add(object);
	else
		Scene.Objects.Replace(slot, object);
	static constexpr u8 codes[12] = {130, 131, 129, 128, 0, 0, 0, 132, 133, 134, 135, 136};
	Scene.Grid.ItemCodes[tile] = codes[kind - 7];
	Scene.Grid.ItemObjects[tile] = static_cast<i32>(slot);
}

void LevelItems::SpawnItems()
{
	// 0x411400 makes one attempt per present marker group, not one per marker.
	static constexpr i32 choices[9] = {7, 8, 9, 10, 14, 15, 16, 17, 18};
	for (i32 group = 0; group < 3; ++group)
	{
		if (Scene.Grid.Spawners[group].empty())
			continue;
		i32 tile = Scene.Grid.Spawners[group][Scene.RandomInt(static_cast<i32>(Scene.Grid.Spawners[group].size()))];
		if (Scene.Grid.ItemCodes[tile])
			continue;
		const i32 occupant = Scene.Grid.TileAt(Scene.Player.X, Scene.Player.Y) == tile ? 100 : Scene.Grid.Tiles[tile];
		if (group != 1 && occupant >= 3 && occupant <= 6)
			continue;
		if (group != 0 && occupant == 100)
			continue;
		const i32 selection = group == 0 ? Scene.RandomInt(4) : group == 1 ? 4 + Scene.RandomInt(5)
		                                                                   : Scene.RandomInt(9);
		SpawnItem(choices[selection], tile);
	}
}

void LevelItems::UpdateItems(f32 seconds)
{
	EffectAge += seconds;
	if (PlayerEffect)
	{
		EffectRemaining -= seconds;
		if (EffectRemaining < 0)
		{
			PlayerEffect = 0;
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
	if (PlayerEffect != 135 && PlayerEffect != 136)
		return;
	const i32 tile = Scene.Grid.TileAt(Scene.Player.X, Scene.Player.Y);
	if (tile < 0)
		return;
	const i32 mood = PlayerEffect == 135 ? 10 : 0;
	bool changed = false;
	for (WorldObject& animal : Scene.Objects)
	{
		if (animal.Freed || animal.Kind != Kinds::Sheep || animal.Temperament == 2 || animal.Impatience == mood || animal.Tile < 0)
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
	const i32 code = Scene.Grid.ItemCodes[tile];
	if (code == 128)
	{
		if (PlayerEffect == 137 || Scene.UfoController.Ufo.State >= 0)
			return;
		RemoveItem(tile);
		Scene.UfoController.RequestUfo(static_cast<i32>(Scene.PlayerIndex));
		return;
	}
	if (code < 132 || code > 136 || (code != 133 && PlayerEffect == 137))
		return;
	RemoveItem(tile);
	if (code == 133)
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
	cstr duration = code == 132 ? "FireworkDuration" : code == 134 ? "ImpotentDuration"
	                                               : code == 135   ? "SmellDuration"
	                                                               : "FlowerDuration";
	EffectRemaining = Scene.Parameter(duration, code == 135 ? 15.0f : 10.0f);
	cstr sound = code == 132 ? "item_firework.ogg" : code == 134 ? "item_impotenz.ogg"
	                                             : code == 135   ? "item_stinky.ogg"
	                                                             : "item_rose.ogg";
	Scene.Sounds.push_back(str("Sven2/sfx/") + sound);
}

void LevelItems::UpdateObject(size_t i, f32 seconds)
{
	WorldObject& object = Scene.Objects[i];
	if (object.Freed)
		return;
	if (object.Kind == Kinds::Scenery && object.State == 1)
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
	if ((object.Kind >= Kinds::Beer && object.Kind <= Kinds::Artifact && !object.Permanent) || (object.Kind >= Kinds::Rocket && object.Kind <= Kinds::Flower))
	{
		object.Pulse -= seconds;
		if (object.Pulse < 0)
			RemoveItem(object.Tile);
	}
}
