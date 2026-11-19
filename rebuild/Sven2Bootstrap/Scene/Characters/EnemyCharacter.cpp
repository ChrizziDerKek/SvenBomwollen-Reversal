#include "Scene/Objects/ObjectCatalog.h"
#include "Scene/Characters/EnemyCharacter.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

EnemyCharacter::EnemyCharacter(LevelScene& scene, size_t index)
    : BaseCharacter(scene, ObjectPhase::Enemies, index)
{}

void EnemyCharacter::Initialize()
{
	DrawEnemy(GetState());
}

void EnemyCharacter::Update(f32 seconds)
{
	UpdateEnemy(seconds);
}

void EnemyCharacter::EnemyIdle(Object& actor, bool retry)
{
	const ::EnemyRules& rules = Scene.Resources.EnemyRules[actor.Kind];
	actor.State = ActorState::Idle;
	actor.Age = 0;
	actor.Hunting = false;
	actor.ItemTarget = -1;
	actor.Retry = retry;
	if (actor.Kind == ObjectKind::Alien)
	{
		actor.IdleAnimating = false;
		actor.Idle = static_cast<f32>(50 + Scene.RandomInt(100));
	}
	actor.Pulse = rules.Idle + (actor.Kind == ObjectKind::Farmer || actor.Kind == ObjectKind::Girl ? rules.IdleRandom : Scene.RandomInt(static_cast<i32>(rules.IdleRandom)));
	actor.TargetX = actor.X;
	actor.TargetY = actor.Y;
}

void EnemyCharacter::EnemyNotice(const Object& actor, bool item)
{
	Scene.EmitNamedSound(actor.Kind == ObjectKind::Alien ? "SfxAlienFoundSven" : (actor.Kind == ObjectKind::Dog ? (item ? "SfxDogFoundBone" : "SfxDogFoundSven") : (actor.Kind == ObjectKind::Farmer ? (item ? "SfxGuyFoundBeer" : "SfxGuyFoundSven") : (item ? "SfxBroemseFoundTeddy" : "SfxBroemseFoundSven"))));
}

bool EnemyCharacter::ChooseEnemyStep()
{
	const size_t index = GetIndex();
	WorldObject& actor = GetState();
	i32 preferred = actor.Facing;
	if (actor.Hunting)
	{
		i32 target = Scene.Grid.TileAt(Scene.GetPlayer().X, Scene.GetPlayer().Y);
		if (actor.ItemTarget >= 0 && !Scene.Objects[actor.ItemTarget].Freed)
			target = Scene.Objects[actor.ItemTarget].Tile;
		else if (Scene.GetCombat().Enemy >= 0 || Scene.GetPlayer().PlayerBeamState || Scene.GetPlayer().SplashAge * Scene.GetPlayer().DefaultRate < 16)
			actor.Hunting = false;
		if (actor.Hunting && target >= 0)
			preferred = EnemyDirection(actor.Tile, target);
	}
	auto attempt = [&](i32 direction) {
		const i32* d = Directions[direction];
		i32 x = actor.Tile % Stride + d[0], y = actor.Tile / Stride + d[1];
		// Original helpers allow column 21 / row 16 if their border cell is OccupancyCode::Empty.
		if (x < 1 || x > Columns + 1 || y < 1 || y > Rows + 1)
			return false;
		const i32 tile = y * Stride + x;
		if (Scene.Grid.Tiles[tile] != OccupancyCode::Empty || Scene.Grid.Reservations[tile] >= 0)
			return false;
		// Rejected attempts have no facing, velocity, or reservation side effects.
		actor.Facing = direction;
		actor.TargetX = actor.X + 40 * d[0];
		actor.TargetY = actor.Y + 40 * d[1];
		Scene.Grid.Reservations[tile] = static_cast<i32>(index);
		return true;
	};
	if (actor.Hunting && actor.Kind != ObjectKind::Farmer && attempt(preferred))
		return true;
	for (i32 tries = 0; tries < 128; ++tries)
		if (attempt(Steering[EnemyTableIndex(actor.Kind)][preferred][Scene.RandomInt(39)]))
			return true;
	return false;
}

