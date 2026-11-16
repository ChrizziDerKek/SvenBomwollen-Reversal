#include "Scene/Objects/ObjectCatalog.h"
#include "Scene/LevelScene.h"
#include "Scene/SceneConstants.h"
#include "Scene/Level/LevelFile.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>

static const LevelPackInfo& FirstPack(const MudGE& engine)
{
	for (const LevelPackInfo& pack : engine.GetData().GetPacks())
		if (pack.Name == "XS")
			return pack;
	throw std::runtime_error("The XS level pack is missing.");
}

static const LevelInfo& FirstLevel(const MudGE& engine)
{
	const LevelPackInfo& pack = FirstPack(engine);
	for (const LevelSetInfo& set : pack.LevelSets)
		if (!set.Levels.empty())
			return set.Levels.front();
	throw std::runtime_error("The XS level pack has no levels.");
}

LevelScene::LevelScene(MudGE& engine)
    : LevelScene(engine, static_cast<u32>(GetTickCount64()))
{}

LevelScene::LevelScene(MudGE& engine, u32 seed)
    : LevelScene(engine, FirstPack(engine), FirstLevel(engine), seed)
{}

LevelScene::LevelScene(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, u32 seed, u32* sharedRandomState)
    : Engine(engine), Objects(*this), RandomState(seed), SharedRandomState(sharedRandomState), Grid(*this), Resources(*this), Player(Objects.GetPlayer()), Items(*this), UfoController(*this), Effects(*this), Renderer(*this)
{
	const TileSetInfo& descriptor = engine.GetData().TileSet(pack, level);
	Log("Read level map: " + level.Map);
	const LevelFile* file = new LevelFile(engine.ReadAsset(level.Map), level.Map);
	UfoController.HideUfo = file->GetHidesUfo();
	InitialTime = file->GetTimeLimit();
	Objects.Reserve(LevelFile::ObjectCount);
	std::array<i32, LevelFile::ObjectCount> slots{};
	slots.fill(-1);
	Grid.Load(*file);
	Renderer.Background = { engine.LoadSprite(descriptor.Background, 1, 1, true), 0, 0, 0 };
	Effects.Splashes[0].Sprite = engine.LoadSprite("Sven2/gfx/FX_all_splash.bmp", SplashFrames, 1);
	Resources.Explosion = engine.LoadSprite("Sven2/gfx/explode.bmp", 14, 1);
	Resources.Hearts = engine.LoadSprite("Sven2/gfx/anim_clouds_with_hearts.bmp", 21, 1);
	Resources.InitializeActors();
	std::map<str, const Sprite*> sprites;
	u32 scenery = 0, actors = 0;
	bool foundPlayer = false;
	for (size_t slot = 0; slot < file->GetObjects().size(); ++slot)
	{
		const LevelObjectRecord& record = file->GetObjects()[slot];
		ObjectKind kind = static_cast<ObjectKind>(record.Kind());
		if (kind == ObjectKind::Inactive)
			continue;
		f32 x = record.X(), y = record.Y();
		if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 32767 || std::abs(y) > 32767)
			throw std::runtime_error("Invalid coordinates in level object " + std::to_string(slot));
		i32 pass = record.RenderPass();
		if (pass != 0 && pass != 1)
			continue;
		i32 texture = record.TextureOrPermanent(), facing = record.Facing();
		TileInfo image;
		if (kind == ObjectKind::Player)
		{
			if (foundPlayer || pass != 0)
				throw std::runtime_error("Expected one world player per level.");
			foundPlayer = true;
			PlayerIndex = Objects.Size();
			Player.X = x;
			Player.Y = y;
		}
		const CharacterDefinition* character = FindCharacterDefinition(kind);
		const ItemDefinition* item = FindItemDefinition(kind);
		if (character)
		{
			image = { character->SpriteAsset, character->Frames };
			++actors;
		}
		else if (item)
			image = { item->SpriteAsset, item->Frames };
		else if (kind == ObjectKind::Scenery)
		{
			if (texture < 0 || static_cast<size_t>(texture) >= descriptor.Tiles.size())
				throw std::runtime_error("Level object references a missing tileset texture.");
			image = descriptor.Tiles[texture];
			++scenery;
		}
		else
			throw std::runtime_error("Unsupported object kind in LevelScene: " + std::to_string(static_cast<i32>(kind)));
		auto [entry, inserted] = sprites.try_emplace(image.File, nullptr);
		if (inserted)
			entry->second = engine.LoadSprite(image.File, image.Frames, 1);
		i32 frame = IsPowerup(kind) ? GetItemDefinition(kind).Frame : (kind == ObjectKind::Scenery ? std::clamp(record.Frame(), 0, static_cast<i32>(image.Frames) - 1) : std::clamp(facing, 0, static_cast<i32>(image.Frames) - 1));
		SpriteDraw draw{ entry->second, static_cast<i32>(x), static_cast<i32>(y), frame };
		if (pass == 0)
		{
			Object object;
			object.Draw = draw;
			if (IsAnimal(kind))
				Resources.ActorSprites[kind].Stand = draw.Sprite;
			object.DepthOffset = record.Height() - record.DepthAdjustment();
			object.CommittedY = y;
			object.Kind = kind;
			object.Tile = (IsAnimal(kind) || IsEnemy(kind) || IsItem(kind)) ? record.Tile() : Grid.TileAt(x, y);
			object.Facing = IsAnimal(kind) ? std::clamp(facing, 0, 3) : std::clamp(facing, 0, 7);
			object.Temperament = record.Temperament();
			object.AngerTicks = record.AngerTicks();
			object.Attempts = record.Attempts();
			object.Impatience = record.Impatience();
			object.Idle = record.Idle();
			object.Pulse = record.Pulse();
			object.X = object.TargetX = x;
			object.Y = object.TargetY = y;
			object.Original = record.GetWords();
			object.State = static_cast<ActorState>(record.State());
			object.Permanent = record.TextureOrPermanent();
			object.CenterX = record.Width() * 0.5f;
			object.CenterY = record.Height() * 0.5f;
			object.AnimRate = record.FrameRate();
			object.Hunting = record.Hunting();
			object.Retry = record.Temperament() != 0;
			object.MoodBlink = record.MoodBlink() & 1;
			object.Attempts = 0; // 0x422420 resets interaction counts at level entry.
			if (IsAnimal(kind))
			{
				object.Impatience = 2 * (object.Impatience / 2);
				++RemainingTargets;
			}
			// 0x422AF6 sets the stored adjustment (+92), not the final sort offset.
			if (IsAnimal(kind) && kind != ObjectKind::Sheep)
			{
				object.Pulse = 26;
				object.State = ActorState::Idle;
				object.DepthOffset = record.Height() - 40;
			}
			if (IsPowerup(kind))
			{
				// Native Zwo initializer 0x422B41..0x422B6F also initializes
				// serialized powerups, not just those created by a spawner.
				const ItemDefinition& definition = GetItemDefinition(kind);
				object.Pulse = Parameter(definition.LifetimeKey, definition.DefaultLifetime);
				object.DepthOffset = record.Height() - 40;
			}
			if (kind == ObjectKind::Alien)
				object.Idle = static_cast<f32>(50 + RandomInt(100));
			if (kind == ObjectKind::Alien || kind == ObjectKind::Girl)
			{
				const ::EnemyRules& rules = Resources.EnemyRules[kind];
				object.Pulse = rules.Idle + (kind == ObjectKind::Girl ? rules.IdleRandom : RandomInt(static_cast<i32>(rules.IdleRandom)));
			}
			slots[slot] = static_cast<i32>(Objects.Size());
			Objects.Add(object);
		}
		else
			Renderer.Overlays.push_back(draw);
	}
	for (size_t tile = 0; tile < Grid.ItemCodes.size(); ++tile)
	{
		const i32 slot = Grid.ItemObjects[tile];
		Grid.ItemObjects[tile] = slot >= 0 && slot < 256 ? slots[slot] : -1;
		if (Grid.ItemCodes[tile] == ItemCode::None)
			Grid.ItemObjects[tile] = -1;
	}
	std::vector<size_t> sheep;
	for (size_t i = 0; i < Objects.Size(); ++i)
		if (Objects[i].Kind == ObjectKind::Sheep)
			sheep.push_back(i);
	if (!sheep.empty())
		Objects[sheep[RandomInt(static_cast<i32>(sheep.size()))]].Temperament = 1; // 0x422C00
	Items.SpawnPeriod = Parameter("WaitTime", 10);
	Items.SpawnClock = Items.SpawnPeriod;
	if (!foundPlayer || Grid.TileAt(Player.X, Player.Y) < 0)
		throw std::runtime_error("Missing or invalid player spawn.");
	Player.TargetX = Player.X;
	Player.TargetY = Player.Y;
	Resources.InitializePlayer();
	Objects[PlayerIndex].CommittedY = Player.Y;
	Renderer.Sorted.reserve(Objects.Size());
	Renderer.DrawCommands.reserve(1 + Objects.Size() + Renderer.Overlays.size());
	Log("Loaded " + level.Map + " with " + std::to_string(scenery) + " scenery objects and " + std::to_string(actors) + " actors from folder data.");
	delete file;
}

