#include "Scene/Characters/EnemyCharacter.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

EnemyCharacter::EnemyCharacter(LevelScene& scene, size_t index)
    : BaseCharacter(scene, ObjectPhase::Enemies, index)
{
}
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
	actor.State = 0;
	actor.Age = 0;
	actor.Hunting = false;
	actor.ItemTarget = -1;
	actor.Retry = retry;
	if (actor.Kind == Kinds::Alien)
	{
		actor.IdleAnimating = false;
		actor.Idle = static_cast<f32>(50 + Scene.RandomInt(100));
	}
	actor.Pulse = rules.Idle + (actor.Kind == Kinds::Farmer || actor.Kind == Kinds::Girl ? rules.IdleRandom : Scene.RandomInt(static_cast<i32>(rules.IdleRandom)));
	actor.TargetX = actor.X;
	actor.TargetY = actor.Y;
}

void EnemyCharacter::EnemyNotice(const Object& actor, bool item)
{
	Scene.EmitNamedSound(actor.Kind == Kinds::Alien ? "SfxAlienFoundSven" : actor.Kind == Kinds::Dog  ? (item ? "SfxDogFoundBone" : "SfxDogFoundSven")
	                                                                    : actor.Kind == Kinds::Farmer ? (item ? "SfxGuyFoundBeer" : "SfxGuyFoundSven")
	                                                                                                  : (item ? "SfxBroemseFoundTeddy" : "SfxBroemseFoundSven"));
}

