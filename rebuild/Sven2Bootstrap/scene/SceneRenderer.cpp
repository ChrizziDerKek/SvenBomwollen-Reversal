#include "Scene/Objects/ObjectCatalog.h"
#include "Scene/SceneRenderer.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

SceneRenderer::SceneRenderer(LevelScene& scene)
    : Scene(scene), OverlayFont(scene.Engine, true)
{}

const std::vector<SpriteDraw>& SceneRenderer::Draws()
{
	Sorted = Scene.Objects.CopyStates();
	Sorted.insert(Sorted.end(), Scene.Effects.SceneEffects.begin(), Scene.Effects.SceneEffects.end());
	// Original 0x40D72C sorts rendered Y + height - adjustment. Player grid
	// ownership stays at the committed cell while the sprite moves between cells.
	auto depth = [&](const Object& object)
	{
		// These animations are placed at the animal's canvas, although the
		// command is stored on Sven. Keep that compound sprite at its visible
		// owner's depth instead of promoting it to Sven's approach position.
		if (object.Kind == ObjectKind::Player && Scene.Player.PlayerAction == PlayerActionState::Interaction && Scene.Player.ActiveSheep >= 0)
		{
			const WorldObject& animal = Scene.Objects[Scene.Player.ActiveSheep];
			if (Scene.IsAnimal(animal.Kind) && animal.Kind != ObjectKind::Sheep)
				return static_cast<f32>(animal.Y + animal.DepthOffset);
		}
		const f32 y = object.Kind == ObjectKind::Player ? Scene.Player.Y : object.Y;
		return static_cast<f32>(y + object.DepthOffset);
	};
	// Start each frame in file slot order to preserve the original equal-depth behavior.
	for (size_t i = 0; i < Sorted.size(); ++i)
		for (size_t j = i + 1; j < Sorted.size(); ++j)
			if (depth(Sorted[i]) > depth(Sorted[j]))
				std::swap(Sorted[i], Sorted[j]);
	DrawCommands.clear();
	DrawCommands.push_back(Background);
	auto indicator = [&](const Object& animal)
	{
		static constexpr i32 offsetX[4][4] = { { 40, 40, 20, 20 }, { 40, 24, 30, 16 }, { 50, 50, 5, 5 }, { 50, 50, 5, 5 } };
		static constexpr i32 offsetY[4][4] = { { 0, 0, 0, 0 }, { -1, -1, -1, -1 }, { -10, 10, 10, -10 }, { -10, 20, 20, -10 } };
		const size_t family = AnimalIndicatorIndex(animal.Kind);
		DrawCommands.push_back({ animal.Kind == ObjectKind::Sheep && animal.Temperament == 2 ? Scene.Resources.KettleSprite : Scene.Resources.MoodSprite, static_cast<i32>(animal.X) + offsetX[family][animal.Facing], static_cast<i32>(animal.Y + animal.Lift) + offsetY[family][animal.Facing], std::clamp(animal.Impatience + animal.MoodBlink, 0, 11) });
	};
	for (const WorldObject& object : Sorted)
	{
		if (object.Freed)
			continue;
		const bool interacting = Scene.IsAnimal(object.Kind) && Scene.Player.ActiveSheep >= 0 && (Scene.Player.PlayerAction == PlayerActionState::Interaction || Scene.Player.PlayerAction == PlayerActionState::Kicked || Scene.Player.PlayerAction == PlayerActionState::Frigida) && object.Tile == Scene.Objects[Scene.Player.ActiveSheep].Tile;
		if (object.Kind == ObjectKind::Player && Scene.Objects.Combat.Enemy >= 0)
			continue;
		if (!interacting && object.Draw.Sprite)
			DrawCommands.push_back(object.Draw);
		if ((object.Kind == ObjectKind::Dog || object.Kind == ObjectKind::Farmer) && object.State == ActorState::Sleeping)
		{
			static constexpr i32 sleepX[2][4] = { { 50, 50, 0, 0 }, { 90, 100, -10, 0 } };
			static constexpr i32 sleepY[2][4] = { { -10, 10, 10, -10 }, { 10, 30, 30, 10 } };
			const size_t family = object.Kind == ObjectKind::Dog ? 0 : 1;
			const i32 quadrant = object.Facing / 2;
			// Sleep's secondary counter advances 0..12, then loops 7..12.
			const i32 age = static_cast<i32>(object.Age);
			const i32 frame = age < 13 ? age : 7 + (age - 13) % 6;
			DrawCommands.push_back({ Scene.Resources.SleepSymbols, static_cast<i32>(object.X) + sleepX[family][quadrant], static_cast<i32>(object.Y) + sleepY[family][quadrant], frame });
		}
		if (Scene.IsAnimal(object.Kind) && !interacting && object.State != ActorState::BeamingUp && !(object.Kind == ObjectKind::Sheep && IsDemonState(object.State)))
			indicator(object);
		// Original player render states 1/2 draw the indicator after the combined
		// interaction sprite (0x40D977-0x40D9E8), so it cannot be painted over.
		if (object.Kind == ObjectKind::Player && Scene.Player.ActiveSheep >= 0 && (Scene.Player.PlayerAction == PlayerActionState::Interaction || Scene.Player.PlayerAction == PlayerActionState::Kicked || Scene.Player.PlayerAction == PlayerActionState::Frigida) && Scene.IsAnimal(Scene.Objects[Scene.Player.ActiveSheep].Kind))
			indicator(Scene.Objects[Scene.Player.ActiveSheep]);
	}
	const i32 frame = static_cast<i32>(Scene.Effects.SplashAge * SplashFrameRate);
	if (frame < SplashFrames)
	{
		for (SpriteDraw splash : Scene.Effects.Splashes)
		{
			if (!splash.Sprite)
				continue;
			splash.Frame = frame;
			DrawCommands.push_back(splash);
		}
	}
	if (Scene.Items.PlayerEffect != ItemCode::None)
	{
		const i32 effect = Scene.Items.PlayerEffect == ItemCode::Rocket ? 0 : (Scene.Items.PlayerEffect == ItemCode::SlowMushroom ? 1 : (Scene.Items.PlayerEffect == ItemCode::StinkMushroom ? 2 : (Scene.Items.PlayerEffect == ItemCode::Flower ? 3 : 4)));
		const ::Sprite* image = Scene.Resources.EffectSprites[effect];
		DrawCommands.push_back({ image, static_cast<i32>(Scene.Player.X) + 28, static_cast<i32>(Scene.Player.Y + Scene.Player.PlayerLift) - 4, static_cast<i32>(Scene.Items.EffectAge * 15) % image->Rows });
	}
	if (Scene.UfoController.Ufo.State != UfoState::Inactive)
	{
		const f32 orbitX = std::sin(Scene.UfoController.Ufo.AngleX * 57.295776f) * Scene.UfoController.Ufo.Radius + Scene.UfoController.Ufo.X;
		const f32 orbitY = std::cos(Scene.UfoController.Ufo.AngleY * 57.295776f) * Scene.UfoController.Ufo.Radius + Scene.UfoController.Ufo.Y;
		const i32 ufoFrame = static_cast<i32>(Scene.UfoController.Ufo.Age) % 13;
		if (Scene.UfoController.Ufo.Beam)
		{
			const std::vector<i32>& frames = ActorFrameTables.at(AnimationSequence::UfoBeam);
			const i32 step = std::min(static_cast<i32>(frames.size()) - 1, static_cast<i32>(Scene.UfoController.Ufo.BeamAge));
			DrawCommands.push_back({ Scene.Resources.BeamSprite, static_cast<i32>(Scene.UfoController.Ufo.X) - 150, static_cast<i32>(Scene.UfoController.Ufo.Y) - 93, frames[step] });
		}
		if (!Scene.UfoController.HideUfo)
		{
			DrawCommands.push_back({ Scene.Resources.UfoShadow, static_cast<i32>(orbitX) - 240, static_cast<i32>(orbitY) - 160, ufoFrame });
			DrawCommands.push_back({ Scene.Resources.UfoSprite, static_cast<i32>(orbitX) - 240, static_cast<i32>(orbitY) - 460, ufoFrame });
		}
	}
	DrawCommands.insert(DrawCommands.end(), Overlays.begin(), Overlays.end());
	for (const TimePopup& popup : Scene.Effects.TimePopups)
	{
		const str label = (popup.Amount >= 0 ? "+" : "") + std::to_string(popup.Amount) + "sec";
		OverlayFont.Append(DrawCommands, label, static_cast<i32>(popup.X), static_cast<i32>(popup.Y));
	}
	return DrawCommands;
}
