#include "Scene/Characters/PlayerCharacter.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

PlayerCharacter::PlayerCharacter(LevelScene& scene, size_t number)
    : BaseCharacter(scene), Number(number), ActionArmed(scene.GetGameplayEdition() != GameEdition::XXX)
{}

void PlayerCharacter::SetInput(const PlayerInput& input)
{
	if (PlayerAction == PlayerActionState::Idle)
		ActionQueued = false;
	if (input.Action && ActionArmed && (PlayerAction == PlayerActionState::Idle || PlayerAction == PlayerActionState::Interaction || PlayerAction == PlayerActionState::Frigida))
		ActionQueued = true;
	// XXX 0x41947C starts unarmed; 0x41810A..0x41812B arms on release.
	ActionArmed = !input.Action;
	MovementX = std::clamp(input.X, -1, 1);
	MovementY = std::clamp(input.Y, -1, 1);
}

bool PlayerCharacter::CanRepeatInteraction(const WorldObject& animal) const
{
	if (Scene.GetGameplayEdition() != GameEdition::XXX || RequestedMotion == PlayerMotion::Stopped)
		return true;
	// XXX 0x417000 accepts three requested directions for each animal facing.
	return (static_cast<i32>(RequestedMotion) - 2 * animal.Facing + 8) % 8 <= 2;
}

void PlayerCharacter::Update(f32 seconds)
{
	LevelScene::PlayerScope player(Scene, Number);
	AdvancePlayer(seconds, MovementX, MovementY);
}

void PlayerCharacter::MarkPlayerCell()
{
	// 0x42C3F0 marks the committed base cell, not the interpolated center.
	// 0x42C470/0x42BF57 clear it only when the full 40-pixel step commits.
	if (X != TargetX || Y != TargetY)
		return;
	WorldObject& actor = Scene.Objects[Scene.GetPlayerIndex()];
	actor.X = X;
	actor.Y = Y;
	const i32 tile = Scene.Grid.TileAt(X, Y);
	if (actor.Tile != tile)
	{
		if (actor.Tile >= 0 && Scene.Grid.Tiles[actor.Tile] == OccupancyCode::Player && (Scene.GetGameplayEdition() != GameEdition::XXX || Scene.Grid.Reservations[actor.Tile] == static_cast<i32>(GetIndex())))
			Scene.Grid.Tiles[actor.Tile] = Scene.Grid.TileDirections[actor.Tile] ? OccupancyCode::Wall : OccupancyCode::Empty;
		if (Scene.GetGameplayEdition() == GameEdition::XXX)
			Scene.Grid.ClearPlayerCells(GetIndex());
		actor.Tile = tile;
	}
	if (tile >= 0 && (Scene.Grid.Tiles[tile] == OccupancyCode::Empty || Scene.Grid.Tiles[tile] == OccupancyCode::PlayerReservation || Scene.Grid.Tiles[tile] == OccupancyCode::Player))
	{
		if (Scene.GetGameplayEdition() == GameEdition::XXX && Scene.Grid.Reservations[tile] >= 0 && Scene.Grid.Reservations[tile] != static_cast<i32>(GetIndex()))
			return;
		Scene.Grid.Tiles[tile] = OccupancyCode::Player;
		if (Scene.GetGameplayEdition() == GameEdition::XXX)
			Scene.Grid.Reservations[tile] = static_cast<i32>(GetIndex());
	}
}

void PlayerCharacter::CancelPlayerStep()
{
	if (Scene.GetGameplayEdition() == GameEdition::XXX)
	{
		Scene.Grid.ClearPlayerCells(GetIndex());
		Motion = RequestedMotion = PlayerMotion::Stopped;
		StepFinished = true;
		OffsetX = OffsetY = 0;
	}
	// Original 0x42C816 clears interpolation offsets, preserving the base cell.
	const i32 destination = Scene.Grid.TileAt(TargetX, TargetY);
	if (Scene.GetGameplayEdition() != GameEdition::XXX && destination >= 0 && Scene.Grid.Tiles[destination] == OccupancyCode::PlayerReservation)
		Scene.Grid.Tiles[destination] = OccupancyCode::Empty;
	const WorldObject& getPlayerDraw = Scene.Objects[Scene.GetPlayerIndex()];
	X = TargetX = getPlayerDraw.X;
	Y = TargetY = getPlayerDraw.Y;
	MarkPlayerCell();
}

