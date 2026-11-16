#include "Scene/Characters/PlayerCharacter.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

PlayerCharacter::PlayerCharacter(LevelScene& scene)
    : BaseCharacter(scene)
{
}

void PlayerCharacter::MarkPlayerCell()
{
	// 0x42C3F0 marks the committed base cell, not the interpolated center.
	// 0x42C470/0x42BF57 clear it only when the full 40-pixel step commits.
	if (X != TargetX || Y != TargetY)
		return;
	WorldObject& actor = Scene.Objects[Scene.PlayerIndex];
	actor.X = X;
	actor.Y = Y;
	const i32 tile = Scene.Grid.TileAt(X, Y);
	if (actor.Tile != tile)
	{
		if (actor.Tile >= 0 && Scene.Grid.Tiles[actor.Tile] == 100)
			Scene.Grid.Tiles[actor.Tile] = Scene.Grid.TileDirections[actor.Tile] ? Wall : Empty;
		actor.Tile = tile;
	}
	if (tile >= 0 && (Scene.Grid.Tiles[tile] == Empty || Scene.Grid.Tiles[tile] == 99 || Scene.Grid.Tiles[tile] == 100))
		Scene.Grid.Tiles[tile] = 100;
}

void PlayerCharacter::CancelPlayerStep()
{
	// Original 0x42C816 clears interpolation offsets, preserving the base cell.
	const i32 destination = Scene.Grid.TileAt(TargetX, TargetY);
	if (destination >= 0 && Scene.Grid.Tiles[destination] == 99)
		Scene.Grid.Tiles[destination] = Empty;
	const WorldObject& getPlayerDraw = Scene.Objects[Scene.PlayerIndex];
	X = TargetX = getPlayerDraw.X;
	Y = TargetY = getPlayerDraw.Y;
	MarkPlayerCell();
}

void PlayerCharacter::Teleport(i32 source)
{
	const ::Sprite* sprite = Scene.Effects.Splashes[0].Sprite;
	auto splashAt = [sprite](i32 tile)
	{
		return SpriteDraw{sprite, tile % Stride * TileSize - 70, tile / Stride * TileSize - 65, 0};
	};
	Scene.Effects.Splashes[0] = splashAt(source);
	Scene.Effects.Splashes[1] = {};
	Scene.Effects.SplashAge = 0;
	Scene.Effects.SplashSound = true;
	Scene.Sounds.push_back("Sven2/sfx/water_splash22.ogg");
	Scene.Effects.SplashPlayerAge = 0;
	if (Scene.Grid.Puddles.empty())
		return;
	std::array<i32, 16> destinations{};
	size_t count = 0;
	for (i32 tile : Scene.Grid.Puddles)
		if (tile != source || Scene.Grid.Puddles.size() == 1)
			destinations[count++] = tile;
	i32 target = destinations[Scene.RandomInt(static_cast<i32>(count))];
	for (const i32* direction : Directions)
	{
		i32 cellX = target % Stride - 1 + direction[0], cellY = target / Stride - 1 + direction[1];
		if (cellX < 0 || cellY < 0 || cellX >= Columns || cellY >= Rows || !Scene.Objects.CanOccupy((cellY + 1) * Stride + cellX + 1, Scene.PlayerIndex))
			continue;
		X = static_cast<f32>(cellX * TileSize + TileSize / 2) - PlayerCenter;
		Y = static_cast<f32>(cellY * TileSize + TileSize / 2) - PlayerCenter;
		TargetX = X;
		TargetY = Y;
		Scene.Objects[Scene.PlayerIndex].CommittedY = Y;
		Scene.Effects.Splashes[1] = splashAt(target);
		Log(str(Scene.Grid.Tiles[source] == Water ? "Water entry: " : "Puddle teleport: ") +
		    std::to_string(source) + " -> " + std::to_string(target));
		return;
	}
}

void PlayerCharacter::RestorePlayer()
{
	SpriteDraw& draw = Scene.Objects[Scene.PlayerIndex].Draw;
	draw = {Scene.Resources.Stand, static_cast<i32>(X), static_cast<i32>(Y + PlayerLift), Facing};
	if (PlayerShockAge >= 0)
	{
		const std::vector<i32>& frames = ActorFrameTables.at(AnimationSequence::PlayerElectricShock);
		draw = {Scene.Resources.Electric[Facing], static_cast<i32>(X), static_cast<i32>(Y), frames[std::min(frames.size() - 1, static_cast<size_t>(PlayerShockAge))], SvenMirror[Facing]};
		return;
	}
	if (PlayerBeamState)
	{
		if (PlayerBeamState == 22 && PlayerBeamClock <= 2.5f)
		{
			draw = {};
			return;
		}
		const i32 frame = PlayerBeamState == 21 ? std::min(21, static_cast<i32>(PlayerBeamClock * 10)) : std::min(7, 3 + static_cast<i32>((PlayerBeamClock - 2.5f) * 10));
		draw = {Scene.Resources.PlayerBeamSprites[Facing / 2], static_cast<i32>(X), static_cast<i32>(Y + PlayerLift), frame, Facing / 2 == 1 || Facing / 2 == 3};
	}
}

