#include "Scene/Characters/Characters.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

AnimalCharacter::AnimalCharacter(LevelScene& scene, size_t index, ObjectPhase phase)
    : BaseCharacter(scene, phase, index)
{
	const f64 speed = scene.Engine.GetData().NumericParam("SheepSpeed", 50, scene.GetGameplayEdition());
	SheepSpeed = std::isfinite(speed) && speed > 0 ? static_cast<f32>(speed) : 50;
}

void AnimalCharacter::Update(f32 seconds)
{
	UpdateOtherAnimal(seconds);
}

void AnimalCharacter::Resolve()
{
	const i32 owner = GetState().InteractionPlayer;
	LevelScene::PlayerScope scope(Scene, owner >= 0 && static_cast<size_t>(owner) < Scene.GetPlayerCount() ? static_cast<size_t>(owner) : Scene.GetPlayer().GetNumber());
	ResolveSheep(GetState());
}

void Sheep::Update(f32 seconds)
{
	UpdateSheep(seconds);
}

void AnimalCharacter::UpdateOtherAnimal(f32 seconds)
{
	if (Scene.GetGameplayEdition() == GameEdition::XXX)
	{
		UpdateXxxSmallAnimal(seconds);
		return;
	}
	const size_t i = GetIndex();
	WorldObject& actor = GetState();
	if (actor.Freed || !Scene.IsAnimal(actor.Kind) || actor.Kind == ObjectKind::Sheep)
		return;
	if (static_cast<i32>(i) == Scene.GetPlayer().ActiveSheep && Scene.GetPlayer().PlayerAction != PlayerActionState::Whistle)
		return;
	if (actor.State == ActorState::BeamingUp)
	{
		actor.Lift -= 20 * seconds;
		if (actor.Lift < -27)
		{
			Scene.RemoveActor(i, false);
			return;
		}
	}
	else if (actor.State == ActorState::Turning)
	{
		actor.Idle -= 60 * seconds;
		if (actor.Idle < 0)
		{
			actor.Facing = actor.TurnTo;
			actor.TurnTo = -1;
			actor.State = ActorState::Idle;
		}
	}
	if (actor.State == ActorState::Idle || actor.State == ActorState::Turning)
	{
		actor.Pulse -= 60 * seconds;
		if (actor.Pulse < 0)
		{
			actor.Pulse = 26;
			actor.MoodBlink ^= 1;
		}
	}
	actor.Draw = { Scene.Resources.ActorSprites[actor.Kind].Stand, static_cast<i32>(actor.X), static_cast<i32>(actor.Y + actor.Lift), actor.Facing };
}

