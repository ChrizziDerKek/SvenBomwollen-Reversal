#include "Scene/Characters/PlayerCharacter.h"
#include "Scene/Objects/ObjectCatalog.h"
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
	if (Scene.GetGameplayEdition() == GameEdition::XXX && !Scene.IsBonusLevel())
	{
		IconAge += seconds * 20;
		if (IconAge >= Scene.Resources.FrameList(Number == 0 ? "aPlayerIcon1" : "aPlayerIcon2").size())
			IconAge = 0;
	}
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
	std::array<SpriteDraw, 2>& splashes = Scene.Effects.Splashes[Number];
	const ::Sprite* sprite = splashes[0].Sprite;
	auto splashAt = [sprite](i32 tile) {
		return SpriteDraw{ sprite, tile % Stride * TileSize - 70, tile / Stride * TileSize - 65, 0 };
	};
	splashes[0] = splashAt(source);
	splashes[1] = {};
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
		splashes[1] = splashAt(target);
		Log(str(Scene.Grid.Tiles[source] == OccupancyCode::Water ? "Water entry: " : "Puddle teleport: ") + std::to_string(source) + " -> " + std::to_string(target));
		return;
	}
}

void PlayerCharacter::RestorePlayer()
{
	if (Scene.IsBonusLevel())
	{
		DrawBonusPlayer();
		return;
	}
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
		i32 frame;
		if (Scene.GetGameplayEdition() == GameEdition::XXX)
		{
			// 0x40AA10 states 21/22: up clamps to the final up frame; down's
			// one-frame descriptor completes immediately and restores that frame.
			const std::vector<i32>& up = Scene.Resources.PlayerFrames(PlayerAnimation::BeamUp);
			const std::vector<i32>& down = Scene.Resources.PlayerFrames(PlayerAnimation::BeamDown);
			const size_t index = static_cast<size_t>(PlayerBeamClock * 10);
			frame = PlayerBeamState == 21 ? up[std::min(up.size() - 1, index)] : (index >= down.size() ? up.back() : down[index]);
		}
		else
			frame = PlayerBeamState == 21 ? std::min(21, static_cast<i32>(PlayerBeamClock * 10)) : std::min(7, 3 + static_cast<i32>((PlayerBeamClock - 2.5f) * 10));
		draw = { Scene.Resources.PlayerBeamSprites[Facing / 2], static_cast<i32>(X), static_cast<i32>(Y + PlayerLift), frame, Facing / 2 == 1 || Facing / 2 == 3 };
	}
}

void PlayerCharacter::Act(bool continuation)
{
	if (Scene.IsBonusLevel())
	{
		ActBonus(continuation);
		return;
	}
	const i32 tile = Scene.Grid.TileAt(X + Directions[Facing][0] * 40, Y + Directions[Facing][1] * 40);
	for (size_t i = 0; i < Scene.Objects.Size(); ++i)
	{
		WorldObject& animal = Scene.Objects[i];
		if (!Scene.IsAnimal(animal.Kind) || animal.Freed || (animal.InteractionPlayer >= 0 && animal.InteractionPlayer != static_cast<i32>(Number)) || animal.Tile != tile || animal.State == ActorState::AnimalWalking || animal.State == ActorState::Turning || (IsDemonState(animal.State)))
			continue;
		ActiveSheep = static_cast<i32>(i);
		ActionAge = 0;
		ActionComplete = false;
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
			CurrentActionRate = DefaultRate;
			DrawActionFrame(0);
			Scene.Sounds.push_back("Sven2/sfx/Sven_whistle.ogg");
			return;
		}
		if (animal.Kind == ObjectKind::Sheep && animal.Impatience == 10 && animal.Temperament != 2)
		{
			PlayerAction = PlayerActionState::Kicked;
			CurrentActionRate = DefaultRate;
			animal.InteractionPlayer = static_cast<i32>(Number);
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
			CurrentActionRate = GetActionRate();
			animal.InteractionPlayer = static_cast<i32>(Number);
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
			CurrentActionRate = GetInteractionRate();
			animal.InteractionPlayer = static_cast<i32>(Number);
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
		CurrentActionRate = GetInteractionRate();
		animal.InteractionPlayer = static_cast<i32>(Number);
		Scene.Sounds.push_back(Interaction->Sound);
		Scene.Objects[Scene.GetPlayerIndex()].Draw = { Interaction->Sprite, static_cast<i32>(animal.X), static_cast<i32>(animal.Y), Interaction->Frames.front(), Interaction->Mirror };
		return;
	}
}

