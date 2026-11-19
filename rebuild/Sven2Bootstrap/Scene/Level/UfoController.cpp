#include "Scene/Level/UfoController.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

UfoController::UfoController(LevelScene& scene)
    : Scene(scene)
{}

void UfoController::RequestUfo(i32 target)
{
	if (Ufo.State != UfoState::Inactive || target < 0 || static_cast<size_t>(target) >= Scene.Objects.Size() || Scene.Objects[target].Freed)
		return;
	const WorldObject& actor = Scene.Objects[target];
	Ufo = UfoFlight{};
	Ufo.State = UfoState::Arriving;
	Ufo.Target = target;
	Ufo.TargetTile = actor.Tile;
	if (static_cast<size_t>(target) == Scene.GetPlayerIndex())
	{
		Ufo.X = Scene.GetPlayer().X + 60;
		Ufo.Y = Scene.GetPlayer().Y + 60;
		Ufo.TargetTile = Scene.Grid.TileAt(Scene.GetPlayer().X, Scene.GetPlayer().Y);
	}
	else
	{
		Ufo.X = actor.X + (actor.Kind == ObjectKind::Farmer ? 80 : 60);
		Ufo.Y = actor.Y + (actor.Kind == ObjectKind::Farmer ? 120 : (actor.Kind == ObjectKind::Girl ? 80 : 60));
	}
	Scene.Sounds.emplace_back(Scene.Engine.GetData().SoundPath("SfxUFO"), SoundPlayback::Loop);
}

void UfoController::BeginBeam()
{
	Ufo.Clock = 250.0f / 60;
	Ufo.Beam = false;
	Ufo.CollectAnimals = false;
	const i32 target = Ufo.Target;
	if (target < 0 || static_cast<size_t>(target) >= Scene.Objects.Size())
	{
		Ufo.Clock = 0;
		return;
	}
	WorldObject& actor = Scene.Objects[target];
	if (actor.Freed || (static_cast<size_t>(target) != Scene.GetPlayerIndex() && actor.Tile != Ufo.TargetTile) || (Scene.IsAnimal(actor.Kind) && (Scene.GetPlayer().ActiveSheep == target || actor.State == ActorState::AnimalWalking || actor.State == ActorState::Turning || (actor.Kind == ObjectKind::Sheep && IsDemonState(actor.State)))))
	{
		Ufo.Clock = 0;
		return;
	}
	if (static_cast<size_t>(target) == Scene.GetPlayerIndex())
	{
		if (Scene.GetPlayer().PlayerAction != PlayerActionState::Idle || Scene.GetPlayer().PlayerShockAge >= 0 || Scene.GetCombat().Enemy >= 0 || Scene.Grid.TileAt(Scene.GetPlayer().X, Scene.GetPlayer().Y) != Ufo.TargetTile)
		{
			Ufo.Clock = 0;
			return;
		}
		Scene.GetPlayer().PlayerBeamState = 21;
		Scene.GetPlayer().PlayerBeamClock = 0;
		Scene.GetPlayer().PlayerLift = 0;
		Scene.Items.GetPowerup().Code = ItemCode::None;
		Scene.GetPlayer().CancelPlayerStep();
	}
	else
	{
		actor.State = ActorState::BeamingUp;
		actor.Lift = 0;
		actor.Age = 0;
		actor.Pulse = 175;
		if (Scene.IsAnimal(actor.Kind))
		{
			Ufo.CollectAnimals = true;
			Scene.Sounds.push_back("Sven2/sfx/Sheep_beamup.ogg");
		}
		else if (actor.Kind == ObjectKind::Alien)
			Scene.AddScore(75);
		else if (actor.Kind == ObjectKind::Dog)
		{
			Scene.AddScore(50);
			Scene.Sounds.push_back("Sven2/sfx/wotan_beamup.ogg");
		}
		else if (actor.Kind == ObjectKind::Girl)
		{
			Scene.AddScore(25);
			Ufo.Clock = 400.0f / 60;
			Scene.Sounds.push_back("Sven2/sfx/Broemse_beamup.ogg");
		}
		else if (actor.Kind == ObjectKind::Farmer)
			Scene.Sounds.push_back("Sven2/sfx/Lars_angry.ogg");
	}
	Ufo.Beam = true;
	Ufo.BeamAge = 0;
	Scene.Sounds.push_back("Sven2/sfx/beam.ogg");
}