void AnimalCharacter::UpdateXxxSmallAnimal(f32 seconds)
{
	WorldObject& actor = GetState();
	if (actor.Freed || actor.Kind == ObjectKind::Sheep)
		return;
	const ActorState initialState = actor.State;
	for (size_t number = 0; number < Scene.GetPlayerCount(); ++number)
	{
		LevelScene::PlayerScope scope(Scene, number);
		if (Scene.GetPlayer().ActiveSheep == static_cast<i32>(GetIndex()) && Scene.GetPlayer().PlayerAction != PlayerActionState::Whistle)
			return;
	}
	if (actor.State == ActorState::BeamingUp)
	{
		actor.Lift -= seconds * 20;
		if (actor.Lift < -27)
		{
			Scene.RemoveActor(GetIndex(), false);
			return;
		}
	}
	else if (actor.State == ActorState::Turning)
	{
		actor.Idle -= seconds * 60;
		if (actor.Idle < 0)
		{
			actor.Facing = actor.TurnTo;
			actor.TurnTo = -1;
			actor.State = ActorState::Idle;
			actor.Idle = 0;
		}
	}
	else if (actor.State == ActorState::Idle)
	{
		actor.Idle -= seconds * 60;
		if (actor.Idle < 0)
		{
			bool playerNearby = false;
			for (size_t number = 0; number < Scene.GetPlayerCount(); ++number)
			{
				LevelScene::PlayerScope scope(Scene, number);
				const i32 tile = Scene.Grid.TileAt(Scene.GetPlayer().X, Scene.GetPlayer().Y);
				playerNearby = playerNearby || (actor.Tile >= 0 && tile >= 0 && std::abs(tile % Stride - actor.Tile % Stride) < 5 && std::abs(tile / Stride - actor.Tile / Stride) < 5);
			}
			if (playerNearby)
				actor.Idle = static_cast<f32>(Scene.RandomInt(1000) + 450);
			else
			{
				const i32 choice = Scene.RandomInt(15);
				if (choice == 2)
				{
					i32 facing;
					do
						facing = Scene.RandomInt(4);
					while (facing == actor.Facing);
					actor.Facing = facing;
				}
				const SmallAnimalState action = choice == 1 ? SmallAnimalState::Eating : (choice == 5 ? SmallAnimalState::IdleAnimation : SmallAnimalState::ZeroAnimation);
				if (choice == 1 || choice == 5 || (choice >= 6 && (actor.Facing == 1 || actor.Facing == 2)))
				{
					actor.State = ToActorState(action);
					actor.Age = 0;
					actor.AnimRate = 15;
				}
				else
					actor.Idle = static_cast<f32>(Scene.RandomInt(1000) + 450);
			}
		}
	}
	if (initialState == ActorState::Idle || initialState == ActorState::Turning)
	{
		actor.Pulse -= seconds * 60;
		if (actor.Pulse < 0)
		{
			actor.Pulse = 25;
			actor.MoodBlink ^= 1;
		}
	}
	const SmallAnimalState state = static_cast<SmallAnimalState>(actor.State);
	const ShagAnimation* animation = nullptr;
	if (state == SmallAnimalState::Eating)
		animation = &Scene.Resources.AnimalEating.at(actor.Kind).at(actor.Facing);
	else if (state == SmallAnimalState::IdleAnimation)
		animation = &Scene.Resources.AnimalIdle.at(actor.Kind).at(actor.Facing);
	else if (state == SmallAnimalState::ZeroAnimation)
		animation = &Scene.Resources.AnimalZero.at(actor.Kind);
	if (animation && actor.State == initialState && !animation->Frames.empty())
	{
		actor.Age += seconds * 15;
		if (actor.Age >= animation->Frames.size())
		{
			actor.State = ActorState::Idle;
			actor.Idle = static_cast<f32>(Scene.RandomInt(1000) + 450);
		}
		else
		{
			actor.Draw = { animation->Sprite, static_cast<i32>(actor.X), static_cast<i32>(actor.Y + actor.Lift), animation->Frames[static_cast<size_t>(actor.Age)], animation->Mirror };
			return;
		}
	}
	actor.Draw = { Scene.Resources.ActorSprites.at(actor.Kind).Stand, static_cast<i32>(actor.X), static_cast<i32>(actor.Y + actor.Lift), actor.Facing };
}

void AnimalCharacter::ResolveSheep(Object& animal)
{
	// 0x407911..0x407B08: first resolved Frigida is special; otherwise first
	// attempt earns 15, a later attempt 10. Time bonuses are not score awards.
	const bool special = animal.Kind == ObjectKind::Sheep && Scene.CompletedSheep == 0 && animal.Temperament == 2;
	if (animal.Kind != ObjectKind::Sheep)
	{
		Scene.AddScore(10);
		Scene.Effects.AddTime(10, animal.X + 60, animal.Y + 60);
	}
	else if (special)
	{
		Scene.AddScore(20);
		Scene.Effects.AddTime(10, animal.X + 60, animal.Y + 60);
	}
	else
	{
		Scene.AddScore(animal.Attempts == 1 ? 15 : 10);
		Scene.Effects.AddTime(5, animal.X + 60, animal.Y + 60);
	}
	if (special)
	{
		for (i32 i = 0; i < 4; ++i)
			Scene.Effects.SpawnCompletionEffect(animal.X + 60 + Scene.RandomInt(160) - 80, animal.Y + 60 + Scene.RandomInt(160) - 80, true);
	}
	else
	{
		Scene.Effects.SpawnCompletionEffect(animal.X + 60, animal.Y + 60, animal.Kind != ObjectKind::Sheep || animal.Attempts == 1);
	}
	if (animal.Kind == ObjectKind::Sheep)
		++Scene.CompletedSheep;
	if (Scene.GetGameplayEdition() == GameEdition::XXX)
		Scene.EmitNamedSound("SfxExplode");
	else
		Scene.Sounds.push_back("Sven2/sfx/sheep_explode3.ogg");
	i32 oldTile = Scene.Grid.TileAt(Scene.GetPlayer().X, Scene.GetPlayer().Y);
	if (oldTile >= 0)
		Scene.Grid.Tiles[oldTile] = OccupancyCode::Empty;
	if (animal.Tile >= 0)
	{
		Scene.Grid.Tiles[animal.Tile] = OccupancyCode::Player;
		Scene.Grid.TileDirections[animal.Tile] = 0;
	}
	animal.Freed = true;
	animal.State = ActorState::Resolved;
	animal.Age = 0;
	--Scene.RemainingTargets;
	Scene.GetPlayer().X = Scene.GetPlayer().TargetX = animal.X;
	Scene.GetPlayer().Y = Scene.GetPlayer().TargetY = animal.Y;
	Scene.GetPlayer().Facing = 2 * animal.Facing + 1;
	Scene.Objects[Scene.GetPlayerIndex()].CommittedY = Scene.GetPlayer().Y;
	Scene.GetPlayer().ActionQueued = false;
	if (Scene.GetGameplayEdition() == GameEdition::XXX)
		Scene.GetPlayer().MarkPlayerCell();
}