void PlayerCharacter::FinishInteraction()
{
	WorldObject& animal = Scene.Objects[ActiveSheep];
	if (Scene.IsBonusLevel())
	{
		Scene.AddScore(2 * Scene.Items.GetInteractionMultiplier());
		animal.Impatience += 2;
		if (animal.Impatience > 11)
			dynamic_cast<AnimalCharacter&>(Scene.Objects.Get(ActiveSheep)).Resolve();
		const bool repeat = ActionQueued && !animal.Freed && CanRepeatInteraction(animal);
		ReleaseInteraction();
		PlayerAction = PlayerActionState::Idle;
		Interaction = nullptr;
		ActionQueued = false;
		RestorePlayer();
		if (repeat)
			Act(true);
		return;
	}
	if (Scene.GetGameplayEdition() == GameEdition::XXX)
		Scene.AddScore(Powerup.Code == ItemCode::Rocket ? 4 : (Powerup.Code == ItemCode::SlowMushroom ? 0 : (Powerup.Code == ItemCode::AlienPower ? 8 : (2 * Scene.Items.GetInteractionMultiplier())))); // XXX 0x417AC0
	else
		Scene.AddScore(Powerup.Code == ItemCode::Rocket ? 3 : (Powerup.Code == ItemCode::SlowMushroom ? 0 : (Powerup.Code == ItemCode::AlienPower ? 10 : 1)));
	animal.Impatience -= 2 * Scene.Items.GetInteractionMultiplier();
	if (animal.Impatience < 0)
	{
		if (Scene.GetGameplayEdition() == GameEdition::XXX)
			animal.Impatience = 0;
		dynamic_cast<AnimalCharacter&>(Scene.Objects.Get(ActiveSheep)).Resolve();
	}
	ReleaseInteraction();
	PlayerAction = PlayerActionState::Idle;
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
	if (Scene.IsBonusLevel())
	{
		AdvanceBonusPlayer(dt);
		return;
	}
	if (Scene.GetGameplayEdition() == GameEdition::XXX)
		UpdateRequestedMotion();
	if (ActionComplete)
	{
		ActionComplete = false;
		if (PlayerAction == PlayerActionState::Interaction)
			FinishInteraction();
		else
			FinishAction();
		if (PlayerAction == PlayerActionState::Idle)
			return;
	}
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
		AdvanceActionAnimation(dt);
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
			if (PlayerAction != PlayerActionState::Idle)
				AdvanceActionAnimation(seconds);
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

void PlayerCharacter::ReleaseInteraction()
{
	if (ActiveSheep >= 0 && static_cast<size_t>(ActiveSheep) < Scene.Objects.Size())
	{
		WorldObject& animal = Scene.Objects[ActiveSheep];
		if (animal.InteractionPlayer == static_cast<i32>(Number))
			animal.InteractionPlayer = -1;
	}
	ActiveSheep = -1;
}

void PlayerCharacter::InterruptInteraction()
{
	// 0x417A48 maps native whistle states 6/7 to the turn-abort branch.
	if (Scene.GetGameplayEdition() == GameEdition::XXX && PlayerAction == PlayerActionState::Whistle && ActiveSheep >= 0)
	{
		WorldObject& animal = Scene.Objects[ActiveSheep];
		if (!animal.Freed && animal.State == ActorState::Turning && animal.InteractionPlayer < 0)
		{
			animal.State = ActorState::Idle;
			animal.TurnTo = -1;
			animal.Idle = InterruptedTurnIdle;
			animal.Age = 0;
		}
	}
	ReleaseInteraction();
	PlayerAction = PlayerActionState::Idle;
	Interaction = nullptr;
	ActionComplete = false;
	ActionQueued = false;
	SelectedInteraction = -1;
	ActionAge = 0;
}

f32 PlayerCharacter::GetInteractionRate() const
{
	return Powerup.Code == ItemCode::Rocket ? Scene.Parameter("SvenShagFireworkFrameRate", 30) : (Powerup.Code == ItemCode::SlowMushroom ? Scene.Parameter("SvenShagImpotentFrameRate", 12) : (Powerup.Code == ItemCode::AlienPower ? Scene.Parameter("SvenShagAlienFrameRate", 40) : ActionRate));
}

void PlayerCharacter::FinishAction()
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
		animal.Impatience += Scene.Items.GetInteractionMultiplier();
		if (animal.Impatience == 6 || animal.Impatience == 10)
			Scene.Sounds.push_back("Sven2/sfx/Frigida.ogg");
		if (animal.Impatience >= 11)
		{
			if (Scene.GetGameplayEdition() == GameEdition::XXX)
				animal.Impatience = 0;
			dynamic_cast<AnimalCharacter&>(Scene.Objects.Get(ActiveSheep)).Resolve();
		}
	}
	ReleaseInteraction();
	PlayerAction = PlayerActionState::Idle;
	RestorePlayer();
	if (repeat && ActionQueued)
	{
		ActionQueued = false;
		Act(true);
	}
	ActionQueued = false;
}

