#include "Scene/Objects/ObjectCatalog.h"
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
	struct EnemyResources
	{
		ObjectKind Kind;
		cstr Family;
		cstr FilePrefix;
		i32 WalkFrames;
		f32 Speed;
		f32 IdleRandom;
		f32 Range;
		f32 RunRate;
		f32 WalkRate;
	};
	const EnemyResources enemies[] = {
		{ ObjectKind::Alien, "Alien", "alien", 12, 60, 1.5f, 4, 30, 15 },
		{ ObjectKind::Dog, "Dog", "dog", 16, 55, 2, 3, 30, 15 },
		{ ObjectKind::Farmer, "Guy", "guy", 16, 35, 1.5f, 2, 30, 15 },
		{ ObjectKind::Girl, "Broemse", "broemse", 8, 50, 2, 4, 50, 25 }
	};
	cstr headings[] = { "back", "diagonal_back", "side", "diagonal_front", "front", "diagonal_front", "side", "diagonal_back" };
	for (const EnemyResources& enemy : enemies)
	{
		const ObjectKind kind = enemy.Kind;
		::ActorSprites& images = ActorSprites[kind];
		::EnemyRules& rules = EnemyRules[kind];
		str folder = str("Sven2/gfx/") + enemy.Family + "/";
		images.Stand = GetSprite(folder + enemy.Family + (kind == ObjectKind::Girl ? "_stand.bmp" : "_Stand.bmp"), 8);
		for (i32 d = 0; d < 8; ++d)
		{
			images.Walk[d] = GetSprite(folder + "anim_" + enemy.FilePrefix + "_walk_" + headings[d] + ".bmp", enemy.WalkFrames);
			if (kind == ObjectKind::Alien)
				images.Idle[d] = GetSprite(folder + "anim_alien_zero_" + headings[d] + ".bmp", d == 0 || d == 4 ? 5 : 4);
		}
		auto setting = [&](cstr suffix, f32 fallback)
		{
			const str key = str(enemy.Family) + suffix;
			return Scene.Parameter(key.c_str(), fallback);
		};
		rules.Speed = setting("Speed", enemy.Speed);
		rules.Run = setting("RunMultiplier", 2);
		rules.Diagonal = setting("AdjustXY", 0.75f);
		rules.WalkRate = setting("WalkFrameRate", enemy.WalkRate);
		rules.RunRate = setting("RunFrameRate", enemy.RunRate);
		rules.Idle = setting("StandStillTime", kind == ObjectKind::Girl ? 3.0f : 4.0f);
		rules.IdleRandom = setting("StandStillTimeRandom", enemy.IdleRandom);
		rules.Sleep = setting("SleepTime", 15);
		rules.Range = setting("HuntSvenDistance", enemy.Range);
		rules.WakeRange = setting(kind == ObjectKind::Dog ? "WakeUpDistance" : "HuntSvenDistance", enemy.Range);
		rules.HuntSteps = static_cast<i32>(setting("HuntTileLength", kind == ObjectKind::Farmer ? 10.0f : 20.0f));
		rules.WalkSteps = static_cast<i32>(setting("TileLength", kind == ObjectKind::Alien ? 3.0f : (kind == ObjectKind::Girl ? 6.0f : 5.0f)));
		rules.WalkRandom = static_cast<i32>(setting("TileLengthRandom", kind == ObjectKind::Dog ? 3.0f : 2.0f));
		rules.FightTime = setting("FightTime", 1.5f);
	}
	for (i32 d = 0; d < 4; ++d)
	{
		const bool front = d == 1 || d == 2;
		PlayerBeamSprites[d] = GetSprite(str("Sven2/gfx/Sven/anim_sven_beam_up_") + (front ? "front" : "back") + ".bmp", 22);
		ActorSprites[ObjectKind::Dog].Sleep[d] = GetSprite(str("Sven2/gfx/Dog/anim_dog_sleep_") + (front ? "front" : "back") + ".bmp", 4);
		ActorSprites[ObjectKind::Dog].Beam[d] = GetSprite(str("Sven2/gfx/Dog/anim_dog_beamup_") + (front ? "front" : "back") + ".bmp", 6);
		ActorSprites[ObjectKind::Farmer].Sleep[d] = GetSprite(str("Sven2/gfx/Guy/anim_guy_laydown_") + (front ? "front" : "back") + ".bmp", 15);
		ActorSprites[ObjectKind::Farmer].Wake[d] = GetSprite(str("Sven2/gfx/Guy/anim_guy_standup_") + (front ? "front" : "back") + ".bmp", 14);
		ActorSprites[ObjectKind::Farmer].Beam[d] = GetSprite(str("Sven2/gfx/Guy/anim_guy_beam_up_diagonal_") + (front ? "front" : "back") + ".bmp", 6);
		ActorSprites[ObjectKind::Farmer].Grumble[d] = GetSprite(str("Sven2/gfx/Guy/anim_guy_grumble_diagonal_") + (front ? "front" : "back") + ".bmp", front ? 19 : 11);
		ActorSprites[ObjectKind::Girl].DogPlay[d] = GetSprite(str("Sven2/gfx/Broemse/anim_broemse_beating_wotan_") + (front ? "front" : "back") + ".bmp", front ? 11 : 10);
		ActorSprites[ObjectKind::Farmer].Distraction[d] = GetSprite(str("Sven2/gfx/Guy/anim_guy_drinking_diagonal_") + (front ? "front" : "back") + ".bmp", 15);
		ActorSprites[ObjectKind::Girl].Beam[d] = GetSprite(str("Sven2/gfx/Broemse/anim_broemse_beam_up_diagonal_") + (front ? "front" : "back") + ".bmp", 5);
		ActorSprites[ObjectKind::Girl].Distraction[d] = GetSprite(str("Sven2/gfx/Broemse/anim_broemse_beating_teddybear_diagonal_") + (front ? "front" : "back") + ".bmp", front ? 14 : 9);
		ActorSprites[ObjectKind::Girl].Battle[d] = GetSprite(str("Sven2/gfx/Broemse/anim_broemse_beating_sven_") + (front ? "front" : "back") + ".bmp", front ? 35 : 23);
		ActorSprites[ObjectKind::Alien].Battle[d] = GetSprite("Sven2/gfx/Alien/anim_alien_probe2.bmp", 55);
		ActorSprites[ObjectKind::Dog].Battle[d] = ActorSprites[ObjectKind::Farmer].Battle[d] = GetSprite("Sven2/gfx/anim_cloud.bmp", 10);
	}

	struct AnimalResources
	{
		ObjectKind Kind;
		cstr Folder;
		cstr FilePrefix;
		cstr Sound;
		AnimationSequence BackSequence;
		AnimationSequence FrontSequence;
		i32 BackFrames;
		i32 FrontFrames;
		bool UsesSheepMirror;
	};
	const AnimalResources animals[] = {
		{ ObjectKind::Chicken, "Chicken", "chicken", "chick", AnimationSequence::ChickenInteraction, AnimationSequence::ChickenInteraction, 5, 5, true },
		{ ObjectKind::Goat, "Goat", "Goat", "goat", AnimationSequence::GoatInteractionBack, AnimationSequence::GoatInteractionFront, 7, 6, false },
		{ ObjectKind::Pig, "Pig", "pig", "pig", AnimationSequence::PigInteractionBack, AnimationSequence::PigInteractionFront, 4, 3, false }
	};
	for (const AnimalResources& animal : animals)
	{
		ActorSprites[animal.Kind].Stand = GetSprite(str("Sven2/gfx/Animals/") + animal.Folder + "_Stand.bmp", 4);
		for (i32 d = 0; d < 4; ++d)
		{
			const bool back = d == 0 || d == 3;
			const AnimationSequence table = back ? animal.BackSequence : animal.FrontSequence;
			const i32 frames = back ? animal.BackFrames : animal.FrontFrames;
			ShagAnimation& action = AnimalActions[animal.Kind][d];
			action.Sprite = GetSprite(str("Sven2/gfx/Animals/anim_NF_") + animal.FilePrefix + (back ? "_back.bmp" : "_front.bmp"), frames);
			action.Frames = ActorFrameTables.at(table);
			action.Sound = str("Sven2/sfx/shag_") + animal.Sound + ".ogg";
			action.Facing = d;
			action.Mirror = animal.UsesSheepMirror ? SheepMirror[d] : d >= 2;
		}
	}
	for (const ItemDefinition& item : GetItemDefinitions())
		ItemSprites[item.Kind] = GetSprite(item.SpriteAsset, item.Frames);
	cstr effects[5] = { "symbol_power.bmp", "symbol_bubbles_good.bmp", "symbol_smell.bmp", "symbol_flowers.bmp", "icon_alien.bmp" };
	const i32 effectFrames[5] = { 13, 6, 14, 8, 13 };
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
	cstr walkFiles[8] = { "Sven_walk-back.bmp", "Sven_walk02.bmp", "Sven_walk-side.bmp", "Sven_walk01.bmp", "Sven_walk-front.bmp", "Sven_walk01.bmp", "Sven_walk-side.bmp", "Sven_walk02.bmp" };
	for (i32 i = 0; i < 8; ++i)
		Walks[i] = Scene.Engine.LoadSprite(str("Sven2/gfx/Sven/") + walkFiles[i], 14, 1);
	const ::Sprite* away = Scene.Engine.LoadSprite("Sven2/gfx/Sheep/Sheep_walkaway_120x120.bmp", 15, 1);
	const ::Sprite* up = Scene.Engine.LoadSprite("Sven2/gfx/Sheep/Sheep_walkup_120x120.bmp", 15, 1);
	SheepWalks = { away, up, up, away };
	const ::Sprite* demonBack = GetSprite("Sven2/gfx/Sheep/anim_sheep_demon_back.bmp", 23);
	const ::Sprite* demonFront = GetSprite("Sven2/gfx/Sheep/anim_sheep_demon_front.bmp", 23);
	DemonSheep = { demonBack, demonFront, demonFront, demonBack };
	const ::Sprite* demonRunBack = GetSprite("Sven2/gfx/Sheep/anim_sheep_demon_run_back.bmp", 8);
	const ::Sprite* demonRunFront = GetSprite("Sven2/gfx/Sheep/anim_sheep_demon_run_front.bmp", 8);
	DemonWalks = { demonRunBack, demonRunFront, demonRunFront, demonRunBack };
	cstr electricFiles[] = { "back", "diagonal_back", "side", "diagonal_front", "front", "diagonal_front", "side", "diagonal_back" };
	for (i32 i = 0; i < 8; ++i)
		Electric[i] = GetSprite(str("Sven2/gfx/Sven/anim_sven_electric_") + electricFiles[i] + ".bmp", 8);
	Flash = GetSprite("Sven2/gfx/flash.bmp", 2);
	const ::Sprite* back = Scene.Engine.LoadSprite("Sven2/gfx/Sheep/sheep_sb.bmp", 11, 1);
	const ::Sprite* front = Scene.Engine.LoadSprite("Sven2/gfx/Sheep/sheep_sf.bmp", 11, 1);
	Turns = { back, front, front, back };
	cstr whistleFiles[8] = { "back", "diagonal_back", "side", "diagonal_front", "front", "diagonal_front", "side", "diagonal_back" };
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
	Kicks = { kickBack, kickFront, kickFront, kickBack };
	const ::Sprite* frigidaFront = Scene.Engine.LoadSprite("Sven2/gfx/Frigida/anim_frigida_final_normal_front.bmp", 3, 1);
	const ::Sprite* frigidaBack = Scene.Engine.LoadSprite("Sven2/gfx/Frigida/anim_frigida_final_normal_back.bmp", 3, 1);
	Frigida = { frigidaBack, frigidaFront, frigidaFront, frigidaBack };

	// 0x40B040 installs two variants per animation; 0x4413DC supplies all 66 Directions.
	Interactions = ShagAnimation::Load(Scene.Engine);
}