bool AnimalCharacter::AgeSheepMood(Object& animal, f32 seconds)
{
	// 0x408326..0x408436: the pulse also advances during chewing and turning.
	animal.Pulse -= seconds * 60;
	if (animal.Pulse >= 0)
		return false;
	animal.Pulse = 26;
	animal.MoodBlink ^= 1;
	if (animal.Temperament == 2 || ++animal.AngerTicks <= 40)
		return false;
	animal.AngerTicks = 0;
	animal.Impatience += 2;
	if (animal.Impatience < 12)
		return false;
	if (Scene.RandomInt(5) != 0)
	{
		animal.Impatience = 10;
		return false;
	}
	animal.State = ActorState::DemonTransforming;
	animal.Age = animal.Pulse = 0;
	animal.Draw = { Scene.Resources.DemonSheep[animal.Facing], static_cast<i32>(animal.X), static_cast<i32>(animal.Y), 0, SheepMirror[animal.Facing] };
	Scene.EmitNamedSound("SfxDevil");
	return true;
}

bool AnimalCharacter::MoveDemonSheep()
{
	const size_t index = GetIndex();
	WorldObject& animal = Scene.Objects[index];
	const i32 playerTile = Scene.Grid.TileAt(Scene.GetPlayer().X, Scene.GetPlayer().Y);
	const i32 preferred = animal.Tile % Stride > playerTile % Stride ? (animal.Tile / Stride > playerTile / Stride ? 3 : 2) : (animal.Tile / Stride <= playerTile / Stride ? 1 : 0);
	// Literal fallback order in 0x408880. Despite the old analyst name, it
	// chooses a diagonal toward Sven, then tries the other Directions.
	static constexpr i32 order[4][4] = { { 0, 3, 1, 2 }, { 1, 0, 2, 3 }, { 2, 3, 1, 0 }, { 3, 2, 0, 1 } };
	static constexpr i32 steps[4][2] = { { 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 } };
	for (i32 facing : order[preferred])
	{
		const f32 nx = animal.X + 40 * steps[facing][0], ny = animal.Y + 40 * steps[facing][1];
		const i32 tile = Scene.Grid.TileAt(nx, ny);
		if (!Scene.Objects.CanOccupy(tile, index))
			continue;
		animal.Facing = facing;
		animal.TargetX = nx;
		animal.TargetY = ny;
		animal.Age = 0;
		Scene.Grid.Reservations[tile] = static_cast<i32>(index);
		return true;
	}
	return false;
}