void PlayerCharacter::ActBonus(bool continuation)
{
	if (Scene.GetPlayerKind() == ObjectKind::Dog)
		return;
	const i32 tile = Scene.Grid.TileAt(X + Directions[Facing][0] * TileSize, Y + Directions[Facing][1] * TileSize);
	for (size_t index : Scene.Objects.Find(ObjectKind::Sheep))
	{
		WorldObject& animal = Scene.Objects[index];
		if (animal.Tile != tile || animal.InteractionPlayer >= 0 || animal.State == ActorState::AnimalWalking || animal.State == ActorState::Turning || IsDemonState(animal.State))
			continue;
		if ((Facing - 2 * animal.Facing + 8) % 8 > 2)
		{
			static constexpr i32 turn[8] = { 0, 0, 0, 1, 1, 2, 2, 3 };
			animal.TurnTo = turn[Facing];
			animal.State = ActorState::Turning;
			animal.Age = 0;
			Scene.EmitNamedSound(Scene.GetPlayerKind() == ObjectKind::Farmer ? "SfxLarsTurnSheep" : (Scene.GetPlayerKind() == ObjectKind::Girl ? "SfxBroemseTurnSheep" : "SfxAlienTurnSheep"));
			return;
		}
		if (!continuation)
			++animal.Attempts;
		animal.Temperament = 0;
		animal.InteractionPlayer = static_cast<i32>(Number);
		ActiveSheep = static_cast<i32>(index);
		PlayerAction = PlayerActionState::Interaction;
		Interaction = &Scene.Resources.BonusActions[animal.Facing];
		CurrentActionRate = ActionRate;
		ActionAge = 0;
		ActionComplete = false;
		Scene.Sounds.push_back(Interaction->Sound);
		DrawInteractionFrame(Interaction->Frames.front());
		return;
	}
}

void PlayerCharacter::DrawInteractionFrame(i32 frame)
{
	const WorldObject& animal = Scene.Objects[ActiveSheep];
	i32 x = static_cast<i32>(animal.X), y = static_cast<i32>(animal.Y);
	if (Scene.GetBonusMode() == BonusMode::Farmer)
	{
		static constexpr i32 offsetX[4] = { -60, -20, -20, 20 }, offsetY[4] = { -60, -75, -75, -60 };
		x += offsetX[animal.Facing];
		y += offsetY[animal.Facing];
	}
	Scene.Objects[GetIndex()].Draw = { Interaction->Sprite, x, y, frame, Interaction->Mirror };
}