void PlayerCharacter::Teleport(i32 source)
{
	if (Scene.GetGameplayEdition() == GameEdition::XXX)
		CancelPlayerStep();
	const ::Sprite* sprite = Scene.Effects.Splashes[0].Sprite;
	auto splashAt = [sprite](i32 tile) {
		return SpriteDraw{ sprite, tile % Stride * TileSize - 70, tile / Stride * TileSize - 65, 0 };
	};
	Scene.Effects.Splashes[0] = splashAt(source);
	Scene.Effects.Splashes[1] = {};
	Scene.Effects.SplashAge = 0;
	Scene.Effects.SplashSound = true;
	Scene.Sounds.push_back("Sven2/sfx/water_splash22.ogg");
	Scene.GetPlayer().SplashAge = 0;
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
		if (cellX < 0 || cellY < 0 || cellX >= Columns || cellY >= Rows || !Scene.Objects.CanOccupy((cellY + 1) * Stride + cellX + 1, Scene.GetPlayerIndex()))
			continue;
		if (Scene.GetGameplayEdition() == GameEdition::XXX)
			Scene.Grid.ClearPlayerCells(GetIndex());
		X = static_cast<f32>(cellX * TileSize + TileSize / 2) - PlayerCenter;
		Y = static_cast<f32>(cellY * TileSize + TileSize / 2) - PlayerCenter;
		TargetX = X;
		TargetY = Y;
		Scene.Objects[Scene.GetPlayerIndex()].CommittedY = Y;
		if (Scene.GetGameplayEdition() == GameEdition::XXX)
			MarkPlayerCell();
		Scene.Effects.Splashes[1] = splashAt(target);
		Log(str(Scene.Grid.Tiles[source] == OccupancyCode::Water ? "Water entry: " : "Puddle teleport: ") + std::to_string(source) + " -> " + std::to_string(target));
		return;
	}
}

void PlayerCharacter::RestorePlayer()
{
	SpriteDraw& draw = Scene.Objects[Scene.GetPlayerIndex()].Draw;
	draw = { Scene.Resources.Stand, static_cast<i32>(X), static_cast<i32>(Y + PlayerLift), Facing };
	if (PlayerShockAge >= 0)
	{
		const std::vector<i32>& frames = Scene.Resources.PlayerFrames(PlayerAnimation::ElectricShock);
		draw = { Scene.Resources.Electric[Facing], static_cast<i32>(X), static_cast<i32>(Y), frames[std::min(frames.size() - 1, static_cast<size_t>(PlayerShockAge))], SvenMirror[Facing] };
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
		draw = { Scene.Resources.PlayerBeamSprites[Facing / 2], static_cast<i32>(X), static_cast<i32>(Y + PlayerLift), frame, Facing / 2 == 1 || Facing / 2 == 3 };
	}
}