bool AnimalCharacter::ShockPlayer(Object& animal, i32 timeChange)
{
	Object facing = animal;
	facing.Facing = 2 * animal.Facing + 1;
	if (!Scene.Objects.CanCatchPlayer(facing))
		return false;
	// 0x42C990 cancels interpolation, the current interaction and powerup.
	Scene.GetPlayer().CancelPlayerStep();
	Scene.GetPlayer().ActiveSheep = -1;
	Scene.GetPlayer().PlayerAction = PlayerActionState::Idle;
	Scene.GetPlayer().Interaction = nullptr;
	Scene.GetPlayer().ActionQueued = false;
	Scene.GetPlayer().SelectedInteraction = -1;
	Scene.Items.GetPowerup().Code = ItemCode::None;
	Scene.Items.GetPowerup().Remaining = Scene.Items.GetPowerup().Age = 0;
	Scene.GetPlayer().PlayerShockAge = 0;
	if (Scene.GetGameplayEdition() == GameEdition::XXX)
		Scene.GetPlayer().RestorePlayer();
	animal.State = ActorState::DemonShocking;
	animal.Age = 0;
	animal.Steps = 0;
	Scene.Effects.AddTime(timeChange, animal.X + animal.CenterX, animal.Y + animal.CenterY);
	Scene.EmitNamedSound("SfxDevilElectro");
	Object flash;
	flash.Kind = ObjectKind::ElectricFlash;
	flash.X = animal.X;
	flash.Y = animal.Y;
	flash.CommittedY = animal.Y;
	flash.DepthOffset = Scene.Resources.Flash->FrameHeight;
	flash.Draw = { Scene.Resources.Flash, static_cast<i32>(animal.X), static_cast<i32>(animal.Y), 0 };
	Scene.Effects.SceneEffects.push_back(flash);
	return true;
}