void PlayerCharacter::DrawBonusPlayer()
{
	SpriteDraw& draw = Scene.Objects[GetIndex()].Draw;
	if (PlayerAction == PlayerActionState::Interaction && Interaction)
	{
		const size_t frame = ActionComplete ? 0 : std::min(Interaction->Frames.size() - 1, static_cast<size_t>(ActionAge * CurrentActionRate));
		DrawInteractionFrame(Interaction->Frames[frame]);
		return;
	}
	const i32 offsetX = Scene.GetBonusMode() == BonusMode::Farmer ? -20 : 0;
	const i32 offsetY = Scene.GetBonusMode() == BonusMode::Farmer ? -60 : (Scene.GetBonusMode() == BonusMode::Girl ? -20 : 0);
	if (PlayerAction == PlayerActionState::Distraction || PlayerAction == PlayerActionState::BonusExit)
	{
		const size_t frame = static_cast<size_t>(ActionAge * CurrentActionRate);
		if (PlayerAction == PlayerActionState::BonusExit && frame >= BonusAnimation.Frames.size())
			draw = {};
		else
			draw = { BonusAnimation.Sprite, static_cast<i32>(X) + offsetX, static_cast<i32>(Y) + offsetY, BonusAnimation.Frames[std::min(frame, BonusAnimation.Frames.size() - 1)], Scene.GetBonusMode() == BonusMode::Girl ? Facing / 2 < 2 : Facing / 2 >= 2 };
		return;
	}
	if (StepFinished)
		draw = { Scene.Resources.Stand, static_cast<i32>(X) + offsetX, static_cast<i32>(Y) + offsetY, Facing };
	else
	{
		const i32 facing = static_cast<i32>(Motion);
		const Sprite* sprite = Scene.Resources.Walks[facing];
		const str name = Scene.GetBonusMode() == BonusMode::Farmer ? "aLarsWalk" : (Scene.GetBonusMode() == BonusMode::Girl ? "aBroemseWalk" : (Scene.GetBonusMode() == BonusMode::Alien ? "aAlienWalk" : "aWotanWalk"));
		const std::vector<i32>& frames = Scene.Resources.FrameList(name);
		const i32 frame = frames[static_cast<size_t>(AnimationAge * WalkRate) % frames.size()];
		draw = { sprite, static_cast<i32>(X) + offsetX, static_cast<i32>(Y) + offsetY, std::min(frame, sprite->Rows - 1), EnemyWalkMirror[EnemyTableIndex(Scene.GetPlayerKind())][facing] };
	}
}

void PlayerCharacter::AdvanceBonusPlayer(f32 seconds)
{
	if (PlayerAction == PlayerActionState::Idle)
		UpdateRequestedMotion();
	if (ActionComplete)
	{
		ActionComplete = false;
		if (PlayerAction == PlayerActionState::Interaction)
			FinishInteraction();
		else
		{
			PlayerAction = PlayerActionState::Idle;
			RestorePlayer();
		}
		if (PlayerAction == PlayerActionState::Idle)
			return;
	}
	if (PlayerAction == PlayerActionState::Interaction)
		AdvanceActionAnimation(seconds);
	else if (PlayerAction == PlayerActionState::Distraction || PlayerAction == PlayerActionState::BonusExit)
	{
		ActionAge += seconds;
		ActionComplete = PlayerAction == PlayerActionState::Distraction && ActionAge * CurrentActionRate >= BonusAnimation.Frames.size();
		DrawBonusPlayer();
	}
	else if (!Scene.Completed())
	{
		if (SplashAge * DefaultRate < Scene.Resources.PlayerFrames(PlayerAnimation::Teleport).size())
		{
			RestorePlayer();
			return;
		}
		Scene.Items.PickupPlayerItem();
		if (PlayerAction == PlayerActionState::Idle)
			AdvanceMovement(seconds);
		DrawBonusPlayer();
	}
}