bool LevelScene::IsAnimal(ObjectKind kind)
{
	const CharacterDefinition* definition = FindCharacterDefinition(kind);
	return definition && definition->Category == CharacterCategory::Animal;
}

bool LevelScene::IsEnemy(ObjectKind kind)
{
	const CharacterDefinition* definition = FindCharacterDefinition(kind);
	return definition && definition->Category == CharacterCategory::Enemy;
}

OccupancyCode LevelScene::GetOccupancyCode(ObjectKind kind)
{
	const CharacterDefinition* definition = FindCharacterDefinition(kind);
	if (!definition)
		throw std::invalid_argument("Object kind has no character occupancy code.");
	return definition->OccupancyCode;
}

f32 LevelScene::Parameter(cstr key, f32 fallback) const
{
	f64 value = Engine.GetData().NumericParam(key, fallback);
	return std::isfinite(value) && value >= 0 ? static_cast<f32>(value) : fallback;
}

i32 LevelScene::RandomInt(i32 limit)
{
	u32& state = SharedRandomState ? *SharedRandomState : RandomState;
	state = state * 214013u + 2531011u;
	const i32 value = static_cast<i32>((state >> 16) & 0x7fffu);
	return limit > 0 ? value % limit : 0;
}

bool LevelScene::AdvanceActor(Object& actor, AnimationSequence table, f32 rate, f32 seconds)
{
	const std::vector<i32>& frames = ActorFrameTables.at(table);
	actor.Age += seconds * rate;
	if (static_cast<size_t>(actor.Age) >= frames.size())
	{
		actor.Age = 0;
		actor.Draw.Frame = 0;
		return true;
	}
	actor.Draw.Frame = frames[static_cast<size_t>(actor.Age)];
	return false;
}