void AnimalCharacter::UpdateSheep(f32 seconds)
{
	const size_t i = GetIndex();
	WorldObject& animal = GetState();
	if (animal.Kind != ObjectKind::Sheep)
		return;
	if (animal.Freed)
	{
		animal.Age += seconds;
		return;
	}
	if (animal.State == ActorState::BeamingUp)
	{
		animal.Lift -= 20 * seconds;
		animal.Age += seconds;
		animal.Draw = { Scene.Resources.SheepStand, static_cast<i32>(animal.X), static_cast<i32>(animal.Y + animal.Lift), animal.Facing };
		if (animal.Lift < -27)
			Scene.RemoveActor(i, false);
		return;
	}
	if (IsDemonState(animal.State))
	{
		if (animal.State == ActorState::DemonWalking)
		{
			animal.Age += seconds * 20;
			if (animal.Age >= 8)
				animal.Age = 0;
			const f32 dx = animal.TargetX - animal.X, dy = animal.TargetY - animal.Y;
			const f32 distance = std::max(std::abs(dx), std::abs(dy));
			const f32 fraction = distance ? std::min(1.0f, seconds * SheepSpeed * Scene.Parameter("SheepRunMultiplier", 1) / distance) : 1;
			animal.X += dx * fraction;
			animal.Y += dy * fraction;
			animal.Draw = { Scene.Resources.DemonWalks[animal.Facing], static_cast<i32>(animal.X), static_cast<i32>(animal.Y), static_cast<i32>(animal.Age) % 8, SheepMirror[animal.Facing] };
			if (fraction < 1)
				return;
			if (animal.Tile >= 0 && Scene.Grid.Tiles[animal.Tile] == OccupancyCode::Sheep)
				Scene.Grid.Tiles[animal.Tile] = OccupancyCode::Empty;
			animal.Tile = Scene.Grid.TileAt(animal.X, animal.Y);
			animal.CommittedY = animal.Y;
			if (animal.Tile >= 0)
			{
				Scene.Grid.Tiles[animal.Tile] = OccupancyCode::Sheep;
				Scene.Grid.Reservations[animal.Tile] = -1;
			}
			if (++animal.Steps == 3)
			{
				animal.State = ActorState::DemonRecovering;
				animal.Age = 0;
				animal.Draw.Frame = 8;
			}
			else if (!ShockPlayer(animal, -15) && !MoveDemonSheep())
			{
				animal.State = ActorState::DemonRecovering;
				animal.Age = 0;
				animal.Draw.Frame = 0;
			}
		}
		else
		{
			const ActorState previousState = animal.State;
			if (Scene.AdvanceActor(animal, previousState == ActorState::DemonTransforming ? AnimationSequence::DemonTransform : (previousState == ActorState::DemonShocking ? AnimationSequence::DemonShock : AnimationSequence::DemonRecover), 20, seconds))
			{
				if (previousState == ActorState::DemonRecovering)
				{
					animal.State = ActorState::Idle;
					animal.Impatience = 8;
					animal.Pulse = 0;
					animal.Idle = static_cast<f32>(250 + Scene.RandomInt(1000));
					animal.Draw = { Scene.Resources.SheepStand, static_cast<i32>(animal.X), static_cast<i32>(animal.Y), animal.Facing };
					return;
				}
				if (previousState == ActorState::DemonShocking || !ShockPlayer(animal, 10))
				{
					if (previousState == ActorState::DemonShocking || !MoveDemonSheep())
					{
						animal.State = ActorState::DemonRecovering;
						animal.Age = 0;
						animal.Draw.Frame = previousState == ActorState::DemonShocking ? 8 : 0;
					}
					else
					{
						animal.State = ActorState::DemonWalking;
						animal.Steps = 0;
					}
				}
			}
		}
		animal.Draw.Sprite = animal.State == ActorState::DemonWalking ? Scene.Resources.DemonWalks[animal.Facing] : Scene.Resources.DemonSheep[animal.Facing];
		animal.Draw.X = static_cast<i32>(animal.X);
		animal.Draw.Y = static_cast<i32>(animal.Y);
		animal.Draw.Mirror = SheepMirror[animal.Facing];
		return;
	}
	if (static_cast<i32>(i) == Scene.GetPlayer().ActiveSheep && Scene.GetPlayer().PlayerAction != PlayerActionState::Whistle)
		return;
	animal.Age += seconds;
	if (animal.State == ActorState::Turning)
	{
		i32 turnFrame = static_cast<i32>(animal.Age * 20);
		if (turnFrame < 42)
		{
			animal.Draw = { Scene.Resources.Turns[animal.Facing], static_cast<i32>(animal.X), static_cast<i32>(animal.Y), TurnFrames[turnFrame], SheepMirror[animal.Facing] };
			AgeSheepMood(animal, seconds);
			return;
		}
		if (turnFrame >= 42)
		{
			animal.Facing = animal.TurnTo;
			animal.TurnTo = -1;
			animal.State = ActorState::Idle;
			animal.Idle = static_cast<f32>(450 + Scene.RandomInt(1000));
			if (animal.Temperament == 0 && animal.Impatience > 1)
				animal.Impatience -= 2;
		}
	}
	else if (animal.State == ActorState::AnimalWalking)
	{
		f32 vx = animal.TargetX - animal.X, vy = animal.TargetY - animal.Y;
		f32 distance = std::max(std::abs(vx), std::abs(vy));
		f32 step = distance ? std::min(1.0f, seconds * SheepSpeed / distance) : 1;
		animal.X += vx * step;
		animal.Y += vy * step;
		animal.Draw = { Scene.Resources.SheepWalks[animal.Facing], static_cast<i32>(animal.X), static_cast<i32>(animal.Y), static_cast<i32>(animal.Age * 20) % 15, SheepMirror[animal.Facing] };
		if (step < 1)
			return;
		if (animal.Tile >= 0 && Scene.Grid.Tiles[animal.Tile] == OccupancyCode::Sheep)
			Scene.Grid.Tiles[animal.Tile] = OccupancyCode::Empty;
		animal.Tile = Scene.Grid.TileAt(animal.X, animal.Y);
		animal.CommittedY = animal.Y;
		if (animal.Tile >= 0)
		{
			Scene.Grid.Tiles[animal.Tile] = OccupancyCode::Sheep;
			Scene.Grid.Reservations[animal.Tile] = -1;
		}
		animal.State = ActorState::Idle;
	}
	else if (animal.State == ActorState::Chewing)
	{
		if (animal.Age * 20 < 42)
		{
			animal.Draw = { Scene.Resources.Chewing, static_cast<i32>(animal.X), static_cast<i32>(animal.Y), ChewFrames[static_cast<i32>(animal.Age * 20)], animal.Facing == 1 };
			AgeSheepMood(animal, seconds);
			return;
		}
		animal.State = ActorState::Idle;
		animal.Idle = static_cast<f32>(250 + Scene.RandomInt(1000));
	}
	else
	{
		animal.Idle -= seconds * 60;
		if (animal.Idle < 0 && !Scene.Objects.PlayerInRange(animal, 5))
		{
			animal.Idle = static_cast<f32>(450 + Scene.RandomInt(1000));
			i32 choice = Scene.RandomInt(5);
			if (choice == 1 && (animal.Facing == 1 || animal.Facing == 2))
			{
				animal.State = ActorState::Chewing;
				animal.Age = 0;
			}
			else if (choice == 2)
				animal.Facing = Scene.RandomInt(4);
			else if (choice == 3)
			{
				// Sheep move one diagonal cell along their four authored facings.
				static constexpr i32 steps[4][2] = { { 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 } };
				f32 nx = animal.X + 40 * steps[animal.Facing][0], ny = animal.Y + 40 * steps[animal.Facing][1];
				i32 target = Scene.Grid.TileAt(nx, ny);
				if (Scene.Objects.CanOccupy(target, i))
				{
					animal.TargetX = nx;
					animal.TargetY = ny;
					animal.State = ActorState::AnimalWalking;
					animal.Age = 0;
					Scene.Grid.Reservations[target] = static_cast<i32>(i);
				}
				else
					animal.Facing = Scene.RandomInt(4);
			}
		}
	}
	if (animal.State != ActorState::AnimalWalking && AgeSheepMood(animal, seconds))
		return;
	animal.Draw = { Scene.Resources.SheepStand, static_cast<i32>(animal.X), static_cast<i32>(animal.Y), animal.Facing };
}