void EnemyCharacter::DrawEnemy(Object& actor)
{
	const ::ActorSprites& images = Scene.Resources.ActorSprites[actor.Kind];
	const Sprite* image = images.Stand;
	i32 frame = actor.Facing;
	i32 x = static_cast<i32>(actor.X), y = static_cast<i32>(actor.Y + actor.Lift);
	bool mirror = false;
	const i32 quadrant = actor.Facing / 2;
	if (actor.Kind == ObjectKind::Alien && actor.State == ActorState::Idle && actor.IdleAnimating)
	{
		image = images.Idle[actor.Facing];
		frame = actor.Draw.Frame;
		mirror = EnemyWalkMirror[0][actor.Facing];
	}
	else if (actor.State == ActorState::Walking)
	{
		image = images.Walk[actor.Facing];
		frame = actor.Kind == ObjectKind::Girl ? ActorFrameTables.at(AnimationSequence::GirlWalk)[static_cast<size_t>(actor.Age) % ActorFrameTables.at(AnimationSequence::GirlWalk).size()] : static_cast<i32>(actor.Age) % image->Rows;
		mirror = EnemyWalkMirror[EnemyTableIndex(actor.Kind)][actor.Facing];
	}
	else if (actor.Kind == ObjectKind::Farmer && (actor.State == ActorState::FarmerGrumblingBack || actor.State == ActorState::FarmerGrumblingFront))
	{
		image = images.Grumble[quadrant];
		frame = actor.Draw.Frame;
		mirror = quadrant >= 2;
	}
	else if (actor.Kind == ObjectKind::Girl && (actor.State == ActorState::GirlPlayingDogBack || actor.State == ActorState::GirlPlayingDogFront))
	{
		image = images.DogPlay[quadrant];
		frame = actor.Draw.Frame;
		mirror = quadrant < 2;
	}
	else if (actor.State == ActorState::Sleeping)
	{
		image = images.Sleep[quadrant];
		if (actor.Kind == ObjectKind::Farmer)
		{
			const f32 bodyAge = std::fmod(actor.Age, 28.0f);
			frame = bodyAge > 14 ? 14 : 13;
			x -= 50;
			y -= 20;
		}
		else
		{
			frame = 0;
		}
		mirror = actor.Kind == ObjectKind::Dog ? (quadrant == 0 || quadrant == 2) : quadrant < 2;
	}
	else if (actor.State == ActorState::FarmerWaking && actor.Kind == ObjectKind::Farmer)
	{
		image = images.Wake[quadrant];
		frame = actor.Draw.Frame;
		x -= 50;
		y -= 20;
		mirror = quadrant >= 2;
	}
	else if (actor.State == ActorState::FarmerLyingDown && actor.Kind == ObjectKind::Farmer)
	{
		image = images.Sleep[quadrant];
		frame = actor.Draw.Frame;
		x -= 50;
		y -= 20;
		mirror = quadrant < 2;
	}
	else if (actor.Kind == ObjectKind::Dog && actor.State == ActorState::DogDigging)
	{
		image = images.Sleep[quadrant];
		frame = actor.Draw.Frame;
		mirror = quadrant == 0 || quadrant == 2;
	}
	else if (actor.State == ActorState::DistractionPrimary || actor.State == ActorState::DistractionSecondary)
	{
		if (images.Distraction[quadrant])
		{
			image = images.Distraction[quadrant];
			frame = actor.Draw.Frame;
			mirror = actor.Kind == ObjectKind::Girl ? (quadrant == 0 || quadrant == 1) : quadrant >= 2;
		}
	}
	else if (actor.State == ActorState::BeamingUp && images.Beam[quadrant])
	{
		image = images.Beam[quadrant];
		frame = std::min(image->Rows - 1, static_cast<i32>(actor.Age * (actor.Kind == ObjectKind::Dog || actor.Kind == ObjectKind::Girl ? 4 : 15)));
		mirror = actor.Kind == ObjectKind::Girl ? quadrant < 2 : quadrant >= 2;
	}
	else if (actor.State == ActorState::FightingBack || actor.State == ActorState::FightingFront)
	{
		if (images.Battle[quadrant])
		{
			image = images.Battle[quadrant];
			frame = actor.Draw.Frame;
			mirror = actor.Kind == ObjectKind::Girl ? quadrant < 2 : quadrant >= 2;
			if (actor.Kind == ObjectKind::Dog || actor.Kind == ObjectKind::Farmer)
			{
				x -= 30;
				y -= 20;
				mirror = false;
			}
			if (actor.Kind == ObjectKind::Girl && actor.State == ActorState::FightingFront)
				x -= 20;
		}
	}
	if ((actor.Kind == ObjectKind::Girl && actor.State == ActorState::GirlAboardUfo) || (actor.Kind == ObjectKind::Dog && actor.State == ActorState::DogStunned))
	{
		actor.Draw = {};
		return;
	}
	if (image)
		actor.Draw = { image, x, y, std::clamp(frame, 0, image->Rows * image->Columns - 1), mirror };
}