void PlayerCharacter::Act(bool continuation)
{
	const i32 tile = Scene.Grid.TileAt(X + Directions[Facing][0] * 40, Y + Directions[Facing][1] * 40);
	for (size_t i = 0; i < Scene.Objects.Size(); ++i)
	{
		WorldObject& animal = Scene.Objects[i];
		if (!Scene.IsAnimal(animal.Kind) || animal.Freed || animal.Tile != tile || animal.State == ActorState::AnimalWalking || animal.State == ActorState::Turning || (IsDemonState(animal.State)))
			continue;
		ActiveSheep = static_cast<i32>(i);
		ActionAge = 0;
		// 0x42B730: each sheep direction accepts three of Sven's eight Directions.
		const i32 difference = (Facing - 2 * animal.Facing + 8) % 8;
		if (difference > 2)
		{
			static constexpr i32 turn[8] = { 0, 0, 0, 1, 1, 2, 2, 3 }; // 0x44478C
			animal.TurnTo = turn[Facing];
			animal.State = ActorState::Turning;
			animal.Age = 0;
			animal.Idle = 75;
			PlayerAction = PlayerActionState::Whistle;
			DrawActionFrame(0);
			Scene.Sounds.push_back("Sven2/sfx/Sven_whistle.ogg");
			return;
		}
		if (animal.Kind == ObjectKind::Sheep && animal.Impatience == 10 && animal.Temperament != 2)
		{
			PlayerAction = PlayerActionState::Kicked;
			DrawActionFrame(0);
			Scene.Effects.AddTime(-10, animal.X + 60, animal.Y + 60);
			Scene.Sounds.push_back("Sven2/sfx/Sheep_kickSven.ogg");
			return;
		}
		if (animal.Kind == ObjectKind::Sheep && animal.Temperament == 1)
		{
			animal.Impatience = 0;
			animal.Temperament = 2;
		}
		if (!continuation)
			++animal.Attempts;
		if (animal.Kind == ObjectKind::Sheep && animal.Temperament == 2)
		{
			PlayerAction = PlayerActionState::Frigida;
			Scene.Sounds.push_back("Sven2/sfx/Frigida_kettle.ogg");
			DrawActionFrame(0);
			return;
		}
		if (animal.Kind != ObjectKind::Sheep)
		{
			Interaction = dynamic_cast<AnimalCharacter&>(Scene.Objects.Get(i)).GetInteraction(animal.Facing);
			if (!Interaction || !Interaction->Sprite || Interaction->Frames.empty())
			{
				Interaction = nullptr;
				ActiveSheep = -1;
				return;
			}
			PlayerAction = PlayerActionState::Interaction;
			Scene.Sounds.push_back(Interaction->Sound);
			Scene.Objects[Scene.GetPlayerIndex()].Draw = { Interaction->Sprite, static_cast<i32>(animal.X), static_cast<i32>(animal.Y), Interaction->Frames.front(), Interaction->Mirror };
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
		PlayerAction = PlayerActionState::Interaction;
		Scene.Sounds.push_back(Interaction->Sound);
		Scene.Objects[Scene.GetPlayerIndex()].Draw = { Interaction->Sprite, static_cast<i32>(animal.X), static_cast<i32>(animal.Y), Interaction->Frames.front(), Interaction->Mirror };
		return;
	}
}

void PlayerCharacter::FinishInteraction()
{
	WorldObject& animal = Scene.Objects[ActiveSheep];
	Scene.AddScore(Scene.Items.GetPowerup().Code == ItemCode::Rocket ? 3 : (Scene.Items.GetPowerup().Code == ItemCode::SlowMushroom ? 0 : (Scene.Items.GetPowerup().Code == ItemCode::AlienPower ? 10 : 1))); // 0x429B7E
	animal.Impatience -= 2;
	if (animal.Impatience < 0)
		dynamic_cast<AnimalCharacter&>(Scene.Objects.Get(ActiveSheep)).Resolve();
	PlayerAction = PlayerActionState::Idle;
	ActiveSheep = -1;
	Interaction = nullptr;
	RestorePlayer();
	const bool repeat = ActionQueued && !animal.Freed && CanRepeatInteraction(animal);
	ActionQueued = false;
	if (repeat)
	{
		Act(true);
	}
}

const std::vector<i32>& PlayerCharacter::GetActionFrames() const
{
	switch (PlayerAction)
	{
	case PlayerActionState::Whistle:
		return Scene.Resources.PlayerFrames(Facing % 2 ? PlayerAnimation::WhistleDiagonal : PlayerAnimation::Whistle);
	case PlayerActionState::Kicked:
	{
		const i32 facing = Scene.Objects[ActiveSheep].Facing;
		return Scene.Resources.PlayerFrames(facing == 0 || facing == 3 ? PlayerAnimation::KickBack : PlayerAnimation::KickFront);
	}
	case PlayerActionState::Frigida:
		return Scene.Resources.PlayerFrames(PlayerAnimation::Frigida);
	default:
		throw std::logic_error("Player action has no standalone frame sequence.");
	}
}

f32 PlayerCharacter::GetActionRate() const
{
	if (PlayerAction != PlayerActionState::Frigida)
		return DefaultRate;
	if (Scene.GetGameplayEdition() != GameEdition::XXX)
		return FrigidaRate;
	// XXX 0x419CD3 selects the equipped-effect rate for each Frigida cycle.
	return Powerup.Code == ItemCode::Rocket ? Scene.Parameter("SvenShagFireworkFrameRate", 30) : (Powerup.Code == ItemCode::SlowMushroom ? Scene.Parameter("SvenShagImpotentFrameRate", 12) : (Powerup.Code == ItemCode::AlienPower ? Scene.Parameter("SvenShagAlienFrameRate", 40) : FrigidaRate));
}

void PlayerCharacter::DrawActionFrame(size_t frame)
{
	const i32 image = GetActionFrames().at(frame);
	SpriteDraw& draw = Scene.Objects[Scene.GetPlayerIndex()].Draw;
	if (PlayerAction == PlayerActionState::Whistle)
	{
		draw = { Scene.Resources.Whistles[Facing], static_cast<i32>(X), static_cast<i32>(Y), image, SvenMirror[Facing] };
		return;
	}
	const WorldObject& animal = Scene.Objects[ActiveSheep];
	draw = { PlayerAction == PlayerActionState::Frigida ? Scene.Resources.Frigida[animal.Facing] : Scene.Resources.Kicks[animal.Facing], static_cast<i32>(animal.X), static_cast<i32>(animal.Y), image, SheepMirror[animal.Facing] };
}

void PlayerCharacter::AdvancePlayer(f32 dt, i32 dx, i32 dy)
{
	if (Scene.GetGameplayEdition() == GameEdition::XXX)
		UpdateRequestedMotion();
	if (PlayerShockAge >= 0)
	{
		PlayerShockAge += dt * DefaultRate;
		if (PlayerShockAge >= Scene.Resources.PlayerFrames(PlayerAnimation::ElectricShock).size())
			PlayerShockAge = -1;
		RestorePlayer();
		return;
	}
	if (PlayerBeamState)
	{
		RestorePlayer();
		return;
	}
	if (Scene.GetCombat().Enemy >= 0)
		return;
	Scene.Items.PickupPlayerItem();
	if (PlayerAction != PlayerActionState::Idle)
	{
		ActionAge += dt;
		if (PlayerAction == PlayerActionState::Interaction && Interaction)
		{
			const f32 effectiveActionRate = Scene.Items.GetPowerup().Code == ItemCode::Rocket ? Scene.Parameter("SvenShagFireworkFrameRate", 30) : (Scene.Items.GetPowerup().Code == ItemCode::SlowMushroom ? Scene.Parameter("SvenShagImpotentFrameRate", 12) : (Scene.Items.GetPowerup().Code == ItemCode::AlienPower ? Scene.Parameter("SvenShagAlienFrameRate", 40) : ActionRate));
			i32 frame = static_cast<i32>(ActionAge * effectiveActionRate);
			if (frame >= static_cast<i32>(Interaction->Frames.size()))
				FinishInteraction();
			else
			{
				WorldObject& animal = Scene.Objects[ActiveSheep];
				Scene.Objects[Scene.GetPlayerIndex()].Draw = { Interaction->Sprite, static_cast<i32>(animal.X), static_cast<i32>(animal.Y), Interaction->Frames[frame], Interaction->Mirror };
			}
		}
		else if (ActionAge * GetActionRate() < GetActionFrames().size())
		{
			DrawActionFrame(static_cast<size_t>(ActionAge * GetActionRate()));
		}
		else
		{
			const bool repeat = ActionQueued && (Scene.GetGameplayEdition() != GameEdition::XXX || (PlayerAction == PlayerActionState::Frigida && CanRepeatInteraction(Scene.Objects[ActiveSheep])));
			if (PlayerAction == PlayerActionState::Kicked)
			{
				// Player state 13 at 0x42A283 releases the sheep at mood 6.
				Scene.Objects[ActiveSheep].Impatience = 6;
				ActionQueued = false;
			}
			if (PlayerAction == PlayerActionState::Frigida)
			{
				WorldObject& animal = Scene.Objects[ActiveSheep];
				++animal.Impatience;
				if (animal.Impatience == 6 || animal.Impatience == 10)
					Scene.Sounds.push_back("Sven2/sfx/Frigida.ogg");
				if (animal.Impatience == 11)
					dynamic_cast<AnimalCharacter&>(Scene.Objects.Get(ActiveSheep)).Resolve();
			}
			PlayerAction = PlayerActionState::Idle;
			ActiveSheep = -1;
			RestorePlayer();
			if (repeat && ActionQueued)
			{
				ActionQueued = false;
				Act(true);
			}
			ActionQueued = false;
		}
		return;
	}
	const std::vector<i32>& teleportFrames = Scene.Resources.PlayerFrames(PlayerAnimation::Teleport);
	if (SplashAge * DefaultRate < teleportFrames.size())
	{
		Scene.Objects[Scene.GetPlayerIndex()].Draw = { Scene.Resources.Blinks[Facing], static_cast<i32>(X), static_cast<i32>(Y), teleportFrames[static_cast<size_t>(SplashAge * DefaultRate)], SvenMirror[Facing] };
		return;
	} // 0x409A4C / 0x440158, independently of the splash effect lifetime.
	RestorePlayer();
	if (Scene.Completed())
	{
		ActionQueued = false;
		return;
	}
	if (Scene.GetGameplayEdition() == GameEdition::XXX)
		AdvanceMovement(dt);
	else
		AdvanceLegacyMovement(dt, dx, dy);
}

void PlayerCharacter::AdvanceLegacyMovement(f32 dt, i32 dx, i32 dy)
{
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
		if (PlayerAction != PlayerActionState::Idle)
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
		OccupancyCode kind = Scene.Grid.Tiles[destination];
		if (kind == OccupancyCode::Puddle || kind == OccupancyCode::Water)
		{
			Teleport(destination);
			RestorePlayer();
			return;
		}
		if (!Scene.Objects.CanOccupy(destination, Scene.GetPlayerIndex()) && !(kind == OccupancyCode::Wall && Scene.Grid.TileDirections[destination] == direction))
			return;
		if (kind == OccupancyCode::Empty)
			Scene.Grid.Tiles[destination] = OccupancyCode::PlayerReservation; // 0x42A94B: reserve the proposed destination.
		TargetX = nextX;
		TargetY = nextY;
	}
	const f32 vx = TargetX - X, vy = TargetY - Y;
	const f32 distance = std::max(std::abs(vx), std::abs(vy));
	const f32 currentSpeed = Scene.Items.GetPowerup().Code == ItemCode::Rocket || Scene.Items.GetPowerup().Code == ItemCode::AlienPower ? Scene.Parameter("SvenRunSpeed", 220) : (Scene.Items.GetPowerup().Code == ItemCode::SlowMushroom ? Scene.Parameter("SvenSlowSpeed", 95) : Speed);
	const f32 rate = currentSpeed * (vx != 0 && vy != 0 ? Diagonal : 1.0f);
	const f32 fraction = distance > 0 ? std::min(1.0f, dt * rate / distance) : 1;
	X += vx * fraction;
	Y += vy * fraction;
	if (X == TargetX && Y == TargetY)
	{
		MarkPlayerCell();
		Scene.Objects[Scene.GetPlayerIndex()].CommittedY = Y;
		Scene.Items.AffectNearbyAnimals(); // 0x42BF9E applies smell/flowers at tile arrival.
	}
	Scene.Items.PickupPlayerItem();
	AnimationAge += dt;
	SpriteDraw& draw = Scene.Objects[Scene.GetPlayerIndex()].Draw;
	const f32 walkRate = Scene.Items.GetPowerup().Code == ItemCode::Rocket ? Scene.Parameter("SvenWalkFireworkFrameRate", 36) : (Scene.Items.GetPowerup().Code == ItemCode::SlowMushroom ? Scene.Parameter("SvenWalkImpotentFrameRate", 16) : (Scene.Items.GetPowerup().Code == ItemCode::AlienPower ? Scene.Parameter("SvenWalkAlienFrameRate", 36) : WalkRate));
	const std::vector<i32>& walkFrames = Scene.Resources.PlayerFrames(PlayerAnimation::Walk);
	draw = { Scene.Resources.Walks[Facing], static_cast<i32>(X), static_cast<i32>(Y), walkFrames[static_cast<size_t>(AnimationAge * walkRate) % walkFrames.size()], SvenMirror[Facing] };
}