const ShagAnimation* AnimalCharacter::GetInteraction(i32 facing) const
{
	const auto found = Scene.Resources.AnimalActions.find(GetState().Kind);
	if (found == Scene.Resources.AnimalActions.end() || facing < 0 || facing >= static_cast<i32>(found->second.size()))
		return nullptr;
	return &found->second[facing];
}

Mermaid::Mermaid(LevelScene& scene, size_t index)
    : BaseCharacter(scene, ObjectPhase::Animals, index)
{}

MermaidState Mermaid::GetMermaidState() const
{
	return static_cast<MermaidState>(State.State);
}

void Mermaid::Initialize()
{
	if (Scene.GetGameplayEdition() != GameEdition::XXX)
		throw std::logic_error("Mermaids require the XXX edition profile.");
	Scene.Resources.NamedSprite("MermaidJump");
	Scene.Resources.NamedSprite("MermaidShag");
	if (GetMermaidState() == MermaidState::Dormant)
		ClearCell();
	Draw();
}

void Mermaid::SetState(MermaidState state)
{
	State.State = static_cast<ActorState>(state);
	State.Age = 0;
	Draw();
}

void Mermaid::ClearCell()
{
	if (State.Tile < 0 || State.Tile >= static_cast<i32>(Scene.Grid.Tiles.size()))
		return;
	if (Scene.Grid.Tiles[State.Tile] == OccupancyCode::Mermaid && (Scene.Grid.Reservations[State.Tile] == -1 || Scene.Grid.Reservations[State.Tile] == static_cast<i32>(GetIndex())))
	{
		Scene.Grid.Tiles[State.Tile] = OccupancyCode::Empty;
		Scene.Grid.Reservations[State.Tile] = -1;
	}
}

bool Mermaid::Reveal()
{
	if (State.Freed || GetMermaidState() != MermaidState::Dormant || State.Tile < 0 || State.Tile >= static_cast<i32>(Scene.Grid.Tiles.size()) || Scene.Grid.Tiles[State.Tile] != OccupancyCode::Empty)
		return false;
	Scene.Grid.Tiles[State.Tile] = OccupancyCode::Mermaid;
	Scene.Grid.Reservations[State.Tile] = static_cast<i32>(GetIndex());
	Cycles = 0;
	SetState(MermaidState::Emerging);
	return true;
}