void LevelScene::EmitNamedSound(cstr name)
{
	str path = Engine.GetData().SoundPath(name);
	if (!path.empty())
		Sounds.push_back(path);
}

void LevelScene::AddScore(i32 amount)
{
	ScoreDelta += amount;
}

void LevelScene::RecordLifeLoss()
{
	// 0x412B00 insurance depends on campaign score and belongs to GameSession.
	++PendingLifeHits;
}

i32 LevelScene::TakeLivesGained()
{
	const i32 result = Items.LivesGained;
	Items.LivesGained = 0;
	return result;
}

void LevelScene::RemoveActor(size_t index, bool playerResolved)
{
	WorldObject& actor = Objects[index];
	if (actor.Freed)
		return;
	if (IsAnimal(actor.Kind))
	{
		--RemainingTargets;
		if (playerResolved && actor.Kind == ObjectKind::Sheep)
			++CompletedSheep;
	}
	if (actor.Tile >= 0 && static_cast<size_t>(actor.Tile) < Grid.Tiles.size() && Grid.Tiles[actor.Tile] == GetOccupancyCode(actor.Kind))
		Grid.Tiles[actor.Tile] = OccupancyCode::Empty;
	for (i32& reservation : Grid.Reservations)
		if (reservation == static_cast<i32>(index))
			reservation = -1;
	actor.Freed = true;
}

void LevelScene::Move(f32 seconds, i32 dx, i32 dy)
{
	Update(seconds, dx, dy, false);
}

void LevelScene::Update(f32 seconds, i32 dx, i32 dy, bool actionPressed)
{
	if (!std::isfinite(seconds) || seconds < 0)
		return;
	if (Player.PlayerAction == PlayerActionState::Idle)
		Player.ActionQueued = false; // source clears the edge flag in idle/walking state 0
	if (actionPressed && !Player.ActionDown && (Player.PlayerAction == PlayerActionState::Idle || Player.PlayerAction == PlayerActionState::Interaction || Player.PlayerAction == PlayerActionState::Frigida))
		Player.ActionQueued = true;
	Player.ActionDown = actionPressed;
	dx = std::clamp(dx, -1, 1);
	dy = std::clamp(dy, -1, 1);
	Player.SetMovement(dx, dy);
	// Substeps preserve occupancy and sequence boundaries across slow frames.
	f32 left = seconds;
	do
	{
		f32 dt = std::min(left, 1.0f / 60.0f);
		left -= dt;
		Effects.SplashAge += dt;
		Effects.SplashPlayerAge += dt;
		Effects.UpdateEffects(dt);
		Player.MarkPlayerCell();
		Items.UpdateItems(dt);
		Objects.UpdatePhase(ObjectPhase::Enemies, dt);
		Objects.UpdatePhase(ObjectPhase::Animals, dt);
		Objects.UpdatePhase(ObjectPhase::Sheep, dt);
		UfoController.UpdateUfo(dt);
		Objects.UpdatePhase(ObjectPhase::Player, dt);
	} while (left > 0);
}

void LevelScene::TimeExpired()
{
	if (Expired)
		return;
	Expired = true;
	UfoController.UpdateUfo(0); // 0x408B80 requests a remaining animal; no life penalty.
}

i32 LevelScene::TakeScoreDelta()
{
	i32 result = ScoreDelta;
	ScoreDelta = 0;
	return result;
}

f32 LevelScene::TakeTimeDelta()
{
	f32 result = Effects.TimeDelta;
	Effects.TimeDelta = 0;
	return result;
}

i32 LevelScene::TakeLifeHits()
{
	i32 result = PendingLifeHits;
	PendingLifeHits = 0;
	return result;
}

std::vector<SoundRequest> LevelScene::TakeAudioEvents()
{
	Effects.SplashSound = false;
	std::vector<SoundRequest> result;
	result.swap(Sounds);
	return result;
}

std::vector<str> LevelScene::TakeSounds()
{
	std::vector<str> result;
	for (const SoundRequest& request : TakeAudioEvents())
		result.push_back(request.Asset);
	return result;
}

const std::vector<SpriteDraw>& LevelScene::Draws()
{
	return Renderer.Draws();
}

bool LevelScene::TakeSplashSound()
{
	return Effects.TakeSplashSound();
}
