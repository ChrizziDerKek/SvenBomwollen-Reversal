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

LevelScene::LevelScene(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, u32 seed, u32* sharedRandomState, size_t playerCount)
    : Engine(engine), GameplayEdition(engine.GetData().GetGameplayEdition()), PlayerCount(GameplayEdition == GameEdition::XXX ? std::clamp(playerCount, size_t{ 1 }, size_t{ 2 }) : 1), Objects(*this), RandomState(seed), SharedRandomState(sharedRandomState), Grid(*this), Resources(*this), Items(*this), UfoController(*this), Effects(*this), Renderer(*this)
{
	const TileSetInfo& descriptor = engine.GetData().TileSet(pack, level);
	Log("Read level map: " + level.Map);
	const LevelFile* file = new LevelFile(engine.ReadAsset(level.Map), level.Map);
	UfoController.HideUfo = file->GetHidesUfo();
	if (pack.Edition == GameEdition::XXX)
	{
		Mode = file->GetBonusMode();
		if (PlayerCount > 1 && Mode != BonusMode::Sven)
			Mode = BonusMode::Alien;
	}
	InitialTime = file->GetTimeLimit();
	Objects.Reserve(LevelFile::ObjectCount);
	std::array<i32, LevelFile::ObjectCount> slots{};
	slots.fill(-1);
	Grid.Load(*file);
	Renderer.Background = { engine.LoadSprite(descriptor.Background, 1, 1, true), 0, 0, 0 };
	for (std::array<SpriteDraw, 2>& splashes : Effects.Splashes)
		splashes[0].Sprite = engine.LoadSprite(CoreAsset("Sven2/gfx/FX_all_splash.bmp"), SplashFrames, 1);
	Resources.Explosion = engine.LoadSprite(CoreAsset("Sven2/gfx/explode.bmp"), 14, 1);
	Resources.Hearts = engine.LoadSprite(CoreAsset("Sven2/gfx/anim_clouds_with_hearts.bmp"), 21, 1);
	Resources.InitializeActors();
	std::map<str, const Sprite*> sprites;
	u32 scenery = 0, actors = 0;
	size_t foundPlayers = 0;
	for (size_t slot = 0; slot < file->GetObjects().size(); ++slot)
	{
		const LevelObjectRecord& record = file->GetObjects()[slot];
		ObjectKind kind = static_cast<ObjectKind>(record.Kind());
		if (kind == ObjectKind::Inactive)
			continue;
		f32 x = record.X(), y = record.Y();
		if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 32767 || std::abs(y) > 32767)
			throw std::runtime_error("Invalid coordinates in level object " + std::to_string(slot));
		// XXX packs species metadata into bytes 17..19 (0x40EE00).
		// For actors/effects byte 16 is mirroring, not a foreground render pass.
		i32 pass = kind == ObjectKind::Scenery ? record.RenderPass() : 0;
		if (pass != 0 && pass != 1)
			continue;
		i32 texture = record.TextureOrPermanent(), facing = record.Facing();
		TileInfo image;
		if (kind == ObjectKind::Player)
		{
			if (pass != 0 || foundPlayers >= (pack.Edition == GameEdition::Zwo ? size_t{ 1 } : size_t{ 2 }))
				throw std::runtime_error("Invalid world player records for this game edition.");
			const size_t number = foundPlayers++;
			if (number >= PlayerCount)
			{
				const i32 tile = record.Tile();
				if (tile >= 0 && static_cast<size_t>(tile) < Grid.Tiles.size() && Grid.Tiles[tile] == OccupancyCode::Player)
					Grid.Tiles[tile] = OccupancyCode::Empty;
				continue;
			}
			ActivePlayer = number;
			GetPlayer().X = x;
			GetPlayer().Y = y;
		}
		const CharacterDefinition* character = FindCharacterDefinition(kind);
		const ItemDefinition* item = FindItemDefinition(kind);
		if (character)
		{
			if (character->XxxOnly && GameplayEdition != GameEdition::XXX)
				throw std::runtime_error("Object requires XXX runtime assets.");
			image = { GameplayEdition == GameEdition::XXX && character->SpriteName ? engine.GetData().GetSpriteDefinition(character->SpriteName, GameplayEdition).File : CoreAsset(character->SpriteAsset), character->GetFrames(engine.GetData(), GameplayEdition) };
			++actors;
		}
		else if (item)
			image = { CoreAsset(item->SpriteAsset), item->Frames };
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
		if (kind != ObjectKind::Scenery)
			draw.Mirror = record.Mirrored();
		if (pass == 0)
		{
			Object object;
			object.Draw = draw;
			if (IsAnimal(kind))
				Resources.ActorSprites[kind].Stand = draw.Sprite;
			object.DepthOffset = record.Height() - record.DepthAdjustment();
			object.CommittedY = y;
			object.Kind = kind;
			object.TargetPlayer = ActivePlayer;
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
			ActivePlayer = 0;
		}
		else
			Renderer.Overlays.push_back(draw);
	}
	delete file;
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
	if (pack.Edition == GameEdition::Zwo && PlayerCount == 2 && foundPlayers == 1)
	{
		SpawnCoopPlayer();
		++foundPlayers;
		++actors;
	}
	if (foundPlayers < PlayerCount || Grid.TileAt(GetPlayer().X, GetPlayer().Y) < 0)
		throw std::runtime_error("Missing or invalid player spawn.");
	for (size_t number = 0; number < PlayerCount; ++number)
	{
		PlayerScope player(*this, number);
		GetPlayer().TargetX = GetPlayer().X;
		GetPlayer().TargetY = GetPlayer().Y;
		Resources.InitializePlayer();
		GetPlayer().RestorePlayer();
		if (pack.Edition == GameEdition::Zwo && PlayerCount == 2)
			GetPlayer().MarkPlayerCell();
		Objects[GetPlayerIndex()].CommittedY = GetPlayer().Y;
	}
	Renderer.Sorted.reserve(Objects.Size());
	Renderer.DrawCommands.reserve(1 + Objects.Size() + Renderer.Overlays.size());
	Log("Loaded " + level.Map + " with " + std::to_string(scenery) + " scenery objects and " + std::to_string(actors) + " actors from folder data.");
}

