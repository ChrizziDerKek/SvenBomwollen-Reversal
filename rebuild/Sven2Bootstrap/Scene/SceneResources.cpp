#include "Scene/SceneResources.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

SceneResources::SceneResources(LevelScene& scene)
    : Scene(scene)
{
}

const Sprite* SceneResources::GetSprite(const str& path, i32 frames)
{
	auto found = SpriteCache.find(path);
	if (found != SpriteCache.end())
		return found->second;
	return SpriteCache[path] = Scene.Engine.LoadSprite(path, static_cast<i16>(frames), 1);
}

void SceneResources::InitializeActors()
{
	cstr families[4] = {"Alien", "Dog", "Guy", "Broemse"};
	cstr lower[4] = {"alien", "dog", "guy", "broemse"};
	cstr headings[8] = {"back", "diagonal_back", "side", "diagonal_front", "front", "diagonal_front", "side", "diagonal_back"};
	const i32 walkFrames[4] = {12, 16, 16, 8};
	const f32 speeds[4] = {60, 55, 35, 50};
	const f32 idleRandom[4] = {1.5f, 2, 1.5f, 2};
	const f32 ranges[4] = {4, 3, 2, 4};
	const f32 runRates[4] = {30, 30, 30, 50};
	const f32 walkRates[4] = {15, 15, 15, 25};
	for (i32 i = 0; i < 4; ++i)
	{
		const i32 kind = i + 3;
		::ActorSprites& images = ActorSprites[kind];
		::EnemyRules& rules = EnemyRules[kind];
		str folder = str("Sven2/gfx/") + families[i] + "/";
		images.Stand = GetSprite(folder + families[i] + (kind == Kinds::Girl ? "_stand.bmp" : "_Stand.bmp"), 8);
		for (i32 d = 0; d < 8; ++d)
		{
			images.Walk[d] = GetSprite(folder + "anim_" + lower[i] + "_walk_" + headings[d] + ".bmp", walkFrames[i]);
			if (kind == Kinds::Alien)
				images.Idle[d] = GetSprite(folder + "anim_alien_zero_" + headings[d] + ".bmp", d == 0 || d == 4 ? 5 : 4);
		}
		auto setting = [&](cstr suffix, f32 fallback)
		{
			const str key = str(families[i]) + suffix;
			return Scene.Parameter(key.c_str(), fallback);
		};
		rules.Speed = setting("Speed", speeds[i]);
		rules.Run = setting("RunMultiplier", 2);
		rules.Diagonal = setting("AdjustXY", 0.75f);
		rules.WalkRate = setting("WalkFrameRate", walkRates[i]);
		rules.RunRate = setting("RunFrameRate", runRates[i]);
		rules.Idle = setting("StandStillTime", kind == Kinds::Girl ? 3.0f : 4.0f);
		rules.IdleRandom = setting("StandStillTimeRandom", idleRandom[i]);
		rules.Sleep = setting("SleepTime", 15);
		rules.Range = setting("HuntSvenDistance", ranges[i]);
		rules.WakeRange = setting(kind == Kinds::Dog ? "WakeUpDistance" : "HuntSvenDistance", ranges[i]);
		rules.HuntSteps = static_cast<i32>(setting("HuntTileLength", kind == Kinds::Farmer ? 10.0f : 20.0f));
		rules.WalkSteps = static_cast<i32>(setting("TileLength", kind == Kinds::Alien ? 3.0f : kind == Kinds::Girl ? 6.0f
		                                                                                                           : 5.0f));
		rules.WalkRandom = static_cast<i32>(setting("TileLengthRandom", kind == Kinds::Dog ? 3.0f : 2.0f));
		rules.FightTime = setting("FightTime", 1.5f);
	}
	for (i32 d = 0; d < 4; ++d)
	{
		const bool front = d == 1 || d == 2;
		PlayerBeamSprites[d] = GetSprite(str("Sven2/gfx/Sven/anim_sven_beam_up_") + (front ? "front" : "back") + ".bmp", 22);
		ActorSprites[4].Sleep[d] = GetSprite(str("Sven2/gfx/Dog/anim_dog_sleep_") + (front ? "front" : "back") + ".bmp", 4);
		ActorSprites[4].Beam[d] = GetSprite(str("Sven2/gfx/Dog/anim_dog_beamup_") + (front ? "front" : "back") + ".bmp", 6);
		ActorSprites[5].Sleep[d] = GetSprite(str("Sven2/gfx/Guy/anim_guy_laydown_") + (front ? "front" : "back") + ".bmp", 15);
		ActorSprites[5].Wake[d] = GetSprite(str("Sven2/gfx/Guy/anim_guy_standup_") + (front ? "front" : "back") + ".bmp", 14);
		ActorSprites[5].Beam[d] = GetSprite(str("Sven2/gfx/Guy/anim_guy_beam_up_diagonal_") + (front ? "front" : "back") + ".bmp", 6);
		ActorSprites[5].Grumble[d] = GetSprite(str("Sven2/gfx/Guy/anim_guy_grumble_diagonal_") + (front ? "front" : "back") + ".bmp", front ? 19 : 11);
		ActorSprites[6].DogPlay[d] = GetSprite(str("Sven2/gfx/Broemse/anim_broemse_beating_wotan_") + (front ? "front" : "back") + ".bmp", front ? 11 : 10);
		ActorSprites[5].Distraction[d] = GetSprite(str("Sven2/gfx/Guy/anim_guy_drinking_diagonal_") + (front ? "front" : "back") + ".bmp", 15);
		ActorSprites[6].Beam[d] = GetSprite(str("Sven2/gfx/Broemse/anim_broemse_beam_up_diagonal_") + (front ? "front" : "back") + ".bmp", 5);
		ActorSprites[6].Distraction[d] = GetSprite(str("Sven2/gfx/Broemse/anim_broemse_beating_teddybear_diagonal_") + (front ? "front" : "back") + ".bmp", front ? 14 : 9);
		ActorSprites[6].Battle[d] = GetSprite(str("Sven2/gfx/Broemse/anim_broemse_beating_sven_") + (front ? "front" : "back") + ".bmp", front ? 35 : 23);
		ActorSprites[3].Battle[d] = GetSprite("Sven2/gfx/Alien/anim_alien_probe2.bmp", 55);
		ActorSprites[4].Battle[d] = ActorSprites[5].Battle[d] = GetSprite("Sven2/gfx/anim_cloud.bmp", 10);
	}
	cstr animals[3] = {"Chicken", "Goat", "Pig"};
	cstr animalFiles[3] = {"chicken", "Goat", "pig"};
	for (i32 a = 0; a < 3; ++a)
	{
		ActorSprites[a + 11].Stand = GetSprite(str("Sven2/gfx/Animals/") + animals[a] + "_Stand.bmp", 4);
		for (i32 d = 0; d < 4; ++d)
		{
			const bool back = d == 0 || d == 3;
			const AnimationSequence table = a == 0 ? AnimationSequence::ChickenInteraction : a == 1 ? (back ? AnimationSequence::GoatInteractionBack : AnimationSequence::GoatInteractionFront)
			                                                                                        : (back ? AnimationSequence::PigInteractionBack : AnimationSequence::PigInteractionFront);
			const i32 frames = a == 0 ? 5 : a == 1 ? (back ? 7 : 6)
			                                       : (back ? 4 : 3);
			ShagAnimation& action = AnimalActions[a + Kinds::Chicken][d];
			action.Sprite = GetSprite(str("Sven2/gfx/Animals/anim_NF_") + animalFiles[a] + (back ? "_back.bmp" : "_front.bmp"), frames);
			action.Frames = ActorFrameTables.at(table);
			action.Sound = str("Sven2/sfx/shag_") + (a == 0 ? "chick" : a == 1 ? "goat"
			                                                                   : "pig") +
			               ".ogg";
			action.Facing = d;
			action.Mirror = a == 0 ? SheepMirror[d] : d >= 2;
		}
	}
	cstr itemFiles[4] = {"Beer.bmp", "Teddybear.bmp", "Bone.bmp", "Artifact.bmp"};
	for (i32 i = 0; i < 4; ++i)
		ItemSprites[i + 7] = GetSprite(str("Sven2/gfx/") + itemFiles[i], 1);
	for (i32 i = 14; i <= 18; ++i)
		ItemSprites[i] = GetSprite("Sven2/gfx/icons.bmp", 5);
	cstr effects[5] = {"symbol_power.bmp", "symbol_bubbles_good.bmp", "symbol_smell.bmp", "symbol_flowers.bmp", "icon_alien.bmp"};
	const i32 effectFrames[5] = {13, 6, 14, 8, 13};
	for (i32 i = 0; i < 5; ++i)
		EffectSprites[i] = GetSprite(str("Sven2/gfx/") + effects[i], effectFrames[i]);
	UfoSprite = GetSprite("Sven2/gfx/ufo.bmp", 13);
	UfoShadow = GetSprite("Sven2/gfx/ufo_shadow.bmp", 13);
	BeamSprite = GetSprite("Sven2/gfx/anim_beam.bmp", 44);
	MoodSprite = GetSprite("Sven2/gfx/symbols.bmp", 12);
	KettleSprite = GetSprite("Sven2/gfx/symbol_kettle.bmp", 12);
	Digging = GetSprite("Sven2/gfx/Digging.bmp", 18);
	SleepSymbols = GetSprite("Sven2/gfx/Guy/anim_guy_gfx_zzz.bmp", 13);
}

