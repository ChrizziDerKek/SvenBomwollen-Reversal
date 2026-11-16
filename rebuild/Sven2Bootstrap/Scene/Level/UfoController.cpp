#include "Scene/Level/UfoController.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

UfoController::UfoController(LevelScene& scene)
    : Scene(scene)
{
}

void UfoController::RequestUfo(i32 target)
{
	if (Ufo.State >= 0 || target < 0 || static_cast<size_t>(target) >= Scene.Objects.Size() || Scene.Objects[target].Freed)
		return;
	const WorldObject& actor = Scene.Objects[target];
	Ufo = UfoFlight{};
	Ufo.State = 0;
	Ufo.Target = target;
	Ufo.TargetTile = actor.Tile;
	if (static_cast<size_t>(target) == Scene.PlayerIndex)
	{
		Ufo.X = Scene.Player.X + 60;
		Ufo.Y = Scene.Player.Y + 60;
		Ufo.TargetTile = Scene.Grid.TileAt(Scene.Player.X, Scene.Player.Y);
	}
	else
	{
		Ufo.X = actor.X + (actor.Kind == Kinds::Farmer ? 80 : 60);
		Ufo.Y = actor.Y + (actor.Kind == Kinds::Farmer ? 120 : actor.Kind == Kinds::Girl ? 80
		                                                                                 : 60);
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
	if (actor.Freed || (static_cast<size_t>(target) != Scene.PlayerIndex && actor.Tile != Ufo.TargetTile) ||
	    (Scene.IsAnimal(actor.Kind) && (Scene.Player.ActiveSheep == target || actor.State == 4 || actor.State == 3 ||
	                                    (actor.Kind == Kinds::Sheep && actor.State >= 11 && actor.State <= 14))))
	{
		Ufo.Clock = 0;
		return;
	}
	if (static_cast<size_t>(target) == Scene.PlayerIndex)
	{
		if (Scene.Player.PlayerAction || Scene.Player.PlayerShockAge >= 0 || Scene.Objects.Combat.Enemy >= 0 || Scene.Grid.TileAt(Scene.Player.X, Scene.Player.Y) != Ufo.TargetTile)
		{
			Ufo.Clock = 0;
			return;
		}
		Scene.Player.PlayerBeamState = 21;
		Scene.Player.PlayerBeamClock = 0;
		Scene.Player.PlayerLift = 0;
		Scene.Items.PlayerEffect = 0;
		Scene.Player.CancelPlayerStep();
	}
	else
	{
		actor.State = 17;
		actor.Lift = 0;
		actor.Age = 0;
		actor.Pulse = 175;
		if (Scene.IsAnimal(actor.Kind))
		{
			Ufo.CollectAnimals = true;
			Scene.Sounds.push_back("Sven2/sfx/Sheep_beamup.ogg");
		}
		else if (actor.Kind == Kinds::Alien)
			Scene.AddScore(75);
		else if (actor.Kind == Kinds::Dog)
		{
			Scene.AddScore(50);
			Scene.Sounds.push_back("Sven2/sfx/wotan_beamup.ogg");
		}
		else if (actor.Kind == Kinds::Girl)
		{
			Scene.AddScore(25);
			Ufo.Clock = 400.0f / 60;
			Scene.Sounds.push_back("Sven2/sfx/Broemse_beamup.ogg");
		}
		else if (actor.Kind == Kinds::Farmer)
			Scene.Sounds.push_back("Sven2/sfx/Lars_angry.ogg");
	}
	Ufo.Beam = true;
	Ufo.BeamAge = 0;
	Scene.Sounds.push_back("Sven2/sfx/beam.ogg");
}

void UfoController::UpdateUfo(f32 seconds)
{
	if (Scene.Player.PlayerBeamState)
	{
		Scene.Player.PlayerBeamClock += seconds;
		if (Scene.Player.PlayerBeamState == 21)
		{
			Scene.Player.PlayerLift -= seconds * 20;
			if (Scene.Player.PlayerLift < -27)
			{
				Scene.Player.PlayerLift = -27;
				Scene.Player.PlayerBeamState = 22;
				Scene.Player.PlayerBeamClock = 0;
			}
		}
		else if (Scene.Player.PlayerBeamClock > 2.5f)
		{
			Scene.Player.PlayerLift += seconds * 60;
			if (Scene.Player.PlayerLift >= 0)
			{
				Scene.Player.PlayerLift = 0;
				Scene.Player.PlayerBeamState = 0;
				Scene.Items.PlayerEffect = 137;
				Scene.Items.EffectRemaining = Scene.Parameter("AlienDuration", 15);
				Scene.Items.EffectAge = 0;
				Scene.Sounds.push_back("Sven2/sfx/sven_hits_ground.ogg");
			}
		}
	}
	if (Ufo.State < 0)
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
	if (Ufo.State == 0)
	{
		Ufo.AngleX -= seconds * Ufo.AngularSpeed;
		Ufo.AngleY -= seconds * Ufo.AngularSpeed;
		Ufo.AngularSpeed = std::max(0.0f, Ufo.AngularSpeed - seconds * 0.0001f);
		Ufo.Radius -= seconds * 125;
		if (Ufo.Radius < 0)
		{
			Ufo.Radius = 0;
			Ufo.State = 1;
			BeginBeam();
		}
	}
	else if (Ufo.State == 1)
	{
		Ufo.Clock -= seconds;
		if (Ufo.Clock < 0)
		{
			Ufo.AngularSpeed = 0.005f;
			Ufo.State = Ufo.CollectAnimals ? 3 : 2;
		}
	}
	else if (Ufo.State == 2)
	{
		Ufo.AngleX += seconds * Ufo.AngularSpeed;
		Ufo.AngleY += seconds * Ufo.AngularSpeed;
		Ufo.AngularSpeed += seconds * 0.0002f;
		Ufo.Radius += seconds * 175;
		if (Ufo.Radius > 1000)
			Ufo.State = -1;
	}
	else if (Ufo.State == 3)
	{
		std::vector<i32> candidates;
		for (size_t i = 0; i < Scene.Objects.Size(); ++i)
			if (Scene.IsAnimal(Scene.Objects[i].Kind) && !Scene.Objects[i].Freed)
				candidates.push_back(static_cast<i32>(i));
		if (candidates.empty())
		{
			Ufo.State = 2;
			return;
		}
		Ufo.Target = candidates[Scene.RandomInt(static_cast<i32>(candidates.size()))];
		const WorldObject& actor = Scene.Objects[Ufo.Target];
		Ufo.TargetTile = actor.Tile;
		Ufo.TargetX = actor.X + 60;
		Ufo.TargetY = actor.Y + 60;
		Ufo.VX = (Ufo.TargetX - Ufo.X) * 0.5f;
		Ufo.VY = (Ufo.TargetY - Ufo.Y) * 0.5f;
		Ufo.State = 4;
	}
	else if (Ufo.State == 4)
	{
		auto advance = [seconds](f32 current, f32 target, f32 velocity)
		{
			const f32 next = current + velocity * seconds;
			return velocity >= 0 ? std::min(next, target) : std::max(next, target);
		};
		Ufo.X = advance(Ufo.X, Ufo.TargetX, Ufo.VX);
		Ufo.Y = advance(Ufo.Y, Ufo.TargetY, Ufo.VY);
		if (Ufo.X == Ufo.TargetX && Ufo.Y == Ufo.TargetY)
		{
			Ufo.State = 1;
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