bool EnemyCharacter::ChooseEnemyStep()
{
	const size_t index = GetIndex();
	WorldObject& actor = GetState();
	i32 preferred = actor.Facing;
	if (actor.Hunting)
	{
		i32 target = Scene.Grid.TileAt(Scene.Player.X, Scene.Player.Y);
		if (actor.ItemTarget >= 0 && !Scene.Objects[actor.ItemTarget].Freed)
			target = Scene.Objects[actor.ItemTarget].Tile;
		else if (Scene.Objects.Combat.Enemy >= 0 || Scene.Player.PlayerBeamState || Scene.Effects.SplashPlayerAge * Scene.Player.DefaultRate < 16)
			actor.Hunting = false;
		if (actor.Hunting && target >= 0)
			preferred = EnemyDirection(actor.Tile, target);
	}
	auto attempt = [&](i32 direction)
	{
		const i32* d = Directions[direction];
		i32 x = actor.Tile % Stride + d[0], y = actor.Tile / Stride + d[1];
		// Original helpers allow column 21 / row 16 if their border cell is Empty.
		if (x < 1 || x > Columns + 1 || y < 1 || y > Rows + 1)
			return false;
		const i32 tile = y * Stride + x;
		if (Scene.Grid.Tiles[tile] != Empty || Scene.Grid.Reservations[tile] >= 0)
			return false;
		// Rejected attempts have no facing, velocity, or reservation side effects.
		actor.Facing = direction;
		actor.TargetX = actor.X + 40 * d[0];
		actor.TargetY = actor.Y + 40 * d[1];
		Scene.Grid.Reservations[tile] = static_cast<i32>(index);
		return true;
	};
	if (actor.Hunting && actor.Kind != Kinds::Farmer && attempt(preferred))
		return true;
	for (i32 tries = 0; tries < 128; ++tries)
		if (attempt(Steering[actor.Kind - 3][preferred][Scene.RandomInt(39)]))
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
	if (actor.Kind == Kinds::Alien && actor.State == 0 && actor.IdleAnimating)
	{
		image = images.Idle[actor.Facing];
		frame = actor.Draw.Frame;
		mirror = EnemyWalkMirror[0][actor.Facing];
	}
	else if (actor.State == 1)
	{
		image = images.Walk[actor.Facing];
		frame = actor.Kind == Kinds::Girl ? ActorFrameTables.at(AnimationSequence::GirlWalk)[static_cast<size_t>(actor.Age) % ActorFrameTables.at(AnimationSequence::GirlWalk).size()] : static_cast<i32>(actor.Age) % image->Rows;
		mirror = EnemyWalkMirror[actor.Kind - 3][actor.Facing];
	}
	else if (actor.Kind == Kinds::Farmer && (actor.State == 14 || actor.State == 15))
	{
		image = images.Grumble[quadrant];
		frame = actor.Draw.Frame;
		mirror = quadrant >= 2;
	}
	else if (actor.Kind == Kinds::Girl && (actor.State == 22 || actor.State == 23))
	{
		image = images.DogPlay[quadrant];
		frame = actor.Draw.Frame;
		mirror = quadrant < 2;
	}
	else if (actor.State == 10)
	{
		image = images.Sleep[quadrant];
		if (actor.Kind == Kinds::Farmer)
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
		mirror = actor.Kind == Kinds::Dog ? (quadrant == 0 || quadrant == 2) : quadrant < 2;
	}
	else if (actor.State == 11 && actor.Kind == Kinds::Farmer)
	{
		image = images.Wake[quadrant];
		frame = actor.Draw.Frame;
		x -= 50;
		y -= 20;
		mirror = quadrant >= 2;
	}
	else if (actor.State == 13 && actor.Kind == Kinds::Farmer)
	{
		image = images.Sleep[quadrant];
		frame = actor.Draw.Frame;
		x -= 50;
		y -= 20;
		mirror = quadrant < 2;
	}
	else if (actor.Kind == Kinds::Dog && actor.State == 12)
	{
		image = images.Sleep[quadrant];
		frame = actor.Draw.Frame;
		mirror = quadrant == 0 || quadrant == 2;
	}
	else if (actor.State == 12 || actor.State == 13)
	{
		if (images.Distraction[quadrant])
		{
			image = images.Distraction[quadrant];
			frame = actor.Draw.Frame;
			mirror = actor.Kind == Kinds::Girl ? (quadrant == 0 || quadrant == 1) : quadrant >= 2;
		}
	}
	else if (actor.State == 17 && images.Beam[quadrant])
	{
		image = images.Beam[quadrant];
		frame = std::min(image->Rows - 1, static_cast<i32>(actor.Age * (actor.Kind == Kinds::Dog || actor.Kind == Kinds::Girl ? 4 : 15)));
		mirror = actor.Kind == Kinds::Girl ? quadrant < 2 : quadrant >= 2;
	}
	else if (actor.State == 20 || actor.State == 21)
	{
		if (images.Battle[quadrant])
		{
			image = images.Battle[quadrant];
			frame = actor.Draw.Frame;
			mirror = actor.Kind == Kinds::Girl ? quadrant < 2 : quadrant >= 2;
			if (actor.Kind == Kinds::Dog || actor.Kind == Kinds::Farmer)
			{
				x -= 30;
				y -= 20;
				mirror = false;
			}
			if (actor.Kind == Kinds::Girl && actor.State == 21)
				x -= 20;
		}
	}
	if ((actor.Kind == Kinds::Girl && actor.State == 18) || (actor.Kind == Kinds::Dog && actor.State == 13))
	{
		actor.Draw = {};
		return;
	}
	if (image)
		actor.Draw = {image, x, y, std::clamp(frame, 0, image->Rows * image->Columns - 1), mirror};
}