void UfoController::UpdateUfo(f32 seconds)
{
	if (Scene.GetPlayer().PlayerBeamState)
	{
		Scene.GetPlayer().PlayerBeamClock += seconds;
		if (Scene.GetPlayer().PlayerBeamState == 21)
		{
			Scene.GetPlayer().PlayerLift -= seconds * 20;
			if (Scene.GetPlayer().PlayerLift < -27)
			{
				Scene.GetPlayer().PlayerLift = -27;
				Scene.GetPlayer().PlayerBeamState = 22;
				Scene.GetPlayer().PlayerBeamClock = 0;
			}
		}
		else if (Scene.GetPlayer().PlayerBeamClock > 2.5f)
		{
			Scene.GetPlayer().PlayerLift += seconds * 60;
			if (Scene.GetPlayer().PlayerLift >= 0)
			{
				Scene.GetPlayer().PlayerLift = 0;
				Scene.GetPlayer().PlayerBeamState = 0;
				Scene.Items.GetPowerup().Code = ItemCode::AlienPower;
				Scene.Items.GetPowerup().Remaining = Scene.Parameter("AlienDuration", 15);
				Scene.Items.GetPowerup().Age = 0;
				Scene.Sounds.push_back("Sven2/sfx/sven_hits_ground.ogg");
			}
		}
	}
	if (Ufo.State == UfoState::Inactive)
	{
		if (Scene.Expired)
		{
			std::vector<i32> candidates;
			for (size_t i = 0; i < Scene.Objects.Size(); ++i)
				if (Scene.IsAnimal(Scene.Objects[i].Kind) && !Scene.Objects[i].Freed)
					candidates.push_back(static_cast<i32>(i));
			if (!candidates.empty())
				RequestUfo(candidates[Scene.RandomInt(static_cast<i32>(candidates.size()))]);
		}
		return;
	}
	Ufo.Age += seconds * 20;
	if (Ufo.Beam)
	{
		Ufo.BeamAge += seconds * 15;
		if (static_cast<size_t>(Ufo.BeamAge) >= ActorFrameTables.at(AnimationSequence::UfoBeam).size())
			Ufo.Beam = false;
	}
	if (Ufo.State == UfoState::Arriving)
	{
		Ufo.AngleX -= seconds * Ufo.AngularSpeed;
		Ufo.AngleY -= seconds * Ufo.AngularSpeed;
		Ufo.AngularSpeed = std::max(0.0f, Ufo.AngularSpeed - seconds * 0.0001f);
		Ufo.Radius -= seconds * 125;
		if (Ufo.Radius < 0)
		{
			Ufo.Radius = 0;
			Ufo.State = UfoState::Beaming;
			BeginBeam();
		}
	}
	else if (Ufo.State == UfoState::Beaming)
	{
		Ufo.Clock -= seconds;
		if (Ufo.Clock < 0)
		{
			Ufo.AngularSpeed = 0.005f;
			Ufo.State = Ufo.CollectAnimals ? UfoState::SelectingAnimal : UfoState::Departing;
		}
	}
	else if (Ufo.State == UfoState::Departing)
	{
		Ufo.AngleX += seconds * Ufo.AngularSpeed;
		Ufo.AngleY += seconds * Ufo.AngularSpeed;
		Ufo.AngularSpeed += seconds * 0.0002f;
		Ufo.Radius += seconds * 175;
		if (Ufo.Radius > 1000)
			Ufo.State = UfoState::Inactive;
	}
	else if (Ufo.State == UfoState::SelectingAnimal)
	{
		std::vector<i32> candidates;
		for (size_t i = 0; i < Scene.Objects.Size(); ++i)
			if (Scene.IsAnimal(Scene.Objects[i].Kind) && !Scene.Objects[i].Freed)
				candidates.push_back(static_cast<i32>(i));
		if (candidates.empty())
		{
			Ufo.State = UfoState::Departing;
			return;
		}
		Ufo.Target = candidates[Scene.RandomInt(static_cast<i32>(candidates.size()))];
		const WorldObject& actor = Scene.Objects[Ufo.Target];
		Ufo.TargetTile = actor.Tile;
		Ufo.TargetX = actor.X + 60;
		Ufo.TargetY = actor.Y + 60;
		Ufo.VX = (Ufo.TargetX - Ufo.X) * 0.5f;
		Ufo.VY = (Ufo.TargetY - Ufo.Y) * 0.5f;
		Ufo.State = UfoState::MovingToAnimal;
	}
	else if (Ufo.State == UfoState::MovingToAnimal)
	{
		auto advance = [seconds](f32 current, f32 target, f32 velocity) {
			const f32 next = current + velocity * seconds;
			return velocity >= 0 ? std::min(next, target) : std::max(next, target);
		};
		Ufo.X = advance(Ufo.X, Ufo.TargetX, Ufo.VX);
		Ufo.Y = advance(Ufo.Y, Ufo.TargetY, Ufo.VY);
		if (Ufo.X == Ufo.TargetX && Ufo.Y == Ufo.TargetY)
		{
			Ufo.State = UfoState::Beaming;
			if (Scene.Expired)
				BeginBeam();
			else
			{
				Ufo.CollectAnimals = false;
				Ufo.Clock = 0;
			}
		}
	}
}