bool Mermaid::BeginInteraction(size_t playerNumber, i32 facing)
{
	if (GetMermaidState() != MermaidState::Idle || playerNumber >= Scene.GetPlayerCount() || facing < 2 || facing > 4)
		return false;
	LinkedPlayer = static_cast<i32>(playerNumber);
	State.InteractionPlayer = LinkedPlayer;
	State.Attempts = 0;
	State.State = static_cast<ActorState>(MermaidState::Interacting);
	Scene.EmitNamedSound("SfxShagMermaid");
	return true;
}

bool Mermaid::FinishInteraction(bool repeat)
{
	if (State.Freed || GetMermaidState() != MermaidState::Interacting || LinkedPlayer < 0)
		return false;
	Scene.AddScore(16);
	if (++State.Attempts > 6)
	{
		SetState(MermaidState::Resolved);
		ReleasedPlayer = LinkedPlayer;
		LinkedPlayer = -1;
		State.InteractionPlayer = -1;
		return false;
	}
	if (repeat)
	{
		Scene.EmitNamedSound("SfxShagMermaid");
		return true;
	}
	ReleasedPlayer = LinkedPlayer;
	LinkedPlayer = -1;
	State.InteractionPlayer = -1;
	State.State = static_cast<ActorState>(MermaidState::Idle);
	return false;
}

i32 Mermaid::TakeReleasedPlayer()
{
	const i32 number = ReleasedPlayer;
	ReleasedPlayer = -1;
	return number;
}

bool Mermaid::Advance(cstr sequence, f32 rate, f32 seconds)
{
	const std::vector<i32>& frames = Scene.Resources.FrameList(sequence);
	if (frames.empty())
		throw std::runtime_error(str("Missing XXX animation: ") + sequence);
	State.Age += seconds * rate;
	if (State.Age >= static_cast<f32>(frames.size()))
	{
		State.Age = 0;
		return true;
	}
	return false;
}

void Mermaid::Draw()
{
	State.Draw = {};
	if (State.Freed || GetMermaidState() == MermaidState::Dormant || GetMermaidState() == MermaidState::Resolved)
		return;
	cstr sequence = GetMermaidState() == MermaidState::Emerging ? "aMermaidJump" : (GetMermaidState() == MermaidState::Submerging ? "aMermaidJumpBack" : "aMermaidIdle");
	const std::vector<i32>& frames = Scene.Resources.FrameList(sequence);
	if (frames.empty())
		throw std::runtime_error(str("Missing XXX animation: ") + sequence);
	const size_t frame = std::min(static_cast<size_t>(State.Age), frames.size() - 1);
	State.Draw = { Scene.Resources.NamedSprite("MermaidJump"), static_cast<i32>(State.X), static_cast<i32>(State.Y), frames[frame] };
	State.CommittedY = State.Y;
}

void Mermaid::Update(f32 seconds)
{
	if (State.Freed)
		return;
	switch (GetMermaidState())
	{
	case MermaidState::Dormant:
		ClearCell();
		break;
	case MermaidState::Emerging:
		if (Advance("aMermaidJump", 15, seconds))
		{
			Cycles = 0;
			SetState(MermaidState::Idle);
		}
		break;
	case MermaidState::Idle:
	case MermaidState::Interacting:
		// 0x40BF47..54 and 0x40C020..2D: C0|C3 skips the branch (<=10).
		if (Advance("aMermaidIdle", 5, seconds) && ++Cycles > 10)
		{
			ClearCell();
			ReleasedPlayer = LinkedPlayer;
			LinkedPlayer = -1;
			State.InteractionPlayer = -1;
			SetState(MermaidState::Submerging);
		}
		break;
	case MermaidState::Submerging:
		if (Advance("aMermaidJumpBack", 15, seconds))
			SetState(MermaidState::Dormant);
		break;
	case MermaidState::Resolved:
		ClearCell();
		Scene.AddScore(30);
		// Native 0x40AA10 calls 0x41E850(10, slot): this is a time popup, not ten lives.
		Scene.Effects.AddTime(10, State.X + State.CenterX, State.Y + State.CenterY);
		Scene.Effects.SpawnCompletionEffect(State.X + State.CenterX, State.Y + State.CenterY, true);
		Scene.EmitNamedSound("SfxExplode");
		State.Freed = true;
		break;
	}
	Draw();
}