void EnemyCharacter::EnemyArrived()
{
	const size_t index = GetIndex();
	WorldObject& actor = GetState();
	const i32 code = actor.Tile >= 0 ? Scene.Grid.ItemCodes[actor.Tile] : 0;
	const i32 distraction = actor.Kind == Kinds::Dog ? 129 : actor.Kind == Kinds::Farmer ? 130
	                                                     : actor.Kind == Kinds::Girl     ? 131
	                                                                                     : 0;
	if (code == 128 || (distraction && code == distraction))
	{
		Scene.Items.RemoveItem(actor.Tile);
		actor.Age = 0;
		actor.Hunting = false;
		actor.ItemTarget = -1;
		if (code == 128)
		{
			actor.State = 16;
			Scene.UfoController.RequestUfo(static_cast<i32>(index));
		}
		else if (actor.Kind == Kinds::Dog)
		{
			actor.State = 12;
			actor.Pulse = Scene.Parameter("DogDiggingTime", 15);
			actor.Idle = 0;
			actor.Draw.Frame = 0;
			Scene.Effects.SpawnDigEffect(actor);
			Scene.EmitNamedSound("SfxDogDig");
		}
		else if (actor.Kind == Kinds::Farmer)
		{
			actor.State = 12;
			actor.AnimRate = 10;
			Scene.EmitNamedSound("SfxLarsDrink");
		}
		else
		{
			actor.State = (actor.Facing >= 2 && actor.Facing <= 5) ? 13 : 12;
			actor.Facing = 2 * (actor.Facing / 2) + 1;
			actor.AnimRate = 9;
			Scene.EmitNamedSound("SfxBroemsePlaysTeddy");
		}
		return;
	}
	// 0x405C50 selects the last dog in facing, -1, +1 order and stuns it.
	if (actor.Kind == Kinds::Girl)
	{
		i32 dog = -1;
		for (i32 offset : {0, -1, 1})
		{
			const i32* d = Directions[(actor.Facing + offset + 8) % 8];
			const i32 tile = actor.Tile + d[0] + Stride * d[1];
			for (size_t j = 0; j < Scene.Objects.Size(); ++j)
				if (!Scene.Objects[j].Freed && Scene.Objects[j].Kind == Kinds::Dog && Scene.Objects[j].Tile == tile)
					dog = static_cast<i32>(j);
		}
		if (dog >= 0)
		{
			WorldObject& target = Scene.Objects[dog];
			if (target.State != 10 && target.State != 12 && target.State != 13 && target.State != 16 && target.State != 17 && target.State != 20)
			{
				for (i32& reservation : Scene.Grid.Reservations)
					if (reservation == dog)
						reservation = -1;
				target.X = target.TargetX = static_cast<f32>((target.Tile % Stride - 1) * 40 + 20) - 60;
				target.Y = target.TargetY = static_cast<f32>((target.Tile / Stride - 1) * 40 + 20) - 60;
				target.State = 13;
				target.Pulse = 6;
				actor.State = actor.Facing >= 2 && actor.Facing <= 5 ? 23 : 22;
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
	const bool activity = Scene.Player.PlayerAction == PlayerActions::Interaction || Scene.Player.PlayerAction == PlayerActions::Frigida;
	// Lars sometimes gives up with a directional grumble once Sven stops.
	if (actor.Kind == Kinds::Farmer && actor.Hunting && actor.ItemTarget < 0 && !activity && Scene.RandomInt(2) == 0)
	{
		const i32 tile = Scene.Grid.TileAt(Scene.Player.X, Scene.Player.Y);
		if (tile >= 0)
			actor.Facing = EnemyDirection(actor.Tile, tile);
		actor.Steps = Scene.Resources.EnemyRules[5].WalkSteps + Scene.RandomInt(Scene.Resources.EnemyRules[5].WalkRandom);
		actor.State = actor.Facing >= 2 && actor.Facing <= 5 ? 15 : 14;
		actor.Hunting = false;
		actor.Age = 0;
		actor.AnimRate = 10;
		Scene.EmitNamedSound("SfxLarsGroan");
		return;
	}
	if (actor.ItemTarget < 0 && distraction)
	{
		actor.ItemTarget = Scene.Items.FindDistraction(actor, distraction);
		if (actor.ItemTarget >= 0)
		{
			actor.Hunting = true;
			actor.Steps = Scene.Resources.EnemyRules[actor.Kind].HuntSteps;
			EnemyNotice(actor, true);
		}
	}
	if (!actor.Hunting && ((actor.Kind == Kinds::Farmer && activity) ||
	                       (actor.Kind == Kinds::Dog && (activity || Scene.Objects.PlayerInRange(actor, Scene.Resources.EnemyRules[4].Range))) ||
	                       ((actor.Kind == Kinds::Alien || actor.Kind == Kinds::Girl) && Scene.Objects.PlayerInRange(actor, Scene.Resources.EnemyRules[actor.Kind].Range))))
	{
		actor.Hunting = true;
		actor.Steps = Scene.Resources.EnemyRules[actor.Kind].HuntSteps;
		EnemyNotice(actor, false);
	}
	if (--actor.Steps == 0 || !ChooseEnemyStep())
	{
		// 0x4022F0/0x4038F0/0x4063C0 draw the next walk budget before idle time.
		if (actor.Kind != Kinds::Alien)
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
	if (actor.State == 10)
	{
		actor.Age += seconds * (actor.Kind == Kinds::Farmer ? actor.AnimRate : 10);
		if (actor.Kind == Kinds::Farmer)
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
			if (actor.Kind == Kinds::Farmer)
			{
				actor.State = 11;
				actor.AnimRate = 10;
				actor.Facing = std::min(7, actor.Facing + 1);
			}
			else
				EnemyIdle(actor);
		}
	}
	else if (actor.State == 11 && actor.Kind == Kinds::Farmer)
	{
		if (Scene.AdvanceActor(actor, AnimationSequence::FarmerStandUp, actor.AnimRate, seconds))
			EnemyIdle(actor);
	}
	else if (actor.State == 0)
	{
		if (actor.Kind == Kinds::Alien)
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
		const i32 code = actor.Kind == Kinds::Dog ? 129 : actor.Kind == Kinds::Farmer ? 130
		                                              : actor.Kind == Kinds::Girl     ? 131
		                                                                              : 0;
		const bool activity = Scene.Player.PlayerAction == PlayerActions::Interaction || Scene.Player.PlayerAction == PlayerActions::Frigida;
		if (!actor.Retry)
		{
			if (code)
				actor.ItemTarget = Scene.Items.FindDistraction(actor, code);
			bool detect = actor.Kind == Kinds::Farmer ? activity : Scene.Objects.PlayerInRange(actor, rules.Range);
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
				actor.State = 1;
				actor.Age = 0;
			}
			else
			{
				// Failed idle chooser keeps pursuit/item link; byte65 gates notice.
				actor.Retry = true;
				actor.Pulse = rules.Idle + (actor.Kind == Kinds::Farmer || actor.Kind == Kinds::Girl ? rules.IdleRandom : Scene.RandomInt(static_cast<i32>(rules.IdleRandom)));
			}
		}
	}
	else if (actor.State == 1)
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
		const size_t walkLength = actor.Kind == Kinds::Girl ? ActorFrameTables.at(AnimationSequence::GirlWalk).size() : static_cast<size_t>(Scene.Resources.ActorSprites[actor.Kind].Walk[actor.Facing]->Rows);
		if (actor.Age >= walkLength)
			actor.Age = 0;
		if (fraction == 1)
		{
			const i32* d = Directions[actor.Facing];
			i32 target = actor.Tile + d[0] + Stride * d[1];
			if (actor.Tile >= 0 && Scene.Grid.Tiles[actor.Tile] == Scene.OccupancyKind(actor.Kind))
				Scene.Grid.Tiles[actor.Tile] = Empty;
			actor.Tile = target;
			actor.CommittedY = actor.Y;
			Scene.Grid.Tiles[target] = Scene.OccupancyKind(actor.Kind);
			Scene.Grid.Reservations[target] = -1;
			EnemyArrived();
		}
	}
	else if (actor.Kind == Kinds::Farmer && (actor.State == 14 || actor.State == 15))
	{
		if (Scene.AdvanceActor(actor, actor.State == 14 ? AnimationSequence::FarmerGrumbleBack : AnimationSequence::FarmerGrumbleFront, actor.AnimRate, seconds))
			EnemyIdle(actor);
	}
	else if (actor.Kind == Kinds::Girl && (actor.State == 22 || actor.State == 23))
	{
		if (Scene.AdvanceActor(actor, actor.State == 22 ? AnimationSequence::GirlDogBack : AnimationSequence::GirlDogFront, actor.AnimRate, seconds))
			EnemyIdle(actor, true);
	}
	else if (actor.State == 12 || actor.State == 13)
	{
		bool done = false;
		if (actor.Kind == Kinds::Dog)
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
		else if (actor.Kind == Kinds::Farmer)
			done = Scene.AdvanceActor(actor, actor.State == 12 ? AnimationSequence::FarmerDrink : AnimationSequence::FarmerLieDown, actor.AnimRate, seconds);
		else if (actor.Kind == Kinds::Girl)
			done = Scene.AdvanceActor(actor, actor.State == 12 ? AnimationSequence::GirlTeddyBack : AnimationSequence::GirlTeddyFront, actor.AnimRate, seconds);
		if (done)
		{
			if (actor.Kind == Kinds::Farmer && actor.State == 12)
			{
				actor.State = 13;
				actor.Age = 0;
				actor.AnimRate = 10;
			}
			else if (actor.Kind == Kinds::Farmer)
			{
				actor.State = 10;
				actor.Pulse = rules.Sleep;
				actor.Facing = 2 * (actor.Facing / 2);
				actor.Age = 0;
				actor.SoundClock = 0;
				Scene.EmitNamedSound("SfxLarsFall");
			}
			else
				EnemyIdle(actor, actor.Kind == Kinds::Girl && actor.State == 13);
		}
	}
	else if (actor.State == 16)
	{
		Scene.UfoController.RequestUfo(static_cast<i32>(i));
	}
	else if (actor.State == 17)
	{
		actor.Age += seconds;
		if (actor.Kind == Kinds::Girl)
		{
			actor.Lift -= 20 * seconds;
			if (actor.Lift < -27)
			{
				actor.Lift = 0;
				actor.State = 18;
				actor.Pulse = 4.5f;
				actor.Age = 0;
				actor.Retry = true;
				actor.Hunting = false;
				Scene.EmitNamedSound("SfxBroemsePlayAliens");
			}
		}
		else if (actor.Kind == Kinds::Farmer)
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
	else if (actor.Kind == Kinds::Girl && actor.State == 18)
	{
		actor.Pulse -= seconds;
		if (actor.Pulse < 0)
		{
			actor.State = 19;
			actor.Lift = -27;
			actor.Age = 0;
			actor.Idle = 0;
			actor.Pulse = rules.Idle + rules.IdleRandom;
			actor.Retry = true;
			actor.Hunting = false;
		}
	}
	else if (actor.Kind == Kinds::Girl && actor.State == 19)
	{
		actor.Lift += 20 * seconds;
		if (actor.Lift > 0)
		{
			actor.Lift = 0;
			EnemyIdle(actor, true);
			Scene.EmitNamedSound("SfxSvenFall");
		}
	}
	else if (actor.State == 20 || actor.State == 21)
	{
		bool done = false;
		if (actor.Kind == Kinds::Alien)
			done = Scene.AdvanceActor(actor, AnimationSequence::AlienProbe, 13, seconds);
		else if (actor.Kind == Kinds::Girl)
			done = Scene.AdvanceActor(actor, actor.State == 20 ? AnimationSequence::GirlSvenBack : AnimationSequence::GirlSvenFront, actor.AnimRate, seconds);
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
	Scene.Objects.Combat.Enemy = static_cast<i32>(index);
	Scene.Objects.Combat.Remaining = Scene.Resources.EnemyRules[actor.Kind].FightTime;
	actor.State = 20;
	actor.Pulse = Scene.Objects.Combat.Remaining;
	actor.Age = 0;
	actor.Hunting = false;
	actor.Draw.Frame = 0;
	if (actor.Kind == Kinds::Girl)
		actor.AnimRate = 20;
	Scene.Player.PlayerAction = PlayerActions::Idle;
	Scene.Player.ActiveSheep = -1;
	Scene.Player.Interaction = nullptr;
	Scene.Player.ActionQueued = false;
	Scene.Player.CancelPlayerStep();
	Scene.Items.PlayerEffect = 0;
	Scene.Items.EffectRemaining = 0;
	cstr sound = actor.Kind == Kinds::Alien ? "alien_analprobe_mix2.ogg" : actor.Kind == Kinds::Dog  ? "fightdog.ogg"
	                                                                   : actor.Kind == Kinds::Farmer ? "fightguy.ogg"
	                                                                                                 : "broemse_plays_sven.ogg";
	Scene.Sounds.push_back(str("Sven2/sfx/") + sound);
	if (actor.Kind == Kinds::Girl)
	{
		actor.State = (actor.Facing >= 2 && actor.Facing <= 5) ? 21 : 20;
		Scene.Effects.AddTime(-20, actor.X + actor.CenterX, actor.Y + actor.CenterY);
	}
}

void EnemyCharacter::FinishBattle(Object& actor)
{
	if (actor.Kind != Kinds::Girl)
		Scene.RecordLifeLoss();
	Scene.Objects.Combat.Enemy = -1;
	EnemyIdle(actor, true);
	Scene.Player.RestorePlayer();
}
