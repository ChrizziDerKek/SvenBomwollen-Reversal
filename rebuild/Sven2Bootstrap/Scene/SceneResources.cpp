#include "Scene/Objects/ObjectCatalog.h"
#include "Scene/SceneResources.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <sstream>

SceneResources::SceneResources(LevelScene& scene)
    : Scene(scene)
{}

const Sprite* SceneResources::GetSprite(const str& path, i32 frames)
{
	auto found = SpriteCache.find(path);
	if (found != SpriteCache.end())
		return found->second;
	return SpriteCache[path] = Scene.Engine.LoadSprite(path, static_cast<i16>(frames), 1);
}

void SceneResources::InitializeActors()
{
	if (Scene.GetEdition() == GameEdition::XXX)
	{
		// Sprite bindings correspond to XXX resource initializer 0x426650.
		const str headings[] = { "Back", "DiagBack", "Side", "DiagFront", "Front", "DiagFront", "Side", "DiagBack" };
		struct EnemyDefinition
		{
			ObjectKind Kind;
			cstr Family;
		};
		const EnemyDefinition enemies[] = {
			{ ObjectKind::Alien, "Alien" },
			{ ObjectKind::Dog, "Dog" },
			{ ObjectKind::Farmer, "Guy" },
			{ ObjectKind::Girl, "Broemse" }
		};
		for (const EnemyDefinition& enemy : enemies)
		{
			const str family = enemy.Family;
			::ActorSprites& sprites = ActorSprites[enemy.Kind];
			sprites.Stand = NamedSprite(family + "Stand");
			for (i32 direction = 0; direction < 8; ++direction)
			{
				sprites.Walk[direction] = NamedSprite(family + "Walk" + headings[direction]);
				if (enemy.Kind == ObjectKind::Alien)
					sprites.Idle[direction] = NamedSprite("AlienZero" + headings[direction]);
			}
			::EnemyRules& rules = EnemyRules[enemy.Kind];
			auto setting = [&](cstr suffix, f32 fallback) { return Scene.Parameter((family + suffix).c_str(), fallback); };
			rules.Speed = setting("Speed", 0);
			rules.Run = setting("RunMultiplier", 1);
			rules.Diagonal = setting("AdjustXY", 0.75f);
			rules.WalkRate = setting("WalkFrameRate", 15);
			// Native XML spells the alien run-rate key ALien, unlike its other keys.
			rules.RunRate = enemy.Kind == ObjectKind::Alien ? Scene.Parameter("ALienRunFrameRate", 30) : setting("RunFrameRate", 30);
			rules.Idle = setting("StandStillTime", 4);
			rules.IdleRandom = setting("StandStillTimeRandom", 1);
			rules.Sleep = setting("SleepTime", 15);
			rules.Range = setting("HuntSvenDistance", 3);
			rules.WakeRange = setting(enemy.Kind == ObjectKind::Dog ? "WakeUpDistance" : "HuntSvenDistance", rules.Range);
			rules.HuntSteps = static_cast<i32>(setting("HuntTileLength", 20));
			rules.WalkSteps = static_cast<i32>(setting("TileLength", 5));
			rules.WalkRandom = static_cast<i32>(setting("TileLengthRandom", 2));
			rules.FightTime = setting("FightTime", 1.5f);
		}
		for (i32 direction = 0; direction < 4; ++direction)
		{
			const str side = direction == 0 || direction == 3 ? "Back" : "Front";
			PlayerBeamSprites[direction] = NamedSprite("SvenBeamUp" + side);
			ActorSprites[ObjectKind::Dog].Sleep[direction] = NamedSprite("DogSleep" + side);
			ActorSprites[ObjectKind::Dog].Beam[direction] = NamedSprite("DogBeamUp" + side);
			ActorSprites[ObjectKind::Farmer].Sleep[direction] = NamedSprite("GuyLaydown" + side);
			ActorSprites[ObjectKind::Farmer].Wake[direction] = NamedSprite("GuyStandUp" + side);
			ActorSprites[ObjectKind::Farmer].Beam[direction] = NamedSprite("GuyBeamUp" + side);
			ActorSprites[ObjectKind::Farmer].Grumble[direction] = NamedSprite("GuyGrumble" + side);
			ActorSprites[ObjectKind::Farmer].Distraction[direction] = NamedSprite("GuyDrink" + side);
			ActorSprites[ObjectKind::Girl].DogPlay[direction] = NamedSprite("BroemseWotan" + side);
			ActorSprites[ObjectKind::Girl].Beam[direction] = NamedSprite("BroemseBeamUp" + side);
			ActorSprites[ObjectKind::Girl].Distraction[direction] = NamedSprite("BroemseTeddy" + side);
			ActorSprites[ObjectKind::Girl].Battle[direction] = NamedSprite("BroemseSven" + side);
			ActorSprites[ObjectKind::Alien].Battle[direction] = NamedSprite("AlienProbe2");
			ActorSprites[ObjectKind::Alien].Beam[direction] = NamedSprite("AlienBeamUp");
			ActorSprites[ObjectKind::Dog].Battle[direction] = ActorSprites[ObjectKind::Farmer].Battle[direction] = GetSprite(Scene.CoreAsset("Sven2/gfx/anim_cloud.bmp"), 10);
		}
		struct AnimalDefinition
		{
			ObjectKind Kind;
			cstr Family;
		};
		const AnimalDefinition animals[] = {
			{ ObjectKind::Chicken, "Chicken" },
			{ ObjectKind::Goat, "Goat" },
			{ ObjectKind::Pig, "Pig" },
			{ ObjectKind::Penguin, "Penguin" },
			{ ObjectKind::Turtle, "Turtle" },
			{ ObjectKind::Rabbit, "Rabbit" }
		};
		for (const AnimalDefinition& animal : animals)
		{
			const str family = animal.Family;
			ActorSprites[animal.Kind].Stand = NamedSprite(family + "Stand");
			for (i32 direction = 0; direction < 4; ++direction)
			{
				const str side = direction == 0 || direction == 3 ? "Back" : "Front";
				ShagAnimation& action = AnimalActions[animal.Kind][direction];
				action.Sprite = NamedSprite(family + "Shag" + side);
				action.Frames = FrameList("aSvenShag" + family + (animal.Kind == ObjectKind::Chicken ? "" : side));
				action.Sound = Scene.Engine.GetData().SoundPath("SfxFuck" + family, GameEdition::XXX);
				action.Facing = direction;
				action.Mirror = animal.Kind == ObjectKind::Chicken ? SheepMirror[direction] : direction >= 2;
				AnimalEating[animal.Kind][direction] = { NamedSprite(family + "Eat" + side), FrameList("a" + family + "Eat" + side), "", direction, direction >= 2 };
				AnimalIdle[animal.Kind][direction] = { NamedSprite(family + "Idle" + side), FrameList("a" + family + "Idle" + side), "", direction, direction >= 2 };
			}
			AnimalZero[animal.Kind] = { NamedSprite(family + "Zero"), FrameList("a" + family + "ZeroFront"), "", 0, false };
		}
		MermaidJump = NamedSprite("MermaidJump");
		MermaidAction = { NamedSprite("MermaidShag"), FrameList("aMermaidShag"), Scene.Engine.GetData().SoundPath("SfxShagMermaid", GameEdition::XXX), 0, false };
		ActorSprites[ObjectKind::Mermaid].Stand = MermaidJump;
		for (const ItemDefinition& item : GetItemDefinitions())
			ItemSprites[item.Kind] = GetSprite(Scene.CoreAsset(item.SpriteAsset), item.Frames);
		cstr effects[] = { "symbol_power.bmp", "symbol_bubbles_good.bmp", "symbol_smell.bmp", "symbol_flowers.bmp", "icon_alien.bmp" };
		const i32 effectFrames[] = { 13, 6, 14, 8, 13 };
		for (i32 index = 0; index < 5; ++index)
			EffectSprites[index] = GetSprite(Scene.CoreAsset(str("Sven2/gfx/") + effects[index]), effectFrames[index]);
		UfoSprite = GetSprite(Scene.CoreAsset("Sven2/gfx/ufo.bmp"), 13);
		UfoShadow = GetSprite(Scene.CoreAsset("Sven2/gfx/ufo_shadow.bmp"), 13);
		BeamSprite = GetSprite(Scene.CoreAsset("Sven2/gfx/anim_beam.bmp"), 44);
		MoodSprite = GetSprite(Scene.CoreAsset("Sven2/gfx/symbols.bmp"), 12);
		KettleSprite = GetSprite(Scene.CoreAsset("Sven2/gfx/symbol_kettle.bmp"), 12);
		Digging = GetSprite(Scene.CoreAsset("Sven2/gfx/Digging.bmp"), 18);
		SleepSymbols = NamedSprite("GuyZzz");
		Explosion = GetSprite(Scene.CoreAsset("Sven2/gfx/explode.bmp"), 14);
		Hearts = GetSprite(Scene.CoreAsset("Sven2/gfx/anim_clouds_with_hearts.bmp"), 21);
		return;
	}
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
	if (Scene.GetEdition() == GameEdition::XXX)
	{
		Stand = NamedSprite("SvenStand");
		ActorSprites[ObjectKind::Player].Stand = Stand;
		SheepStand = NamedSprite("SheepStand");
		ActorSprites[ObjectKind::Sheep].Stand = SheepStand;
		Chewing = NamedSprite("SheepChewing");
		const str headings[] = { "Back", "DiagBack", "Side", "DiagFront", "Front", "DiagFront", "Side", "DiagBack" };
		const str walks[] = { "Back", "02", "Side", "01", "Front", "01", "Side", "02" };
		for (i32 direction = 0; direction < 8; ++direction)
		{
			Walks[direction] = NamedSprite("SvenWalk" + walks[direction]);
			Whistles[direction] = NamedSprite("SvenWhistle" + headings[direction]);
			Blinks[direction] = NamedSprite("SvenBlink" + headings[direction]);
			Electric[direction] = NamedSprite("SvenElectric" + headings[direction]);
		}
		for (i32 direction = 0; direction < 4; ++direction)
		{
			const bool back = direction == 0 || direction == 3;
			const str side = back ? "Back" : "Front";
			SheepWalks[direction] = NamedSprite("SheepWalk" + side);
			DemonSheep[direction] = NamedSprite("SheepDemon" + side);
			DemonWalks[direction] = NamedSprite("SheepDemonRun" + side);
			Turns[direction] = NamedSprite("SheepWhistle" + side);
			Kicks[direction] = NamedSprite(back ? "SheepKick2" : "SheepKick1");
			Frigida[direction] = NamedSprite("FrigidaNormal" + side);
		}
		Flash = GetSprite(Scene.CoreAsset("Sven2/gfx/Flash.bmp"), 2);
		Scene.GetPlayer().Speed = Scene.Parameter("SvenSpeed", 145);
		Scene.GetPlayer().Diagonal = Scene.Parameter("SvenDiagonalMultiplier", 0.75f);
		Scene.GetPlayer().WalkRate = Scene.Parameter("SvenWalkDefaultFrameRate", 24);
		Scene.GetPlayer().DefaultRate = Scene.Parameter("SvenDefaultFrameRate", 15);
		Scene.GetPlayer().ActionRate = Scene.Parameter("SvenShagDefaultFrameRate", 20);
		Scene.GetPlayer().FrigidaRate = Scene.Parameter("SvenShagFrigidaFrameRate", 16);
		// XXX 0x44F250: the 78 native facing entries, two mirrored variants per interaction.
		static constexpr i32 facings[] = { 0, 3, 3, 0, 2, 1, 2, 1, 0, 3, 0, 3, 2, 1, 1, 2, 1, 2, 1, 2, 0, 3, 2, 1, 3, 0, 2, 1, 0, 3, 0, 3, 2, 1, 3, 0, 3, 0, 3, 0, 1, 2, 2, 1, 2, 1, 3, 0, 1, 2, 1, 2, 1, 2, 2, 1, 2, 1, 0, 3, 0, 3, 3, 0, 3, 0, 3, 0, 2, 1, 1, 2, 2, 1, 1, 2, 3, 0 };
		constexpr i32 interactionCount = static_cast<i32>(sizeof(facings) / sizeof(facings[0])) / 2;
		Interactions.clear();
		for (i32 variant = 0; variant < interactionCount; ++variant)
		{
			const bool original = variant < 13;
			const i32 number = original ? variant + 2 : variant - 12;
			const str digits = (number < 10 ? "0" : "") + std::to_string(number);
			const str spriteName = (original ? "Shag" : "NF") + digits;
			const str framesName = (original ? "aShagAnimO" : "aShagAnimN") + digits;
			const str soundName = (original ? "SfxFuck" : "SfxNewFuck") + digits;
			const Sprite* sprite = NamedSprite(spriteName);
			const std::vector<i32>& frames = FrameList(framesName);
			const str sound = Scene.Engine.GetData().SoundPath(soundName, GameEdition::XXX);
			Interactions.push_back({ sprite, frames, sound, facings[variant * 2], false });
			Interactions.push_back({ sprite, frames, sound, facings[variant * 2 + 1], true });
		}
		const f32 count = Scene.Parameter("ShagAnims", static_cast<f32>(interactionCount));
		if (!std::isfinite(count) || count < 1 || count > interactionCount || std::floor(count) != count)
			throw std::runtime_error("Invalid XXX ShagAnims parameter.");
		Interactions.resize(static_cast<size_t>(count) * 2);
		return;
	}
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
	Scene.GetPlayer().Speed = positiveParameter("SvenSpeed", 145);
	Scene.GetPlayer().Diagonal = positiveParameter("SvenDiagonalMultiplier", 0.75f);
	Scene.GetPlayer().WalkRate = positiveParameter("SvenWalkDefaultFrameRate", 24);
	Scene.GetPlayer().DefaultRate = positiveParameter("SvenDefaultFrameRate", 15);
	Scene.GetPlayer().ActionRate = positiveParameter("SvenShagDefaultFrameRate", 20);
	Scene.GetPlayer().FrigidaRate = positiveParameter("SvenShagFrigidaFrameRate", 16);
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

const Sprite* SceneResources::NamedSprite(const str& name)
{
	const GameData::SpriteDefinition& definition = Scene.Engine.GetData().GetSpriteDefinition(name, Scene.GetEdition());
	const str key = "named:" + name;
	auto found = SpriteCache.find(key);
	if (found != SpriteCache.end())
		return found->second;
	return SpriteCache[key] = Scene.Engine.LoadSprite(definition.File, definition.Rows, definition.Columns);
}

const std::vector<i32>& SceneResources::FrameList(const str& name)
{
	auto found = FrameLists.find(name);
	if (found != FrameLists.end())
		return found->second;
	const str csv = Scene.Engine.GetData().StringParam(name, "", Scene.GetEdition());
	if (csv.empty())
		throw std::runtime_error("Missing native animation parameter: " + name);
	std::vector<i32> frames;
	std::istringstream stream(csv);
	str token;
	while (std::getline(stream, token, ','))
	{
		// The shipped aShagAnimN26 ends in a comma; whitespace-only cells carry no frame.
		if (token.find_first_not_of(" \t\r\n") == str::npos)
			continue;
		std::istringstream value(token);
		i32 frame = -1;
		if (!(value >> frame) || frame < 0)
			throw std::runtime_error("Invalid native animation parameter: " + name);
		value >> std::ws;
		if (!value.eof())
			throw std::runtime_error("Invalid native animation parameter: " + name);
		frames.push_back(frame);
	}
	if (frames.empty())
		throw std::runtime_error("Empty native animation parameter: " + name);
	return FrameLists.emplace(name, std::move(frames)).first->second;
}

const std::vector<i32>& SceneResources::AnimationFrames(AnimationSequence sequence)
{
	if (Scene.GetEdition() == GameEdition::Zwo)
		return ActorFrameTables.at(sequence);
	static const std::map<AnimationSequence, str> parameters = {
		{ AnimationSequence::UfoBeam, "aBeams" },
		{ AnimationSequence::AlienStandingIdle, "aAlienLookEast" },
		{ AnimationSequence::AlienVerticalIdle, "aAlienLookWest1" },
		{ AnimationSequence::AlienDiagonalIdle, "aAlienLookWest2" },
		{ AnimationSequence::AlienProbe, "aAlienProbeSven" },
		{ AnimationSequence::GirlWalk, "aBroemseWalk" },
		{ AnimationSequence::GirlTeddyBack, "aBroemseTeddyBack" },
		{ AnimationSequence::GirlTeddyFront, "aBroemseTeddyFront" },
		{ AnimationSequence::GirlSvenBack, "aBroemseCuddleSvenBack" },
		{ AnimationSequence::GirlSvenFront, "aBroemseCuddleSvenFront" },
		{ AnimationSequence::GirlDogBack, "aBroemseCuddleDogBack" },
		{ AnimationSequence::GirlDogFront, "aBroemseCuddleDogFront" },
		{ AnimationSequence::FarmerDrink, "aGuyDrink" },
		{ AnimationSequence::FarmerLieDown, "aGuyLayDown" },
		{ AnimationSequence::FarmerStandUp, "aGuyStandUp" },
		{ AnimationSequence::FarmerGrumbleBack, "aGuyGrumbleBack" },
		{ AnimationSequence::FarmerGrumbleFront, "aGuyGrumbleFront" },
		{ AnimationSequence::ChickenInteraction, "aSvenShagChicken" },
		{ AnimationSequence::GoatInteractionBack, "aSvenShagGoatBack" },
		{ AnimationSequence::GoatInteractionFront, "aSvenShagGoatFront" },
		{ AnimationSequence::PigInteractionBack, "aSvenShagPigBack" },
		{ AnimationSequence::PigInteractionFront, "aSvenShagPigFront" },
		{ AnimationSequence::DemonTransform, "aSheepTurnIntoDevil" },
		{ AnimationSequence::DemonRecover, "aDevilTurnIntoSheep" },
		{ AnimationSequence::DemonShock, "aSheepHeadBang" },
		{ AnimationSequence::ElectricFlash, "aFlash" },
		{ AnimationSequence::PlayerElectricShock, "aSvenElectro" }
	};
	return FrameList(parameters.at(sequence));
}