void PlayerCharacter::UpdateRequestedMotion()
{
	i32 requested = -1;
	for (i32 facing = 0; facing < 8; ++facing)
		if (Directions[facing][0] == MovementX && Directions[facing][1] == MovementY)
			requested = facing;
	if (requested < 0)
	{
		// XXX 0x4180BB: releasing the keys finishes the current step.
		if (StepFinished)
			RequestedMotion = PlayerMotion::Stopped;
		return;
	}
	const i32 tile = Scene.Grid.TileAt(X, Y);
	if (tile >= 0 && (!Scene.Grid.TileDirections[tile] || Scene.Grid.TileDirections[tile] == TileDirection[requested]))
		RequestedMotion = static_cast<PlayerMotion>(requested);
}

void PlayerCharacter::AdvanceMovement(f32 seconds)
{
	WorldObject& actor = Scene.Objects[GetIndex()];
	if (RequestedMotion != PlayerMotion::Stopped)
	{
		Facing = static_cast<i32>(RequestedMotion);
		SelectedInteraction = -1;
		const i32* requested = Directions[Facing];
		if (StepFinished)
		{
			const i32 destination = Scene.Grid.TileAt(X + requested[0] * TileSize, Y + requested[1] * TileSize);
			Motion = PlayerMotion::Stopped;
			if (destination >= 0)
			{
				const OccupancyCode terrain = Scene.Grid.Tiles[destination];
				if (terrain == OccupancyCode::Puddle || terrain == OccupancyCode::Water)
				{
					Teleport(destination);
					RestorePlayer();
					return;
				}
				const bool directionalWall = terrain == OccupancyCode::Wall && Scene.Grid.TileDirections[destination] == TileDirection[Facing];
				if ((terrain == OccupancyCode::Empty && Scene.Objects.CanOccupy(destination, GetIndex())) || (directionalWall && Scene.Grid.Reservations[destination] < 0))
				{
					Motion = RequestedMotion;
					StepFinished = false;
					OffsetX = OffsetY = 0;
					Scene.Grid.Tiles[destination] = OccupancyCode::PlayerReservation;
					Scene.Grid.Reservations[destination] = static_cast<i32>(GetIndex());
				}
			}
		}
		else
		{
			// The 0x4164C0 transition cases reduce to the opposite three-facing arc.
			// Other turns retain the current motion until its tile boundary.
			const i32 difference = (Facing - static_cast<i32>(Motion) + 8) % 8;
			if (difference >= 3 && difference <= 5)
				Motion = static_cast<PlayerMotion>((static_cast<i32>(Motion) + 4) % 8);
		}
	}
	if (StepFinished)
	{
		// XXX 0x41AB4A: attempt a new action only when movement did not start.
		AnimationAge = 0;
		RestorePlayer();
		if (ActionQueued)
		{
			ActionQueued = false;
			Act();
		}
		return;
	}
	// Idle-state input polling discards an unhandled moving-frame press on the
	// next native frame (0x4180D7). Do not replay it after internal substeps.
	ActionQueued = false;
	const i32 walkFacing = static_cast<i32>(Motion);
	const i32* direction = Directions[walkFacing];
	const bool returning = OffsetX * direction[0] < 0 || OffsetY * direction[1] < 0;
	TargetX = actor.X + (returning ? 0 : direction[0] * TileSize);
	TargetY = actor.Y + (returning ? 0 : direction[1] * TileSize);
	const f32 speed = Powerup.Code == ItemCode::Rocket || Powerup.Code == ItemCode::AlienPower ? Scene.Parameter("SvenRunSpeed", 220) : (Powerup.Code == ItemCode::SlowMushroom ? Scene.Parameter("SvenSlowSpeed", 95) : Speed);
	const f64 distance = static_cast<f64>(speed) * seconds * (direction[0] && direction[1] ? Diagonal : 1);
	auto advanceAxis = [distance](f32& offset, i32 sign) {
		if (!sign)
			return false;
		const f64 next = offset + sign * distance;
		const bool returning = offset * sign < 0;
		// 0x418140 x87 branches: strict outer boundary, inclusive return to zero.
		const bool finished = returning ? next * sign >= 0 : next * sign > TileSize;
		offset = static_cast<f32>(finished ? (returning ? 0 : sign * TileSize) : next);
		return finished;
	};
	const bool finishedX = advanceAxis(OffsetX, direction[0]);
	const bool finishedY = advanceAxis(OffsetY, direction[1]);
	X = actor.X + OffsetX;
	Y = actor.Y + OffsetY;
	StepFinished = direction[0] && direction[1] ? ((OffsetX == direction[0] * TileSize && OffsetY == direction[1] * TileSize) || (OffsetX == 0 && OffsetY == 0)) : finishedX || finishedY;
	if (StepFinished)
	{
		Scene.Grid.ClearPlayerCells(GetIndex());
		OffsetX = OffsetY = 0;
		TargetX = X;
		TargetY = Y;
		RequestedMotion = PlayerMotion::Stopped;
		MarkPlayerCell();
		actor.CommittedY = Y;
		Scene.Items.AffectNearbyAnimals();
	}
	Scene.Items.PickupPlayerItem();
	AnimationAge += seconds;
	const f32 walkRate = Powerup.Code == ItemCode::Rocket ? Scene.Parameter("SvenWalkFireworkFrameRate", 36) : (Powerup.Code == ItemCode::SlowMushroom ? Scene.Parameter("SvenWalkImpotentFrameRate", 16) : (Powerup.Code == ItemCode::AlienPower ? Scene.Parameter("SvenWalkAlienFrameRate", 36) : WalkRate));
	const std::vector<i32>& frames = Scene.Resources.PlayerFrames(PlayerAnimation::Walk);
	actor.Draw = { Scene.Resources.Walks[walkFacing], static_cast<i32>(X), static_cast<i32>(Y), frames[static_cast<size_t>(AnimationAge * walkRate) % frames.size()], SvenMirror[walkFacing] };
}