void EnemyCharacter::EnemyArrived()
{
	const size_t index = GetIndex();
	WorldObject& actor = GetState();
	const ItemCode code = actor.Tile >= 0 ? Scene.Grid.ItemCodes[actor.Tile] : ItemCode::None;
	const ItemCode distraction = DistractionCode(actor.Kind);
	if (code == ItemCode::Artifact || (distraction != ItemCode::None && code == distraction))
	{
		Scene.Items.RemoveItem(actor.Tile);
		actor.Age = 0;
		actor.Hunting = false;
		actor.ItemTarget = -1;
		if (code == ItemCode::Artifact)
		{
			actor.State = ActorState::AwaitingUfo;
			Scene.UfoController.RequestUfo(static_cast<i32>(index));
		}
		else if (actor.Kind == ObjectKind::Dog)
		{
			actor.State = ActorState::DogDigging;
			actor.Pulse = Scene.Parameter("DogDiggingTime", 15);
			actor.Idle = 0;
			actor.Draw.Frame = 0;
			Scene.Effects.SpawnDigEffect(actor);
			Scene.EmitNamedSound("SfxDogDig");
		}
		else if (actor.Kind == ObjectKind::Farmer)
		{
			actor.State = ActorState::FarmerDrinking;
			actor.AnimRate = 10;
			Scene.EmitNamedSound("SfxLarsDrink");
		}
		else
		{
			actor.State = (actor.Facing >= 2 && actor.Facing <= 5) ? ActorState::DistractionSecondary : ActorState::DistractionPrimary;
			actor.Facing = 2 * (actor.Facing / 2) + 1;
			actor.AnimRate = 9;
			Scene.EmitNamedSound("SfxBroemsePlaysTeddy");
		}
		return;
	}
	// 0x405C50 selects the last dog in facing, -1, +1 order and stuns it.
	if (actor.Kind == ObjectKind::Girl)
	{
		i32 dog = -1;
		for (i32 offset : { 0, -1, 1 })
		{
			const i32* d = Directions[(actor.Facing + offset + 8) % 8];
			const i32 tile = actor.Tile + d[0] + Stride * d[1];
			for (size_t j = 0; j < Scene.Objects.Size(); ++j)
				if (!Scene.Objects[j].Freed && Scene.Objects[j].Kind == ObjectKind::Dog && Scene.Objects[j].Tile == tile)
					dog = static_cast<i32>(j);
		}
		if (dog >= 0)
		{
			WorldObject& target = Scene.Objects[dog];
			if (target.State != ActorState::Sleeping && target.State != ActorState::DistractionPrimary && target.State != ActorState::DistractionSecondary && target.State != ActorState::AwaitingUfo && target.State != ActorState::BeamingUp && target.State != ActorState::FightingBack)
			{
				for (i32& reservation : Scene.Grid.Reservations)
					if (reservation == dog)
						reservation = -1;
				target.X = target.TargetX = static_cast<f32>((target.Tile % Stride - 1) * 40 + 20) - 60;
				target.Y = target.TargetY = static_cast<f32>((target.Tile / Stride - 1) * 40 + 20) - 60;
				target.State = ActorState::DogStunned;
				target.Pulse = 6;
				actor.State = actor.Facing >= 2 && actor.Facing <= 5 ? ActorState::GirlPlayingDogFront : ActorState::GirlPlayingDogBack;
				actor.Age = 0;
				actor.AnimRate = 10;
				Scene.EmitNamedSound("SfxBroemsePlaysWotan");
				return;
			}
		}
	}
	if (Scene.Objects.CanCatchPlayer(actor))
	{
		BeginBattle();
		return;
	}
	const bool activity = Scene.GetPlayer().PlayerAction == PlayerActionState::Interaction || Scene.GetPlayer().PlayerAction == PlayerActionState::Frigida;
	// Lars sometimes gives up with a directional grumble once Sven stops.
	if (actor.Kind == ObjectKind::Farmer && actor.Hunting && actor.ItemTarget < 0 && !activity && Scene.RandomInt(2) == 0)
	{
		const i32 tile = Scene.Grid.TileAt(Scene.GetPlayer().X, Scene.GetPlayer().Y);
		if (tile >= 0)
			actor.Facing = EnemyDirection(actor.Tile, tile);
		actor.Steps = Scene.Resources.EnemyRules[ObjectKind::Farmer].WalkSteps + Scene.RandomInt(Scene.Resources.EnemyRules[ObjectKind::Farmer].WalkRandom);
		actor.State = actor.Facing >= 2 && actor.Facing <= 5 ? ActorState::FarmerGrumblingFront : ActorState::FarmerGrumblingBack;
		actor.Hunting = false;
		actor.Age = 0;
		actor.AnimRate = 10;
		Scene.EmitNamedSound("SfxLarsGroan");
		return;
	}
	if (actor.ItemTarget < 0 && distraction != ItemCode::None)
	{
		actor.ItemTarget = Scene.Items.FindDistraction(actor, distraction);
		if (actor.ItemTarget >= 0)
		{
			actor.Hunting = true;
			actor.Steps = Scene.Resources.EnemyRules[actor.Kind].HuntSteps;
			EnemyNotice(actor, true);
		}
	}
	if (!actor.Hunting && ((actor.Kind == ObjectKind::Farmer && activity) || (actor.Kind == ObjectKind::Dog && (activity || Scene.Objects.PlayerInRange(actor, Scene.Resources.EnemyRules[ObjectKind::Dog].Range))) || ((actor.Kind == ObjectKind::Alien || actor.Kind == ObjectKind::Girl) && Scene.Objects.PlayerInRange(actor, Scene.Resources.EnemyRules[actor.Kind].Range))))
	{
		actor.Hunting = true;
		actor.Steps = Scene.Resources.EnemyRules[actor.Kind].HuntSteps;
		EnemyNotice(actor, false);
	}
	if (--actor.Steps == 0 || !ChooseEnemyStep())
	{
		// 0x4022F0/0x4038F0/0x4063C0 draw the next walk budget before idle time.
		if (actor.Kind != ObjectKind::Alien)
			actor.Steps = Scene.Resources.EnemyRules[actor.Kind].WalkSteps + Scene.RandomInt(Scene.Resources.EnemyRules[actor.Kind].WalkRandom);
		EnemyIdle(actor);
	}
}