void PlayerCharacter::PickupBonusItem(ItemCode code, i32 tile)
{
	if (PlayerAction != PlayerActionState::Idle || !StepFinished)
		return;
	const BonusMode mode = Scene.GetBonusMode();
	const bool artifact = code == ItemCode::Artifact;
	if (!artifact && !(mode == BonusMode::Farmer && code == ItemCode::Beer) && !(mode == BonusMode::Girl && code == ItemCode::Teddy))
		return;
	Scene.Items.RemoveItem(tile);
	CancelPlayerStep();
	ActionAge = 0;
	ActionComplete = false;
	PlayerAction = artifact ? PlayerActionState::BonusExit : PlayerActionState::Distraction;
	const str side = Facing / 2 == 0 || Facing / 2 == 3 ? "Back" : "Front";
	const ObjectKind kind = Scene.GetPlayerKind();
	BonusAnimation.Sprite = Scene.Resources.ActorSprites[kind].Beam[Facing / 2];
	BonusAnimation.Frames = Scene.Resources.FrameList(mode == BonusMode::Farmer ? "aLarsBeamUp" : (mode == BonusMode::Girl ? "aBroemseBeamUp" : "aAlienBeamingBonus"));
	CurrentActionRate = 10;
	if (artifact)
	{
		Scene.Effects.SpawnBeam(X + PlayerCenter, Y + PlayerCenter);
		Scene.EmitNamedSound("SfxBeam");
		Scene.EmitNamedSound(mode == BonusMode::Farmer ? "SfxLarsFoundSven" : (mode == BonusMode::Girl ? "SfxBroemseFoundSven" : "SfxAlienFoundSven"));
		Scene.FinishBonus();
	}
	else
	{
		BonusAnimation.Sprite = Scene.Resources.ActorSprites[kind].Distraction[Facing / 2];
		BonusAnimation.Frames = Scene.Resources.FrameList(mode == BonusMode::Farmer ? "aGuyDrinkBonus" : "aBroemseTeddy" + side + "Bonus");
		CurrentActionRate = mode == BonusMode::Farmer ? 10.0f : 9.0f;
		Scene.Effects.AddTime(-5, X + PlayerCenter, Y + PlayerCenter);
		Scene.EmitNamedSound(mode == BonusMode::Farmer ? "SfxGuyFoundBeer" : "SfxBroemseFoundTeddy");
		Scene.EmitNamedSound(mode == BonusMode::Farmer ? "SfxLarsDrinkAI" : "SfxBroemsePlaysTeddyAI");
	}
	DrawBonusPlayer();
}

void PlayerCharacter::AdvanceActionAnimation(f32 dt)
{
	ActionAge += dt;
	if (PlayerAction == PlayerActionState::Interaction && Interaction)
	{
		const f32 effectiveActionRate = Scene.GetGameplayEdition() == GameEdition::XXX ? CurrentActionRate : GetInteractionRate();
		i32 frame = static_cast<i32>(ActionAge * effectiveActionRate);
		if (frame >= static_cast<i32>(Interaction->Frames.size()))
		{
			if (Scene.GetGameplayEdition() == GameEdition::XXX)
			{
				ActionComplete = true;
				Scene.Objects[GetIndex()].Draw.Frame = Interaction->Frames.front();
			}
			else
				FinishInteraction();
		}
		else
		{
			DrawInteractionFrame(Interaction->Frames[frame]);
		}
	}
	else if (ActionAge * (Scene.GetGameplayEdition() == GameEdition::XXX ? CurrentActionRate : GetActionRate()) < GetActionFrames().size())
	{
		DrawActionFrame(static_cast<size_t>(ActionAge * (Scene.GetGameplayEdition() == GameEdition::XXX ? CurrentActionRate : GetActionRate())));
	}
	else if (Scene.GetGameplayEdition() == GameEdition::XXX && PlayerAction != PlayerActionState::Whistle)
	{
		ActionComplete = true;
		DrawActionFrame(0);
	}
	else
		FinishAction();
}