void PlayerCharacter::Act(bool continuation)
{
	const i32 tile = Scene.Grid.TileAt(X + Directions[Facing][0] * 40, Y + Directions[Facing][1] * 40);
	for (size_t i = 0; i < Scene.Objects.Size(); ++i)
	{
		WorldObject& animal = Scene.Objects[i];
		if (!Scene.IsAnimal(animal.Kind) || animal.Freed || animal.Tile != tile || animal.State == 4 || animal.State == 3 || (animal.State >= 11 && animal.State <= 14))
			continue;
		ActiveSheep = static_cast<i32>(i);
		ActionAge = 0;
		// 0x42B730: each sheep direction accepts three of Sven's eight Directions.
		const i32 difference = (Facing - 2 * animal.Facing + 8) % 8;
		if (difference > 2)
		{
			static constexpr i32 turn[8] = {0, 0, 0, 1, 1, 2, 2, 3}; // 0x44478C
			animal.TurnTo = turn[Facing];
			animal.State = 3;
			animal.Age = 0;
			animal.Idle = 75;
			PlayerAction = PlayerActions::Whistle;
			Scene.Sounds.push_back("Sven2/sfx/Sven_whistle.ogg");
			return;
		}
		if (animal.Kind == Kinds::Sheep && animal.Impatience == 10 && animal.Temperament != 2)
		{
			PlayerAction = PlayerActions::Kicked;
			Scene.Effects.AddTime(-10, animal.X + 60, animal.Y + 60);
			Scene.Sounds.push_back("Sven2/sfx/Sheep_kickSven.ogg");
			return;
		}
		if (animal.Kind == Kinds::Sheep && animal.Temperament == 1)
		{
			animal.Impatience = 0;
			animal.Temperament = 2;
		}
		if (!continuation)
			++animal.Attempts;
		if (animal.Kind == Kinds::Sheep && animal.Temperament == 2)
		{
			PlayerAction = PlayerActions::Frigida;
			Scene.Sounds.push_back("Sven2/sfx/Frigida_kettle.ogg");
			Scene.Objects[Scene.PlayerIndex].Draw = {Scene.Resources.Frigida[animal.Facing], static_cast<i32>(animal.X), static_cast<i32>(animal.Y), FrigidaFrames[0], SheepMirror[animal.Facing]};
			return;
		}
		if (animal.Kind != Kinds::Sheep)
		{
			Interaction = dynamic_cast<AnimalCharacter&>(Scene.Objects.Get(i)).GetInteraction(animal.Facing);
			if (!Interaction || !Interaction->Sprite || Interaction->Frames.empty())
			{
				Interaction = nullptr;
				ActiveSheep = -1;
				return;
			}
			PlayerAction = PlayerActions::Interaction;
			Scene.Sounds.push_back(Interaction->Sound);
			Scene.Objects[Scene.PlayerIndex].Draw = {Interaction->Sprite, static_cast<i32>(animal.X), static_cast<i32>(animal.Y), Interaction->Frames.front(), Interaction->Mirror};
			return;
		}
		std::vector<size_t> choices;
		for (size_t j = 0; j < Scene.Resources.Interactions.size(); ++j)
			if (Scene.Resources.Interactions[j].Facing == animal.Facing)
				choices.push_back(j);
		if (choices.empty())
		{
			ActiveSheep = -1;
			return;
		}
		// 0x42B09C..0x42B0EE chooses only after movement invalidates the variant.
		if (SelectedInteraction < 0 || Scene.Resources.Interactions[SelectedInteraction].Facing != animal.Facing)
			SelectedInteraction = static_cast<i32>(choices[Scene.RandomInt(static_cast<i32>(choices.size()))]);
		Interaction = &Scene.Resources.Interactions[SelectedInteraction];
		PlayerAction = PlayerActions::Interaction;
		Scene.Sounds.push_back(Interaction->Sound);
		Scene.Objects[Scene.PlayerIndex].Draw = {Interaction->Sprite, static_cast<i32>(animal.X), static_cast<i32>(animal.Y), Interaction->Frames.front(), Interaction->Mirror};
		return;
	}
}