void EnemyCharacter::UpdateEnemy(f32 seconds)
{
	const size_t i = GetIndex();
	WorldObject& actor = GetState();
	if (actor.Freed || !Scene.IsEnemy(actor.Kind))
		return;
	const ::EnemyRules& rules = Scene.Resources.EnemyRules[actor.Kind];
	if (actor.State == ActorState::Sleeping)
	{
		actor.Age += seconds * (actor.Kind == ObjectKind::Farmer ? actor.AnimRate : 10);
		if (actor.Kind == ObjectKind::Farmer)
		{
			actor.SoundClock -= 60 * seconds;
			if (actor.SoundClock < 0)
			{
				actor.SoundClock = 300;
				Scene.EmitNamedSound("SfxLarsSleep");
			}
		}
		if (Scene.Objects.PlayerInRange(actor, rules.WakeRange))
			actor.Pulse = 0;
		actor.Pulse -= seconds;
		if (actor.Pulse < 0)
		{
			actor.Age = 0;
			actor.Retry = false;
			if (actor.Kind == ObjectKind::Farmer)
			{
				actor.State = ActorState::FarmerWaking;
				actor.AnimRate = 10;
				actor.Facing = std::min(7, actor.Facing + 1);
			}
			else
				EnemyIdle(actor);
		}
	}
	else if (actor.State == ActorState::FarmerWaking && actor.Kind == ObjectKind::Farmer)
	{
		if (Scene.AdvanceActor(actor, AnimationSequence::FarmerStandUp, actor.AnimRate, seconds))
			EnemyIdle(actor);
	}
	else if (actor.State == ActorState::Idle)
	{
		if (actor.Kind == ObjectKind::Alien)
		{
			if (!actor.IdleAnimating)
			{
				actor.Idle -= 60 * seconds;
				if (actor.Idle < 0)
				{
					actor.IdleAnimating = true;
					actor.IdleVariant = Scene.RandomInt(2);
					actor.Age = 0;
				}
			}
			if (actor.IdleAnimating)
			{
				const AnimationSequence table = actor.IdleVariant ? (actor.Facing == 0 || actor.Facing == 4 ? AnimationSequence::AlienVerticalIdle : AnimationSequence::AlienDiagonalIdle) : AnimationSequence::AlienStandingIdle;
				if (Scene.AdvanceActor(actor, table, 5, seconds))
				{
					actor.IdleAnimating = false;
					actor.Idle = static_cast<f32>(50 + Scene.RandomInt(100));
				}
			}
		}
		const ItemCode code = DistractionCode(actor.Kind);
		const bool activity = Scene.GetPlayer().PlayerAction == PlayerActionState::Interaction || Scene.GetPlayer().PlayerAction == PlayerActionState::Frigida;
		if (!actor.Retry)
		{
			if (code != ItemCode::None)
				actor.ItemTarget = Scene.Items.FindDistraction(actor, code);
			bool detect = actor.Kind == ObjectKind::Farmer ? activity : Scene.Objects.PlayerInRange(actor, rules.Range);
			if (actor.ItemTarget >= 0 || detect)
			{
				actor.Hunting = true;
				actor.Pulse = 0;
				EnemyNotice(actor, actor.ItemTarget >= 0);
			}
		}
		actor.Pulse -= seconds;
		if (actor.Pulse < 0)
		{
			actor.Retry = false;
			// The chooser must consume its random draws BEFORE the walk budget.
			// A failed chooser consumes no walk-budget draw at all.
			if (ChooseEnemyStep())
			{
				actor.Steps = actor.Hunting ? rules.HuntSteps : rules.WalkSteps + Scene.RandomInt(rules.WalkRandom);
				if (!actor.Hunting)
					actor.ItemTarget = -1;
				actor.State = ActorState::Walking;
				actor.Age = 0;
			}
			else
			{
				// Failed idle chooser keeps pursuit/item link; byte65 gates notice.
				actor.Retry = true;
				actor.Pulse = rules.Idle + (actor.Kind == ObjectKind::Farmer || actor.Kind == ObjectKind::Girl ? rules.IdleRandom : Scene.RandomInt(static_cast<i32>(rules.IdleRandom)));
			}
		}
	}
	else if (actor.State == ActorState::Walking)
	{
		const f32 vx = actor.TargetX - actor.X, vy = actor.TargetY - actor.Y;
		const f32 distance = std::max(std::abs(vx), std::abs(vy));
		const f32 speed = rules.Speed * (actor.Hunting ? rules.Run : 1) * (vx != 0 && vy != 0 ? rules.Diagonal : 1);
		const f32 fraction = distance ? std::min(1.0f, seconds * speed / distance) : 1;
		actor.X += vx * fraction;
		actor.Y += vy * fraction;
		actor.Age += seconds * (actor.Hunting ? rules.RunRate : rules.WalkRate);
		// 0x40B920/0x40B990 discard fractional overflow at sequence end;
		// a successful tile-to-tile chooser preserves the current phase.
		const size_t walkLength = actor.Kind == ObjectKind::Girl ? ActorFrameTables.at(AnimationSequence::GirlWalk).size() : static_cast<size_t>(Scene.Resources.ActorSprites[actor.Kind].Walk[actor.Facing]->Rows);
		if (actor.Age >= walkLength)
			actor.Age = 0;
		if (fraction == 1)
		{
			const i32* d = Directions[actor.Facing];
			i32 target = actor.Tile + d[0] + Stride * d[1];
			if (actor.Tile >= 0 && Scene.Grid.Tiles[actor.Tile] == Scene.GetOccupancyCode(actor.Kind))
				Scene.Grid.Tiles[actor.Tile] = OccupancyCode::Empty;
			actor.Tile = target;
			actor.CommittedY = actor.Y;
			Scene.Grid.Tiles[target] = Scene.GetOccupancyCode(actor.Kind);
			Scene.Grid.Reservations[target] = -1;
			EnemyArrived();
		}
	}
	else if (actor.Kind == ObjectKind::Farmer && (actor.State == ActorState::FarmerGrumblingBack || actor.State == ActorState::FarmerGrumblingFront))
	{
		if (Scene.AdvanceActor(actor, actor.State == ActorState::FarmerGrumblingBack ? AnimationSequence::FarmerGrumbleBack : AnimationSequence::FarmerGrumbleFront, actor.AnimRate, seconds))
			EnemyIdle(actor);
	}
	else if (actor.Kind == ObjectKind::Girl && (actor.State == ActorState::GirlPlayingDogBack || actor.State == ActorState::GirlPlayingDogFront))
	{
		if (Scene.AdvanceActor(actor, actor.State == ActorState::GirlPlayingDogBack ? AnimationSequence::GirlDogBack : AnimationSequence::GirlDogFront, actor.AnimRate, seconds))
			EnemyIdle(actor, true);
	}
	else if (actor.State == ActorState::DistractionPrimary || actor.State == ActorState::DistractionSecondary)
	{
		bool done = false;
		if (actor.Kind == ObjectKind::Dog)
		{
			actor.Draw.Frame = 0;
			actor.Idle -= 60 * seconds;
			if (actor.Idle < 0)
			{
				actor.Idle = 10;
				actor.Draw.Frame = 2;
			}
			actor.Pulse -= seconds;
			done = actor.Pulse < 0;
		}
		else if (actor.Kind == ObjectKind::Farmer)
			done = Scene.AdvanceActor(actor, actor.State == ActorState::DistractionPrimary ? AnimationSequence::FarmerDrink : AnimationSequence::FarmerLieDown, actor.AnimRate, seconds);
		else if (actor.Kind == ObjectKind::Girl)
			done = Scene.AdvanceActor(actor, actor.State == ActorState::DistractionPrimary ? AnimationSequence::GirlTeddyBack : AnimationSequence::GirlTeddyFront, actor.AnimRate, seconds);
		if (done)
		{
			if (actor.Kind == ObjectKind::Farmer && actor.State == ActorState::DistractionPrimary)
			{
				actor.State = ActorState::FarmerLyingDown;
				actor.Age = 0;
				actor.AnimRate = 10;
			}
			else if (actor.Kind == ObjectKind::Farmer)
			{
				actor.State = ActorState::Sleeping;
				actor.Pulse = rules.Sleep;
				actor.Facing = 2 * (actor.Facing / 2);
				actor.Age = 0;
				actor.SoundClock = 0;
				Scene.EmitNamedSound("SfxLarsFall");
			}
			else
				EnemyIdle(actor, actor.Kind == ObjectKind::Girl && actor.State == ActorState::DistractionSecondary);
		}
	}
	else if (actor.State == ActorState::AwaitingUfo)
	{
		Scene.UfoController.RequestUfo(static_cast<i32>(i));
	}
	else if (actor.State == ActorState::BeamingUp)
	{
		actor.Age += seconds;
		if (actor.Kind == ObjectKind::Girl)
		{
			actor.Lift -= 20 * seconds;
			if (actor.Lift < -27)
			{
				actor.Lift = 0;
				actor.State = ActorState::GirlAboardUfo;
				actor.Pulse = 4.5f;
				actor.Age = 0;
				actor.Retry = true;
				actor.Hunting = false;
				Scene.EmitNamedSound("SfxBroemsePlayAliens");
			}
		}
		else if (actor.Kind == ObjectKind::Farmer)
		{
			actor.Pulse -= 60 * seconds;
			if (actor.Pulse < 0)
				EnemyIdle(actor, true);
		}
		else
		{
			actor.Lift -= seconds * 20;
			if (actor.Lift < -27)
			{
				Scene.RemoveActor(i, false);
				return;
			}
		}
	}
	else if (actor.Kind == ObjectKind::Girl && actor.State == ActorState::GirlAboardUfo)
	{
		actor.Pulse -= seconds;
		if (actor.Pulse < 0)
		{
			actor.State = ActorState::GirlBeamingDown;
			actor.Lift = -27;
			actor.Age = 0;
			actor.Idle = 0;
			actor.Pulse = rules.Idle + rules.IdleRandom;
			actor.Retry = true;
			actor.Hunting = false;
		}
	}
	else if (actor.Kind == ObjectKind::Girl && actor.State == ActorState::GirlBeamingDown)
	{
		actor.Lift += 20 * seconds;
		if (actor.Lift > 0)
		{
			actor.Lift = 0;
			EnemyIdle(actor, true);
			Scene.EmitNamedSound("SfxSvenFall");
		}
	}
	else if (actor.State == ActorState::FightingBack || actor.State == ActorState::FightingFront)
	{
		bool done = false;
		if (actor.Kind == ObjectKind::Alien)
			done = Scene.AdvanceActor(actor, AnimationSequence::AlienProbe, 13, seconds);
		else if (actor.Kind == ObjectKind::Girl)
			done = Scene.AdvanceActor(actor, actor.State == ActorState::FightingBack ? AnimationSequence::GirlSvenBack : AnimationSequence::GirlSvenFront, actor.AnimRate, seconds);
		else
		{
			actor.Age += seconds * 15;
			actor.Draw.Frame = static_cast<i32>(actor.Age) % Scene.Resources.ActorSprites[actor.Kind].Battle[actor.Facing / 2]->Rows;
			actor.Pulse -= seconds;
			done = actor.Pulse < 0;
		}
		if (done)
			FinishBattle(actor);
	}
	DrawEnemy(actor);
}

