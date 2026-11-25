#include "Scene/Objects/ObjectCatalog.h"
#include "Scene/SceneRenderer.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

SceneRenderer::SceneRenderer(LevelScene& scene)
    : Scene(scene), OverlayFont(scene.Engine, true, false, scene.Engine.GetData().GetThemeEdition())
{}

const std::vector<SpriteDraw>& SceneRenderer::Draws()
{
	Sorted = Scene.Objects.CopyStates();
	Sorted.insert(Sorted.end(), Scene.Effects.SceneEffects.begin(), Scene.Effects.SceneEffects.end());
	// Original 0x40D72C sorts rendered Y + height - adjustment. Player grid
	// ownership stays at the committed cell while the sprite moves between cells.
	auto depth = [&](const Object& object) {
		LevelScene::PlayerScope owner(Scene, object.Kind == ObjectKind::Player ? object.TargetPlayer : 0);
		// These animations are placed at the animal's canvas, although the
		// command is stored on Sven. Keep that compound sprite at its visible
		// owner's depth instead of promoting it to Sven's approach position.
		if (object.Kind == ObjectKind::Player && Scene.GetPlayer().PlayerAction == PlayerActionState::Interaction && Scene.GetPlayer().ActiveSheep >= 0)
		{
			const WorldObject& animal = Scene.Objects[Scene.GetPlayer().ActiveSheep];
			if ((Scene.IsAnimal(animal.Kind) && animal.Kind != ObjectKind::Sheep) || animal.Kind == ObjectKind::Mermaid)
				return static_cast<f32>(animal.Y + animal.DepthOffset);
		}
		const f32 y = object.Kind == ObjectKind::Player ? Scene.GetPlayer().Y : object.Y;
		return static_cast<f32>(y + object.DepthOffset);
	};
	// Start each frame in file slot order to preserve the original equal-depth behavior.
	for (size_t i = 0; i < Sorted.size(); ++i)
	{
		for (size_t j = i + 1; j < Sorted.size(); ++j)
			if (depth(Sorted[i]) > depth(Sorted[j]))
				std::swap(Sorted[i], Sorted[j]);
	}
	DrawCommands.clear();
	DrawCommands.push_back(Background);
	auto indicator = [&](const Object& animal) {
		// XXX 0x412270: X tables 0x44F388..0x44F408, species Y tables 0x49E950..0x49E990.
		static constexpr i32 offsetX[7][4] = { { 40, 40, 20, 20 }, { 40, 24, 30, 16 }, { 50, 50, 5, 5 }, { 50, 50, 5, 5 }, { 20, 20, 20, 20 }, { 40, 24, 30, 16 }, { 40, 24, 30, 16 } };
		static constexpr i32 offsetY[7][4] = { { 0, 0, 0, 0 }, { -1, -1, -1, -1 }, { -10, 10, 10, -10 }, { -10, 20, 20, -10 }, { -1, -1, -1, -1 }, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } };
		const size_t family = AnimalIndicatorIndex(animal.Kind);
		const i32 yOffset = Scene.GetGameplayEdition() == GameEdition::XXX && animal.Kind == ObjectKind::Sheep ? -1 : offsetY[family][animal.Facing];
		DrawCommands.push_back({ animal.Kind == ObjectKind::Sheep && animal.Temperament == 2 ? Scene.Resources.KettleSprite : Scene.Resources.MoodSprite, static_cast<i32>(animal.X) + offsetX[family][animal.Facing], static_cast<i32>(animal.Y + animal.Lift) + yOffset, std::clamp(animal.Impatience + animal.MoodBlink, 0, 11) });
	};
	for (const WorldObject& object : Sorted)
	{
		const size_t number = object.Kind == ObjectKind::Player ? object.TargetPlayer : (object.InteractionPlayer >= 0 ? static_cast<size_t>(object.InteractionPlayer) : 0);
		LevelScene::PlayerScope owner(Scene, number);
		if (object.Freed || (object.Kind == ObjectKind::HeartParticle || IsWeatherParticle(object.Kind)))
			continue;
		const bool interacting = Scene.IsAnimal(object.Kind) && Scene.GetPlayer().ActiveSheep >= 0 && (Scene.GetPlayer().PlayerAction == PlayerActionState::Interaction || Scene.GetPlayer().PlayerAction == PlayerActionState::Kicked || Scene.GetPlayer().PlayerAction == PlayerActionState::Frigida) && object.Tile == Scene.Objects[Scene.GetPlayer().ActiveSheep].Tile;
		if (object.Kind == ObjectKind::Player && Scene.GetCombat().Enemy >= 0)
			continue;
		if (!interacting && object.Draw.Sprite)
			DrawCommands.push_back(object.Draw);
		if (object.Kind == ObjectKind::Player && Scene.GetPlayerCount() > 1 && !Scene.IsBonusLevel() && Scene.Items.GetPowerup().Code == ItemCode::None && !Scene.GetPlayer().PlayerBeamState && (Scene.GetPlayer().PlayerAction == PlayerActionState::Idle || Scene.GetPlayer().PlayerAction == PlayerActionState::Whistle || Scene.GetPlayer().PlayerShockAge >= 0))
		{
			const std::vector<i32>& frames = Scene.Resources.FrameList(number == 0 ? "aPlayerIcon1" : "aPlayerIcon2");
			DrawCommands.push_back({ Scene.Resources.NamedSprite(number == 0 ? "IconPlayer1" : "IconPlayer2"), static_cast<i32>(Scene.GetPlayer().X) - 15, static_cast<i32>(Scene.GetPlayer().Y) - 50, frames[static_cast<size_t>(Scene.GetPlayer().IconAge)] });
		}
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
		if (object.Kind == ObjectKind::Player && Scene.GetPlayer().ActiveSheep >= 0 && (Scene.GetPlayer().PlayerAction == PlayerActionState::Interaction || Scene.GetPlayer().PlayerAction == PlayerActionState::Kicked || Scene.GetPlayer().PlayerAction == PlayerActionState::Frigida) && Scene.IsAnimal(Scene.Objects[Scene.GetPlayer().ActiveSheep].Kind))
			indicator(Scene.Objects[Scene.GetPlayer().ActiveSheep]);
	}
	for (size_t number = 0; number < Scene.GetPlayerCount(); ++number)
	{
		const i32 frame = static_cast<i32>(Scene.Objects.GetPlayer(number).SplashAge * SplashFrameRate);
		if (frame >= SplashFrames)
			continue;
		for (SpriteDraw splash : Scene.Effects.Splashes[number])
		{
			if (!splash.Sprite)
				continue;
			splash.Frame = frame;
			DrawCommands.push_back(splash);
		}
	}
	for (size_t number = 0; number < Scene.GetPlayerCount(); ++number)
	{
		LevelScene::PlayerScope owner(Scene, number);
		if (Scene.Items.GetPowerup().Code != ItemCode::None)
		{
			const i32 effect = Scene.Items.GetPowerup().Code == ItemCode::Rocket ? 0 : (Scene.Items.GetPowerup().Code == ItemCode::SlowMushroom ? 1 : (Scene.Items.GetPowerup().Code == ItemCode::StinkMushroom ? 2 : (Scene.Items.GetPowerup().Code == ItemCode::Flower ? 3 : 4)));
			const ::Sprite* image = Scene.Resources.EffectSprites[effect];
			const SpriteDraw& player = Scene.GetPlayerDraw();
			if (player.Sprite)
				DrawCommands.push_back({ image, player.X + 28, player.Y - 4, static_cast<i32>(Scene.Items.GetPowerup().Age * 15) % image->Rows });
		}
	}

	if (Scene.UfoController.Ufo.State != UfoState::Inactive)
	{
		const f32 orbitX = std::sin(Scene.UfoController.Ufo.AngleX * 57.295776f) * Scene.UfoController.Ufo.Radius + Scene.UfoController.Ufo.X;
		const f32 orbitY = std::cos(Scene.UfoController.Ufo.AngleY * 57.295776f) * Scene.UfoController.Ufo.Radius + Scene.UfoController.Ufo.Y;
		const i32 ufoFrame = static_cast<i32>(Scene.UfoController.Ufo.Age) % 13;
		if (Scene.UfoController.Ufo.Beam)
		{
			const std::vector<i32>& frames = Scene.Resources.AnimationFrames(AnimationSequence::UfoBeam);
			const i32 step = std::min(static_cast<i32>(frames.size()) - 1, static_cast<i32>(Scene.UfoController.Ufo.BeamAge));
			DrawCommands.push_back({ Scene.Resources.BeamSprite, static_cast<i32>(Scene.UfoController.Ufo.X) - 150, static_cast<i32>(Scene.UfoController.Ufo.Y) - 93, frames[step] });
		}
		if (!Scene.UfoController.HideUfo)
		{
			DrawCommands.push_back({ Scene.Resources.UfoShadow, static_cast<i32>(orbitX) - 240, static_cast<i32>(orbitY) - 160, ufoFrame });
			DrawCommands.push_back({ Scene.Resources.UfoSprite, static_cast<i32>(orbitX) - 240, static_cast<i32>(orbitY) - 460, ufoFrame });
		}
	}
	// XXX 0x41DDC4..CB renders 0x414870's hearts/weather after the world pass.
	for (const WorldObject& object : Sorted)
		if (!object.Freed && (object.Kind == ObjectKind::HeartParticle || IsWeatherParticle(object.Kind)) && object.Draw.Sprite)
			DrawCommands.push_back(object.Draw);
	DrawCommands.insert(DrawCommands.end(), Overlays.begin(), Overlays.end());
	for (const TimePopup& popup : Scene.Effects.TimePopups)
	{
		const str label = (popup.Amount >= 0 ? "+" : "") + std::to_string(popup.Amount) + "sec";
		OverlayFont.Append(DrawCommands, label, static_cast<i32>(popup.X), static_cast<i32>(popup.Y));
	}
	return DrawCommands;
}