void SceneResources::InitializePlayer()
{
	Stand = Scene.GetPlayerDraw().Sprite;
	SheepStand = Scene.Engine.LoadSprite("Sven2/gfx/Sheep/Sheep_Stand.bmp", 4, 1);
	Chewing = Scene.Engine.LoadSprite("Sven2/gfx/Sheep/Sheep_Chewing.bmp", 20, 1);
	cstr walkFiles[8] = {"Sven_walk-back.bmp", "Sven_walk02.bmp", "Sven_walk-side.bmp", "Sven_walk01.bmp", "Sven_walk-front.bmp", "Sven_walk01.bmp", "Sven_walk-side.bmp", "Sven_walk02.bmp"};
	for (i32 i = 0; i < 8; ++i)
		Walks[i] = Scene.Engine.LoadSprite(str("Sven2/gfx/Sven/") + walkFiles[i], 14, 1);
	const ::Sprite* away = Scene.Engine.LoadSprite("Sven2/gfx/Sheep/Sheep_walkaway_120x120.bmp", 15, 1);
	const ::Sprite* up = Scene.Engine.LoadSprite("Sven2/gfx/Sheep/Sheep_walkup_120x120.bmp", 15, 1);
	SheepWalks = {away, up, up, away};
	const ::Sprite* demonBack = GetSprite("Sven2/gfx/Sheep/anim_sheep_demon_back.bmp", 23);
	const ::Sprite* demonFront = GetSprite("Sven2/gfx/Sheep/anim_sheep_demon_front.bmp", 23);
	DemonSheep = {demonBack, demonFront, demonFront, demonBack};
	const ::Sprite* demonRunBack = GetSprite("Sven2/gfx/Sheep/anim_sheep_demon_run_back.bmp", 8);
	const ::Sprite* demonRunFront = GetSprite("Sven2/gfx/Sheep/anim_sheep_demon_run_front.bmp", 8);
	DemonWalks = {demonRunBack, demonRunFront, demonRunFront, demonRunBack};
	cstr electricFiles[] = {"back", "diagonal_back", "side", "diagonal_front", "front", "diagonal_front", "side", "diagonal_back"};
	for (i32 i = 0; i < 8; ++i)
		Electric[i] = GetSprite(str("Sven2/gfx/Sven/anim_sven_electric_") + electricFiles[i] + ".bmp", 8);
	Flash = GetSprite("Sven2/gfx/flash.bmp", 2);
	const ::Sprite* back = Scene.Engine.LoadSprite("Sven2/gfx/Sheep/sheep_sb.bmp", 11, 1);
	const ::Sprite* front = Scene.Engine.LoadSprite("Sven2/gfx/Sheep/sheep_sf.bmp", 11, 1);
	Turns = {back, front, front, back};
	cstr whistleFiles[8] = {"back", "diagonal_back", "side", "diagonal_front", "front", "diagonal_front", "side", "diagonal_back"};
	for (i32 i = 0; i < 8; ++i)
		Whistles[i] = Scene.Engine.LoadSprite(str("Sven2/gfx/Sven/anim_sven_whistle_") + whistleFiles[i] + ".bmp", i % 2 ? 16 : 19, 1);
	auto positiveParameter = [&](cstr key, f32 fallback)
	{
		f64 value = Scene.Engine.GetData().NumericParam(key, fallback);
		return std::isfinite(value) && value > 0 ? static_cast<f32>(value) : fallback;
	};
	Scene.Player.Speed = positiveParameter("SvenSpeed", 145);
	Scene.Player.Diagonal = positiveParameter("SvenDiagonalMultiplier", 0.75f);
	Scene.Player.WalkRate = positiveParameter("SvenWalkDefaultFrameRate", 24);
	Scene.Player.DefaultRate = positiveParameter("SvenDefaultFrameRate", 15);
	Scene.Player.ActionRate = positiveParameter("SvenShagDefaultFrameRate", 20);
	Scene.Player.FrigidaRate = positiveParameter("SvenShagFrigidaFrameRate", 16);
	for (i32 i = 0; i < 8; ++i)
		Blinks[i] = Scene.Engine.LoadSprite(str("Sven2/gfx/Sven/anim_sven_blink_") + whistleFiles[i] + ".bmp", 2, 1);
	const ::Sprite* kickFront = Scene.Engine.LoadSprite("Sven2/gfx/Sheep/Anim_Sven_kick_01.bmp", 20, 1);
	const ::Sprite* kickBack = Scene.Engine.LoadSprite("Sven2/gfx/Sheep/Anim_Sven_kick_02.bmp", 18, 1);
	Kicks = {kickBack, kickFront, kickFront, kickBack};
	const ::Sprite* frigidaFront = Scene.Engine.LoadSprite("Sven2/gfx/Frigida/anim_frigida_final_normal_front.bmp", 3, 1);
	const ::Sprite* frigidaBack = Scene.Engine.LoadSprite("Sven2/gfx/Frigida/anim_frigida_final_normal_back.bmp", 3, 1);
	Frigida = {frigidaBack, frigidaFront, frigidaFront, frigidaBack};

	// 0x40B040 installs two variants per animation; 0x4413DC supplies all 66 Directions.
	Interactions = ShagAnimation::Load(Scene.Engine);
}