void EnemyCharacter::BeginBattle()
{
	const size_t index = GetIndex();
	WorldObject& actor = GetState();
	Scene.GetCombat().Enemy = static_cast<i32>(index);
	Scene.GetCombat().Remaining = Scene.Resources.EnemyRules[actor.Kind].FightTime;
	actor.State = ActorState::FightingBack;
	actor.Pulse = Scene.GetCombat().Remaining;
	actor.Age = 0;
	actor.Hunting = false;
	actor.Draw.Frame = 0;
	if (actor.Kind == ObjectKind::Girl)
		actor.AnimRate = 20;
	Scene.GetPlayer().PlayerAction = PlayerActionState::Idle;
	Scene.GetPlayer().ActiveSheep = -1;
	Scene.GetPlayer().Interaction = nullptr;
	Scene.GetPlayer().ActionQueued = false;
	Scene.GetPlayer().CancelPlayerStep();
	Scene.Items.GetPowerup().Code = ItemCode::None;
	Scene.Items.GetPowerup().Remaining = 0;
	cstr sound = actor.Kind == ObjectKind::Alien ? "alien_analprobe_mix2.ogg" : (actor.Kind == ObjectKind::Dog ? "fightdog.ogg" : (actor.Kind == ObjectKind::Farmer ? "fightguy.ogg" : "broemse_plays_sven.ogg"));
	Scene.Sounds.push_back(str("Sven2/sfx/") + sound);
	if (actor.Kind == ObjectKind::Girl)
	{
		actor.State = (actor.Facing >= 2 && actor.Facing <= 5) ? ActorState::FightingFront : ActorState::FightingBack;
		Scene.Effects.AddTime(-20, actor.X + actor.CenterX, actor.Y + actor.CenterY);
	}
}

void EnemyCharacter::FinishBattle(Object& actor)
{
	if (actor.Kind != ObjectKind::Girl)
		Scene.RecordLifeLoss();
	Scene.GetCombat().Enemy = -1;
	EnemyIdle(actor, true);
	Scene.GetPlayer().RestorePlayer();
}