void SceneRenderer::AppendHelp(std::vector<SpriteDraw>& draws) const
{
	if (Scene.GetGameplayEdition() != GameEdition::XXX)
		return;
	// 0x41E1A8..226 gives player one priority when both occupy help markers.
	for (size_t number = 0; number < Scene.GetPlayerCount(); ++number)
	{
		const i32 tile = Scene.Objects[Scene.Objects.GetPlayer(number).GetIndex()].Tile;
		if (tile < 0 || static_cast<size_t>(tile) >= Scene.HelpIds.size())
			continue;
		const u16 id = Scene.HelpIds[tile];
		if (id == 0)
			continue;
		if (id > Scene.HelpLayouts.size())
			return;
		const LevelHelpLayout& layout = Scene.HelpLayouts[id - 1];
		const i32 fontIndex = static_cast<i32>(layout.Font);
		const str key = "Help" + (id < 10 ? str("0") : str{}) + std::to_string(id);
		const str text = Scene.Engine.GetData().Text(key, GameEdition::XXX);
		if (text == key || fontIndex < 0 || fontIndex >= static_cast<i32>(HelpFonts.size()))
			return;
		// 0x41E21C..226 indexes font1/font1aktiv/font2/font2aktiv with word four.
		if (layout.Font == HelpFontStyle::Small)
			OverlayFont.AppendLevelHelp(draws, text, layout.X, layout.Y, layout.RightEdge);
		else
		{
			std::unique_ptr<BitmapFont>& font = HelpFonts[fontIndex];
			if (!font)
				font = std::make_unique<BitmapFont>(Scene.Engine, layout.Font == HelpFontStyle::SmallActive, layout.Font == HelpFontStyle::LargeActive || layout.Font == HelpFontStyle::SmallActive, GameEdition::XXX);
			font->AppendLevelHelp(draws, text, layout.X, layout.Y, layout.RightEdge);
		}
		return;
	}
}