void LevelScene::SpawnCoopPlayer()
{
	const i32 origin = Grid.TileAt(GetPlayer().X, GetPlayer().Y);
	if (origin < 0)
		throw std::runtime_error("Missing or invalid player spawn.");
	const size_t slot = FindAvailableObjectSlot();
	if (slot >= LevelFile::ObjectCount)
		throw std::runtime_error("No free world object slot for player two.");
	GetPlayer().TargetX = GetPlayer().X;
	GetPlayer().TargetY = GetPlayer().Y;
	std::array<i32, LevelFile::CellCount> queue{};
	std::array<bool, LevelFile::CellCount> visited{};
	size_t head = 0, tail = 1;
	queue[0] = origin;
	visited[origin] = true;
	while (head < tail)
	{
		const i32 tile = queue[head++];
		bool spawner = false;
		for (const auto& [kind, tiles] : Grid.Spawners)
			spawner = spawner || std::find(tiles.begin(), tiles.end(), tile) != tiles.end();
		if (tile != origin && Grid.Tiles[tile] == OccupancyCode::Empty && !Grid.TileDirections[tile] && Grid.ItemCodes[tile] == ItemCode::None && !spawner)
		{
			Object actor = Objects[GetPlayerIndex()];
			actor.Original = {};
			actor.TargetPlayer = 1;
			actor.Tile = tile;
			actor.X = actor.TargetX = static_cast<f32>((tile % Stride - 1) * TileSize + TileSize / 2) - PlayerCenter;
			actor.Y = actor.TargetY = actor.CommittedY = static_cast<f32>((tile / Stride - 1) * TileSize + TileSize / 2) - PlayerCenter;
			actor.Draw.X = static_cast<i32>(actor.X);
			actor.Draw.Y = static_cast<i32>(actor.Y);
			Objects.GetPlayer(1).X = actor.X;
			Objects.GetPlayer(1).Y = actor.Y;
			if (slot == Objects.Size())
				Objects.Add(actor);
			else
				Objects.Replace(slot, actor);
			return;
		}
		for (i32 direction = 0; direction < 8; ++direction)
		{
			const i32 x = tile % Stride + Directions[direction][0], y = tile / Stride + Directions[direction][1];
			if (x < 1 || x > Columns || y < 1 || y > Rows || (Grid.TileDirections[tile] && Grid.TileDirections[tile] != TileDirection[direction]))
				continue;
			const i32 next = y * Stride + x;
			const bool directionalWall = Grid.Tiles[next] == OccupancyCode::Wall && Grid.TileDirections[next] == TileDirection[direction] && Grid.Reservations[next] < 0;
			if (visited[next] || (!directionalWall && (Grid.Tiles[next] != OccupancyCode::Empty || !Grid.Unoccupied(next, slot))))
				continue;
			visited[next] = true;
			queue[tail++] = next;
		}
	}
	throw std::runtime_error("Cannot place player two: no reachable free spawn cell.");
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
	f64 value = Engine.GetData().NumericParam(key, fallback, GameplayEdition);
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
	const std::vector<i32>& frames = Resources.AnimationFrames(table);
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

void LevelScene::EmitNamedSound(cstr name, i32 frequency)
{
	str path = Engine.GetData().SoundPath(name, GameplayEdition);
	if (!path.empty())
		Sounds.emplace_back(path, SoundPlayback::OneShot, frequency);
}

void LevelScene::AddScore(i32 amount)
{
	ScoreDelta += amount;
}

void LevelScene::RecordLifeLoss()
{
	// XXX 0x41E901 resets the chain even when life insurance refunds this hit.
	if (GameplayEdition == GameEdition::XXX)
		Items.ResetGiftChain();
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
	std::array<PlayerInput, 2> inputs{};
	inputs[0] = { dx, dy, actionPressed };
	Update(seconds, inputs);
}

void LevelScene::Update(f32 seconds, const std::array<PlayerInput, 2>& inputs)
{
	if (!std::isfinite(seconds) || seconds < 0)
		return;
	for (size_t number = 0; number < PlayerCount; ++number)
	{
		PlayerScope player(*this, number);
		GetPlayer().SetInput(inputs[number]);
	}
	f32 left = seconds;
	do
	{
		const f32 dt = std::min(left, 1.0f / 60.0f);
		left -= dt;
		for (size_t number = 0; number < PlayerCount; ++number)
			Objects.GetPlayer(number).SplashAge += dt;
		Effects.UpdateEffects(dt);
		for (size_t number = 0; number < PlayerCount; ++number)
		{
			PlayerScope player(*this, number);
			GetPlayer().MarkPlayerCell();
		}
		Items.UpdateItems(dt);
		// XXX session 0x41DB36..0x41DB82 moves Sven before updating NPCs.
		if (GameplayEdition == GameEdition::XXX)
			Objects.UpdatePhase(ObjectPhase::Player, dt);
		Objects.UpdatePhase(ObjectPhase::Enemies, dt);
		Objects.UpdatePhase(ObjectPhase::Animals, dt);
		Objects.UpdatePhase(ObjectPhase::Sheep, dt);
		UfoController.UpdateUfo(dt);
		if (GameplayEdition != GameEdition::XXX)
			Objects.UpdatePhase(ObjectPhase::Player, dt);
	} while (left > 0);
}

void LevelScene::TimeExpired()
{
	if (Expired)
		return;
	Expired = true;
	if (IsBonusLevel())
	{
		FinishBonus();
		return;
	}
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
	if (GameplayEdition == GameEdition::XXX)
		for (SoundRequest& request : result)
			request.Asset = CoreAsset(request.Asset);
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

size_t LevelScene::FindAvailableObjectSlot() const
{
	for (size_t i = 0; i < Objects.Size(); ++i)
	{
		if (!Objects[i].Freed || static_cast<i32>(i) == UfoController.Ufo.Target)
			continue;
		bool referenced = false;
		for (size_t number = 0; number < PlayerCount; ++number)
			referenced = referenced || i == Objects.GetPlayer(number).GetIndex() || static_cast<i32>(i) == Objects.GetPlayer(number).ActiveSheep;
		if (!referenced)
			return i;
	}
	return Objects.Size();
}

bool LevelScene::SpawnNearPlayer(ObjectKind kind)
{
	const CharacterDefinition* character = FindCharacterDefinition(kind);
	if (character && (character->XxxOnly && GameplayEdition != GameEdition::XXX || !character->DebugSpawnable))
		return false;
	if ((!IsAnimal(kind) && !IsItem(kind)) || FindAvailableObjectSlot() >= LevelFile::ObjectCount)
		return false;
	const i32 origin = Grid.TileAt(GetPlayer().X, GetPlayer().Y);
	for (i32 radius = 1; radius <= 4; ++radius)
	{
		for (i32 dy = -radius; dy <= radius; ++dy)
		{
			for (i32 dx = -radius; dx <= radius; ++dx)
			{
				if (std::max(std::abs(dx), std::abs(dy)) != radius)
					continue;
				const i32 x = origin % Stride + dx, y = origin / Stride + dy;
				const i32 tile = y * Stride + x;
				if (x < 1 || x > Columns || y < 1 || y > Rows || !Grid.Unoccupied(tile, Objects.Size()) || Grid.ItemCodes[tile] != ItemCode::None)
					continue;
				if (IsItem(kind))
				{
					Items.SpawnItem(kind, tile);
					return Grid.ItemCodes[tile] != ItemCode::None;
				}
				const CharacterDefinition* definition = FindCharacterDefinition(kind);
				WorldObject animal;
				animal.Kind = kind;
				animal.Tile = tile;
				const Sprite* sprite = kind == ObjectKind::Sheep ? Resources.SheepStand : Resources.ActorSprites[kind].Stand;
				if (!definition || !sprite)
					return false;
				animal.CenterX = sprite->FrameWidth * 0.5f;
				animal.CenterY = sprite->FrameHeight * 0.5f;
				animal.X = animal.TargetX = static_cast<f32>((x - 1) * TileSize + TileSize / 2) - animal.CenterX;
				animal.Y = animal.TargetY = static_cast<f32>((y - 1) * TileSize + TileSize / 2) - animal.CenterY;
				animal.CommittedY = animal.Y;
				animal.DepthOffset = sprite->FrameHeight - (kind == ObjectKind::Sheep ? 0 : TileSize);
				animal.Facing = GetPlayer().Facing / 2;
				animal.Pulse = 26;
				animal.Idle = 1000;
				animal.Draw = { sprite, static_cast<i32>(animal.X), static_cast<i32>(animal.Y), animal.Facing };
				const size_t slot = FindAvailableObjectSlot();
				if (slot == Objects.Size())
					Objects.Add(animal);
				else
					Objects.Replace(slot, animal);
				Grid.Tiles[tile] = GetOccupancyCode(kind);
				++RemainingTargets;
				return true;
			}
		}
	}
	return false;
}

str LevelScene::CoreAsset(const str& path) const
{
	return Engine.GetData().EditionAsset(GameplayEdition, path);
}

PlayerCharacter& LevelScene::GetPlayer()
{
	return Objects.GetPlayer(ActivePlayer);
}

const PlayerCharacter& LevelScene::GetPlayer() const
{
	return Objects.GetPlayer(ActivePlayer);
}

size_t LevelScene::GetPlayerIndex() const
{
	return GetPlayer().GetIndex();
}

LevelScene::PlayerScope::PlayerScope(LevelScene& scene, size_t number)
    : Scene(scene), Previous(scene.ActivePlayer)
{
	if (number >= scene.PlayerCount)
		throw std::out_of_range("Inactive player context.");
	scene.ActivePlayer = number;
}

LevelScene::PlayerScope::~PlayerScope()
{
	Scene.ActivePlayer = Previous;
}

void LevelScene::FinishBonus()
{
	if (Mode != BonusMode::Sven)
	{
		BonusFinished = true;
	}
}

void LevelScene::RecordAnimalCompletion(const WorldObject& animal)
{
	++PlayerCompletions[ActivePlayer];
	if (GameplayEdition == GameEdition::XXX && animal.Kind == ObjectKind::Sheep && animal.Temperament == 2 && animal.Attempts == 1)
		BonusUnlocked = true;
}

ObjectKind LevelScene::GetPlayerKind() const
{
	switch (Mode)
	{
	case BonusMode::Farmer:
		return ObjectKind::Farmer;
	case BonusMode::Dog:
		return ObjectKind::Dog;
	case BonusMode::Girl:
		return ObjectKind::Girl;
	case BonusMode::Alien:
		return ObjectKind::Alien;
	default:
		return ObjectKind::Player;
	}
}

i32 LevelScene::FindInteractingPlayer(size_t slot) const
{
	for (size_t number = 0; number < PlayerCount; ++number)
		if (Objects.GetPlayer(number).ActiveSheep == static_cast<i32>(slot))
			return static_cast<i32>(number);
	return -1;
}