void PlayerCharacter::FinishInteraction()
{
	WorldObject& animal = Scene.Objects[ActiveSheep];
	Scene.AddScore(Scene.Items.PlayerEffect == 132 ? 3 : Scene.Items.PlayerEffect == 134 ? 0
	                                                 : Scene.Items.PlayerEffect == 137   ? 10
	                                                                                     : 1); // 0x429B7E
	animal.Impatience -= 2;
	if (animal.Impatience < 0)
		dynamic_cast<AnimalCharacter&>(Scene.Objects.Get(ActiveSheep)).Resolve();
	PlayerAction = PlayerActions::Idle;
	ActiveSheep = -1;
	Interaction = nullptr;
	RestorePlayer();
	if (ActionQueued && !animal.Freed)
	{
		ActionQueued = false;
		Act(true);
	}
}

void PlayerCharacter::AdvancePlayer(f32 dt, i32 dx, i32 dy)
{
	if (PlayerShockAge >= 0)
	{
		PlayerShockAge += dt * DefaultRate;
		if (PlayerShockAge >= ActorFrameTables.at(AnimationSequence::PlayerElectricShock).size())
			PlayerShockAge = -1;
		RestorePlayer();
		return;
	}
	if (PlayerBeamState)
	{
		RestorePlayer();
		return;
	}
	if (Scene.Objects.Combat.Enemy >= 0)
		return;
	Scene.Items.PickupPlayerItem();
	if (PlayerAction)
	{
		ActionAge += dt;
		if (PlayerAction == PlayerActions::Interaction && Interaction)
		{
			const f32 effectiveActionRate = Scene.Items.PlayerEffect == 132 ? Scene.Parameter("SvenShagFireworkFrameRate", 30) : Scene.Items.PlayerEffect == 134 ? Scene.Parameter("SvenShagImpotentFrameRate", 12)
			                                                                                                                 : Scene.Items.PlayerEffect == 137   ? Scene.Parameter("SvenShagAlienFrameRate", 40)
			                                                                                                                                                     : ActionRate;
			i32 frame = static_cast<i32>(ActionAge * effectiveActionRate);
			if (frame >= static_cast<i32>(Interaction->Frames.size()))
				FinishInteraction();
			else
			{
				WorldObject& animal = Scene.Objects[ActiveSheep];
				Scene.Objects[Scene.PlayerIndex].Draw = {Interaction->Sprite, static_cast<i32>(animal.X), static_cast<i32>(animal.Y), Interaction->Frames[frame], Interaction->Mirror};
			}
		}
		else if (PlayerAction == PlayerActions::Kicked && ActionAge * DefaultRate < Scene.Resources.Kicks[Scene.Objects[ActiveSheep].Facing]->Rows)
		{
			const WorldObject& animal = Scene.Objects[ActiveSheep];
			Scene.Objects[Scene.PlayerIndex].Draw = {Scene.Resources.Kicks[animal.Facing], static_cast<i32>(animal.X), static_cast<i32>(animal.Y), static_cast<i32>(ActionAge * DefaultRate), SheepMirror[animal.Facing]};
		}
		else if (PlayerAction == PlayerActions::Frigida && ActionAge * FrigidaRate < 7)
		{
			const WorldObject& animal = Scene.Objects[ActiveSheep];
			Scene.Objects[Scene.PlayerIndex].Draw = {Scene.Resources.Frigida[animal.Facing], static_cast<i32>(animal.X), static_cast<i32>(animal.Y), FrigidaFrames[static_cast<i32>(ActionAge * FrigidaRate)], SheepMirror[animal.Facing]};
		}
		else if (PlayerAction == PlayerActions::Whistle && ActionAge * DefaultRate < 22)
		{
			i32 frame = std::min(21, static_cast<i32>(ActionAge * DefaultRate));
			Scene.Objects[Scene.PlayerIndex].Draw = {Scene.Resources.Whistles[Facing], static_cast<i32>(X), static_cast<i32>(Y), Facing % 2 ? WhistleDiagonal[frame] : WhistleStraight[frame], SvenMirror[Facing]};
		}
		else
		{
			if (PlayerAction == PlayerActions::Kicked)
			{
				// Player state 13 at 0x42A283 releases the sheep at mood 6.
				Scene.Objects[ActiveSheep].Impatience = 6;
				ActionQueued = false;
			}
			if (PlayerAction == PlayerActions::Frigida)
			{
				WorldObject& animal = Scene.Objects[ActiveSheep];
				++animal.Impatience;
				if (animal.Impatience == 6 || animal.Impatience == 10)
					Scene.Sounds.push_back("Sven2/sfx/Frigida.ogg");
				if (animal.Impatience == 11)
					dynamic_cast<AnimalCharacter&>(Scene.Objects.Get(ActiveSheep)).Resolve();
			}
			PlayerAction = PlayerActions::Idle;
			ActiveSheep = -1;
			RestorePlayer();
			if (ActionQueued)
			{
				ActionQueued = false;
				Act(true);
			}
		}
		return;
	}
	if (Scene.Effects.SplashPlayerAge * DefaultRate < 16)
	{
		Scene.Objects[Scene.PlayerIndex].Draw = {Scene.Resources.Blinks[Facing], static_cast<i32>(X), static_cast<i32>(Y), SplashPlayerFrames[static_cast<i32>(Scene.Effects.SplashPlayerAge * DefaultRate)], SvenMirror[Facing]};
		return;
	} // 0x409A4C / 0x440158, independently of the splash effect lifetime.
	RestorePlayer();
	if (Scene.Completed())
	{
		ActionQueued = false;
		return;
	}
	const bool centered = X == TargetX && Y == TargetY;
	i32 requestedFacing = 0;
	while (requestedFacing < 8 && (dx != Directions[requestedFacing][0] || dy != Directions[requestedFacing][1]))
		++requestedFacing;
	if (centered && requestedFacing < 8)
	{
		Facing = requestedFacing;
		SelectedInteraction = -1;
	}
	if (centered && ActionQueued)
	{
		ActionQueued = false;
		Act();
		if (PlayerAction)
			return;
	}
	if (centered)
	{
		if (requestedFacing == 8)
		{
			AnimationAge = 0;
			RestorePlayer();
			return;
		}
		const i32 current = Scene.Grid.TileAt(X, Y);
		if (current < 0)
			return;
		u8 direction = TileDirection[Facing];
		u8 sourceDirection = Scene.Grid.TileDirections[current];
		if (sourceDirection && sourceDirection != direction)
			return;
		const f32 nextX = X + dx * TileSize, nextY = Y + dy * TileSize;
		i32 destination = Scene.Grid.TileAt(nextX, nextY);
		if (destination < 0)
			return;
		i32 kind = Scene.Grid.Tiles[destination];
		if (kind == Puddle || kind == Water)
		{
			Teleport(destination);
			RestorePlayer();
			return;
		}
		if (!Scene.Objects.CanOccupy(destination, Scene.PlayerIndex) && !(kind == Wall && Scene.Grid.TileDirections[destination] == direction))
			return;
		if (kind == Empty)
			Scene.Grid.Tiles[destination] = 99; // 0x42A94B: reserve the proposed destination.
		TargetX = nextX;
		TargetY = nextY;
	}
	const f32 vx = TargetX - X, vy = TargetY - Y;
	const f32 distance = std::max(std::abs(vx), std::abs(vy));
	const f32 currentSpeed = Scene.Items.PlayerEffect == 132 || Scene.Items.PlayerEffect == 137 ? Scene.Parameter("SvenRunSpeed", 220) : Scene.Items.PlayerEffect == 134 ? Scene.Parameter("SvenSlowSpeed", 95)
	                                                                                                                                                                     : Speed;
	const f32 rate = currentSpeed * (vx != 0 && vy != 0 ? Diagonal : 1.0f);
	const f32 fraction = distance > 0 ? std::min(1.0f, dt * rate / distance) : 1;
	X += vx * fraction;
	Y += vy * fraction;
	if (X == TargetX && Y == TargetY)
	{
		MarkPlayerCell();
		Scene.Objects[Scene.PlayerIndex].CommittedY = Y;
		Scene.Items.AffectNearbySheep(); // 0x42BF9E applies smell/flowers at tile arrival.
	}
	Scene.Items.PickupPlayerItem();
	AnimationAge += dt;
	SpriteDraw& draw = Scene.Objects[Scene.PlayerIndex].Draw;
	draw = {Scene.Resources.Walks[Facing], static_cast<i32>(X), static_cast<i32>(Y), static_cast<i32>(AnimationAge * (Scene.Items.PlayerEffect == 132 ? Scene.Parameter("SvenWalkFireworkFrameRate", 36) : Scene.Items.PlayerEffect == 134 ? Scene.Parameter("SvenWalkImpotentFrameRate", 16)
	                                                                                                                                                                                                   : Scene.Items.PlayerEffect == 137   ? Scene.Parameter("SvenWalkAlienFrameRate", 36)
	                                                                                                                                                                                                                                       : WalkRate)) %
	                                                                                     14,
	        SvenMirror[Facing]};
}
