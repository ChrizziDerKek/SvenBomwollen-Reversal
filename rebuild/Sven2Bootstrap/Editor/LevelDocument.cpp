#include "LevelDocument.h"
#include "EditorFiles.h"
#include "Scene/SceneConstants.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <limits>

namespace
{
// Documented v6/v7 object words; unchanged words remain byte-for-byte intact.
enum class Word : size_t
{
	X,
	Y,
	BaseX,
	BaseY,
	Flags,
	Width,
	Height,
	SpeedX,
	SpeedY,
	OffsetX,
	OffsetY,
	Kind,
	Frame,
	Idle,
	Rate,
	Frames,
	Packed,
	Texture,
	State,
	Facing,
	Tile,
	Mood,
	Pulse,
	Depth,
	Hunting
};
constexpr size_t HeaderBytes = 16, CellBytes = 24, ObjectsOffset = 9048, ObjectBytes = 100;
constexpr size_t MaxMarkers = 16;

template <class T>
T Read(const bytestream& bytes, size_t offset)
{
	T value;
	std::memcpy(&value, bytes.data() + offset, sizeof(value));
	return value;
}
template <class T>
void Write(bytestream& bytes, size_t offset, T value)
{
	std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
size_t Grid(const bytestream& bytes)
{
	return HeaderBytes + Read<u32>(bytes, 12);
}
size_t ObjectOffset(const bytestream& bytes, size_t slot, Word word)
{
	return Grid(bytes) + ObjectsOffset + slot * ObjectBytes + static_cast<size_t>(word) * 4;
}
template <class T>
void Set(bytestream& bytes, size_t slot, Word word, T value)
{
	Write(bytes, ObjectOffset(bytes, slot, word), value);
}
OccupancyCode CellCode(ObjectKind kind)
{
	if (kind == ObjectKind::Player)
		return OccupancyCode::Player;
	const CharacterDefinition* definition = FindCharacterDefinition(kind);
	return definition ? definition->OccupancyCode : OccupancyCode::Empty;
}
bool ValidTile(i32 tile)
{
	return tile >= 0 && tile / Stride >= 1 && tile / Stride <= Rows && tile % Stride >= 1 && tile % Stride <= Columns;
}
} // namespace

LevelDocument::LevelDocument(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level)
    : LevelDocument(engine, pack, level, engine.ReadAsset(level.Map))
{}

LevelDocument::LevelDocument(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, bytestream bytes)
    : Engine(engine), Pack(pack), Info(level), SavedInfo(level), Bytes(std::move(bytes)), SavedBytes(Bytes), Parsed(std::make_unique<LevelFile>(Bytes, level.Map))
{
	for (const LevelObjectRecord& record : Parsed->GetObjects())
	{
		if (record.Kind() != static_cast<i32>(ObjectKind::Inactive) && (!std::isfinite(record.X()) || !std::isfinite(record.Y()) || std::abs(record.X()) > 32767 || std::abs(record.Y()) > 32767))
			throw std::runtime_error("Level object coordinates must be finite and within the native range.");
	}
	for (const LevelPackInfo& source : engine.GetData().GetPacks())
	{
		if (source.Id == Pack.Id)
			continue;
		for (TileSetInfo tiles : source.TileSets)
		{
			tiles.Name = source.Id + ":" + tiles.Name;
			const bool present = std::any_of(Pack.TileSets.begin(), Pack.TileSets.end(), [&](const TileSetInfo& existing) {
				return existing.Name == tiles.Name || (existing.Background == tiles.Background && existing.Tiles.size() == tiles.Tiles.size() && std::equal(existing.Tiles.begin(), existing.Tiles.end(), tiles.Tiles.begin(), [](const TileInfo& left, const TileInfo& right) { return left.File == right.File && left.Frames == right.Frames; }));
			});
			if (!present)
				Pack.TileSets.push_back(std::move(tiles));
		}
	}
	Themes = Pack.TileSets;
}

i32 LevelDocument::TileAt(i32 x, i32 y)
{
	return x >= 0 && x < Columns * TileSize && y >= 0 && y < Rows * TileSize ? (y / TileSize + 1) * Stride + x / TileSize + 1 : -1;
}

void LevelDocument::Change(const std::function<void(bytestream&)>& operation, const LevelInfo* info)
{
	bytestream next = Bytes;
	LevelInfo metadata = info ? *info : Info;
	operation(next);
	std::unique_ptr<LevelFile> parsed = std::make_unique<LevelFile>(next, Info.Map);
	if (next == Bytes && metadata.Name == Info.Name && metadata.TileSet == Info.TileSet && metadata.Music == Info.Music)
		return;
	if (History.size() == 128)
		History.erase(History.begin());
	History.push_back({ std::move(Bytes), std::move(Info) });
	Bytes = std::move(next);
	Info = std::move(metadata);
	Parsed = std::move(parsed);
	Future.clear();
}

bool LevelDocument::Undo()
{
	if (History.empty())
		return false;
	Future.push_back({ std::move(Bytes), std::move(Info) });
	Bytes = std::move(History.back().Bytes);
	Info = std::move(History.back().Info);
	History.pop_back();
	Parsed = std::make_unique<LevelFile>(Bytes, Info.Map);
	return true;
}

bool LevelDocument::Redo()
{
	if (Future.empty())
		return false;
	History.push_back({ std::move(Bytes), std::move(Info) });
	Bytes = std::move(Future.back().Bytes);
	Info = std::move(Future.back().Info);
	Future.pop_back();
	Parsed = std::make_unique<LevelFile>(Bytes, Info.Map);
	return true;
}

const Sprite* LevelDocument::GetSprite(const LevelObjectRecord& record) const
{
	return GetSprite(static_cast<ObjectKind>(record.Kind()), record.TextureOrPermanent());
}

const TileSetInfo& LevelDocument::GetTileSet() const
{
	return Engine.GetData().TileSet(Pack, Info);
}

bool LevelDocument::Supports(ObjectKind kind) const
{
	const CharacterDefinition* definition = FindCharacterDefinition(kind);
	return definition ? definition->Category != CharacterCategory::Effect && (!definition->XxxOnly || Engine.GetData().GetGameplayEdition() == GameEdition::XXX) : kind == ObjectKind::Scenery || IsItem(kind);
}

const Sprite* LevelDocument::GetSprite(ObjectKind kind, i32 texture) const
{
	if (kind == ObjectKind::Scenery)
	{
		const TileInfo& tile = Engine.GetData().TileSet(Pack, Info).Tiles.at(texture);
		return Engine.LoadSprite(tile.File, tile.Frames, 1);
	}
	const GameEdition edition = Engine.GetData().GetGameplayEdition();
	const CharacterDefinition* character = FindCharacterDefinition(kind);
	if (character)
	{
		if (edition == GameEdition::XXX && character->SpriteName)
		{
			const GameData::SpriteDefinition& sprite = Engine.GetData().GetSpriteDefinition(character->SpriteName, edition);
			return Engine.LoadSprite(sprite.File, sprite.Rows, sprite.Columns);
		}
		return Engine.LoadSprite(Engine.GetData().EditionAsset(edition, character->SpriteAsset), character->Frames, 1);
	}
	const ItemDefinition* item = FindItemDefinition(kind);
	return item ? Engine.LoadSprite(Engine.GetData().EditionAsset(edition, item->SpriteAsset), item->Frames, 1) : nullptr;
}

void LevelDocument::CheckDestination(ObjectKind kind, i32 tile, i32 ignore) const
{
	if (!ValidTile(tile))
		throw std::runtime_error("Choose a cell inside the level.");
	if (CellCode(kind) == OccupancyCode::Empty && !IsItem(kind))
		return;
	const LevelCell& cell = Parsed->GetCells()[tile];
	const bool own = ignore >= 0 && Parsed->GetObjects()[ignore].Tile() == tile;
	if (!own && (cell.Terrain != OccupancyCode::Empty || cell.ItemCode != ItemCode::None))
		throw std::runtime_error("That cell is occupied. Move/remove its object or collision first.");
	for (size_t slot = 0; slot < LevelFile::ObjectCount; ++slot)
	{
		const LevelObjectRecord& record = Parsed->GetObjects()[slot];
		const ObjectKind other = static_cast<ObjectKind>(record.Kind());
		if (static_cast<i32>(slot) != ignore && record.Tile() == tile && other != ObjectKind::Inactive && (CellCode(other) != OccupancyCode::Empty || IsItem(other)))
			throw std::runtime_error("Another object owns that cell.");
	}
}

void LevelDocument::ClearLinks(bytestream& bytes, size_t slot) const
{
	const LevelObjectRecord& record = Parsed->GetObjects().at(slot);
	const ObjectKind kind = static_cast<ObjectKind>(record.Kind());
	const i32 tile = record.Tile();
	if (!ValidTile(tile))
		return;
	const size_t cell = Grid(bytes) + tile * CellBytes;
	if (CellCode(kind) != OccupancyCode::Empty && Read<i32>(bytes, cell) == static_cast<i32>(CellCode(kind)))
	{
		Write(bytes, cell, i32{ 0 });
		Write(bytes, cell + 8, i32{ -1 });
	}
	if (IsItem(kind) && Read<i32>(bytes, cell + 16) == static_cast<i32>(slot))
	{
		Write(bytes, cell + 12, u8{ 0 });
		Write(bytes, cell + 16, i32{ -1 });
	}
}

void LevelDocument::Link(bytestream& bytes, size_t slot, ObjectKind kind, i32 tile) const
{
	const size_t cell = Grid(bytes) + tile * CellBytes;
	if (CellCode(kind) != OccupancyCode::Empty)
	{
		Write(bytes, cell, static_cast<i32>(CellCode(kind)));
		Write(bytes, cell + 8, static_cast<i32>(slot));
	}
	if (IsItem(kind))
	{
		Write(bytes, cell + 12, static_cast<u8>(GetItemDefinition(kind).Code));
		Write(bytes, cell + 16, static_cast<i32>(slot));
	}
	Set(bytes, slot, Word::Tile, tile);
}

i32 LevelDocument::Place(ObjectKind kind, i32 tile, i32 texture, const POINT* sceneryPosition)
{
	CheckDestination(kind, tile);
	if (sceneryPosition && kind != ObjectKind::Scenery)
		throw std::runtime_error("Free positioning applies to scenery only.");
	const CharacterDefinition* definition = FindCharacterDefinition(kind);
	if (definition && (definition->Category == CharacterCategory::Effect || (definition->XxxOnly && Engine.GetData().GetGameplayEdition() != GameEdition::XXX)))
		throw std::runtime_error("This object is unavailable in this runtime.");
	if (kind != ObjectKind::Scenery && !definition && !IsItem(kind))
		throw std::runtime_error("Unsupported authored object.");
	i32 slot = -1, players = 0;
	for (size_t index = 0; index < LevelFile::ObjectCount; ++index)
	{
		if (Parsed->GetObjects()[index].Kind() == static_cast<i32>(ObjectKind::Inactive) && slot < 0)
			slot = static_cast<i32>(index);
		players += Parsed->GetObjects()[index].Kind() == static_cast<i32>(ObjectKind::Player);
	}
	if (slot < 0)
		throw std::runtime_error("All 256 object slots are in use.");
	if (kind == ObjectKind::Player && players >= (Parsed->GetVersion() == 6 ? 1 : 2))
		throw std::runtime_error("Move an existing player start; this format has enough authored players.");
	Change([&](bytestream& next) {
		std::fill_n(next.begin() + ObjectOffset(next, slot, Word::X), ObjectBytes, u8{ 0 });
		Set(next, slot, Word::Kind, static_cast<i32>(kind));
		Set(next, slot, Word::Texture, kind == ObjectKind::Scenery ? texture : (IsItem(kind) ? 1 : -1));
		const SpriteDraw preview = PlacementPreview(kind, tile, texture);
		const Sprite* image = preview.Sprite;
		if (sceneryPosition && (sceneryPosition->x <= -image->FrameWidth || sceneryPosition->x >= Columns * TileSize || sceneryPosition->y <= -image->FrameHeight || sceneryPosition->y >= Rows * TileSize))
			throw std::runtime_error("Keep some scenery visible inside the level.");
		const bool scenery = kind == ObjectKind::Scenery;
		const f32 x = static_cast<f32>(sceneryPosition ? sceneryPosition->x : preview.X), y = static_cast<f32>(sceneryPosition ? sceneryPosition->y : preview.Y);
		Set(next, slot, Word::X, x);
		Set(next, slot, Word::Y, y);
		Set(next, slot, Word::BaseX, x);
		Set(next, slot, Word::BaseY, y);
		Set(next, slot, Word::Width, image->FrameWidth);
		Set(next, slot, Word::Height, image->FrameHeight);
		Set(next, slot, Word::Frames, image->Rows * image->Columns);
		Set(next, slot, Word::Idle, 900.0f);
		Set(next, slot, Word::Rate, kind == ObjectKind::Sheep ? 20.0f : 15.0f);
		Set(next, slot, Word::Mood, i32{ 6 });
		Set(next, slot, Word::Pulse, 26.0f);
		Set(next, slot, Word::Depth, scenery ? 0 : (kind == ObjectKind::Player ? 30 : 40));
		if (IsItem(kind))
			Set(next, slot, Word::Frame, GetItemDefinition(kind).Frame);
		if (!scenery)
			Link(next, slot, kind, tile);
	});
	return slot;
}

void LevelDocument::Move(size_t slot, i32 dx, i32 dy, bool snapScenery)
{
	const LevelObjectRecord& record = Parsed->GetObjects().at(slot);
	const ObjectKind kind = static_cast<ObjectKind>(record.Kind());
	if (kind == ObjectKind::Inactive)
		throw std::runtime_error("Select an object first.");
	if (kind != ObjectKind::Scenery || snapScenery)
	{
		dx = static_cast<i32>(std::round(static_cast<f32>(dx) / TileSize)) * TileSize;
		dy = static_cast<i32>(std::round(static_cast<f32>(dy) / TileSize)) * TileSize;
	}
	if (!dx && !dy)
		return;
	const i32 column = record.Tile() % Stride + dx / TileSize, row = record.Tile() / Stride + dy / TileSize;
	if (kind != ObjectKind::Scenery && (column < 1 || column > Columns || row < 1 || row > Rows))
		throw std::runtime_error("Keep the actor inside the level.");
	const i32 tile = row * Stride + column;
	if (kind != ObjectKind::Scenery)
		CheckDestination(kind, tile, static_cast<i32>(slot));
	else if (record.X() + dx < -record.Width() || record.X() + dx >= 800 || record.Y() + dy < -record.Height() || record.Y() + dy >= 600)
		throw std::runtime_error("Keep scenery within the level.");
	Change([&](bytestream& next) {
		if (kind != ObjectKind::Scenery)
			ClearLinks(next, slot);
		for (Word word : { Word::X, Word::BaseX })
			Set(next, slot, word, Read<f32>(next, ObjectOffset(next, slot, word)) + dx);
		for (Word word : { Word::Y, Word::BaseY })
			Set(next, slot, word, Read<f32>(next, ObjectOffset(next, slot, word)) + dy);
		if (kind != ObjectKind::Scenery)
			Link(next, slot, kind, tile);
	});
}

void LevelDocument::Delete(size_t slot)
{
	const ObjectKind kind = static_cast<ObjectKind>(Parsed->GetObjects().at(slot).Kind());
	if (kind == ObjectKind::Player)
	{
		size_t players = 0;
		for (const LevelObjectRecord& record : Parsed->GetObjects())
			players += record.Kind() == static_cast<i32>(ObjectKind::Player);
		if (players <= 1)
			throw std::runtime_error("Keep at least one player start.");
	}
	Change([&](bytestream& next) { ClearLinks(next, slot); Set(next, slot, Word::Kind, static_cast<i32>(ObjectKind::Inactive)); });
}

void LevelDocument::Rotate(size_t slot)
{
	const LevelObjectRecord& record = Parsed->GetObjects().at(slot);
	const CharacterDefinition* definition = FindCharacterDefinition(static_cast<ObjectKind>(record.Kind()));
	if (!definition || definition->Category == CharacterCategory::Player || definition->Category == CharacterCategory::Emitter || definition->Category == CharacterCategory::Mermaid)
		return;
	const i32 facing = (record.Facing() + 1) % (definition->Category == CharacterCategory::Animal ? 4 : 8);
	Change([&](bytestream& next) { Set(next, slot, Word::Facing, facing); Set(next, slot, Word::Frame, facing); });
}

void LevelDocument::ToggleCollision(i32 tile)
{
	if (!ValidTile(tile))
		throw std::runtime_error("Choose a cell inside the level.");
	const LevelCell& cell = Parsed->GetCells()[tile];
	if ((cell.Terrain != OccupancyCode::Empty && cell.Terrain != OccupancyCode::Wall) || cell.ItemCode != ItemCode::None)
		throw std::runtime_error("Move/remove that object first. Water and teleports are preserved.");
	Change([&](bytestream& next) { Write(next, Grid(next) + tile * CellBytes, static_cast<i32>(cell.Terrain == OccupancyCode::Wall ? OccupancyCode::Empty : OccupancyCode::Wall)); Write(next, Grid(next) + tile * CellBytes + 4, u8{ 0 }); });
}

void LevelDocument::SetMetadata(const str& name, const str& music)
{
	if (name.empty() || name.size() > 256 || name.find_first_of("\r\n\t") != str::npos || name.find('\0') != str::npos)
		throw std::runtime_error("Use a nonempty title of at most 256 UTF-8 bytes, without control characters.");
	if (!music.empty())
	{
		if (music.find('\0') != str::npos || music.find_first_of("\r\n\t") != str::npos || !std::filesystem::is_regular_file(Engine.GetData().AssetPath(music)))
			throw std::runtime_error("Music must name an installed file (or be empty).");
	}
	LevelInfo metadata = Info;
	metadata.Name = name;
	metadata.Music = music;
	Change([](bytestream&) {}, &metadata);
}

size_t LevelDocument::GetThemeIndex() const
{
	const TileSetInfo& current = Engine.GetData().TileSet(Pack, Info);
	for (size_t index = 0; index < Themes.size(); ++index)
	{
		const TileSetInfo& theme = Themes[index];
		if (theme.Background == current.Background && theme.Tiles.size() <= current.Tiles.size() && std::equal(theme.Tiles.begin(), theme.Tiles.end(), current.Tiles.begin(), [](const TileInfo& left, const TileInfo& right) { return left.File == right.File && left.Frames == right.Frames; }))
			return index;
	}
	throw std::runtime_error("Current theme is missing from the editor catalog.");
}

void LevelDocument::SetTileSet(size_t index)
{
	TileSetInfo tiles = Themes.at(index);
	const TileSetInfo& previous = Engine.GetData().TileSet(Pack, Info);
	Engine.LoadSprite(tiles.Background, 1, 1, true);
	bytestream remapped = Bytes;
	for (size_t slot = 0; slot < LevelFile::ObjectCount; ++slot)
	{
		const LevelObjectRecord& record = Parsed->GetObjects()[slot];
		if (record.Kind() != static_cast<i32>(ObjectKind::Scenery))
			continue;
		const TileInfo& original = previous.Tiles.at(record.TextureOrPermanent());
		const auto found = std::find_if(tiles.Tiles.begin(), tiles.Tiles.end(), [&](const TileInfo& tile) { return tile.File == original.File && tile.Frames == original.Frames; });
		const i32 texture = static_cast<i32>(std::distance(tiles.Tiles.begin(), found));
		if (found == tiles.Tiles.end())
			tiles.Tiles.push_back(original);
		Set(remapped, slot, Word::Texture, texture);
	}
	const auto existing = std::find_if(Pack.TileSets.begin(), Pack.TileSets.end(), [&](const TileSetInfo& set) {
		return set.Background == tiles.Background && set.Tiles.size() == tiles.Tiles.size() && std::equal(set.Tiles.begin(), set.Tiles.end(), tiles.Tiles.begin(), [](const TileInfo& left, const TileInfo& right) { return left.File == right.File && left.Frames == right.Frames; });
	});
	if (existing != Pack.TileSets.end())
		tiles.Name = existing->Name;
	else
	{
		tiles.Name += " [mixed " + std::to_string(Pack.TileSets.size() + 1) + "]";
		Pack.TileSets.push_back(tiles);
	}
	LevelInfo metadata = Info;
	metadata.TileSet = tiles.Name;
	Change([&](bytestream& next) { next = std::move(remapped); }, &metadata);
}

std::unique_ptr<LevelDocument> LevelDocument::Blank(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level)
{
	const i32 version = pack.Edition == GameEdition::XXX ? 7 : 6;
	const str name = "blank";
	bytestream bytes(HeaderBytes + name.size() + ObjectsOffset + LevelFile::ObjectCount * ObjectBytes + (version == 7 ? LevelFile::ObjectCount * sizeof(LevelHelpLayout) : 0), 0);
	Write(bytes, 0, version);
	Write(bytes, 8, version == 7 ? u32{ 1 } : u32{ 0 });
	Write(bytes, 12, static_cast<u32>(name.size()));
	std::copy(name.begin(), name.end(), bytes.begin() + HeaderBytes);
	for (size_t tile = 0; tile < LevelFile::CellCount; ++tile)
	{
		const size_t offset = Grid(bytes) + tile * CellBytes;
		Write(bytes, offset + 8, i32{ -1 });
		Write(bytes, offset + 16, i32{ -1 });
	}
	for (size_t index = 1; index <= MaxMarkers; ++index)
		Write(bytes, Grid(bytes) + LevelFile::CellCount * CellBytes + index * 4, i32{ -1 });
	Write(bytes, Grid(bytes) + ObjectsOffset - 4, 150.0f);
	for (size_t slot = 0; slot < LevelFile::ObjectCount; ++slot)
	{
		Set(bytes, slot, Word::Kind, static_cast<i32>(ObjectKind::Inactive));
		Set(bytes, slot, Word::Tile, i32{ -1 });
		if (version == 7)
		{
			const size_t extension = Grid(bytes) + ObjectsOffset + LevelFile::ObjectCount * ObjectBytes + slot * sizeof(LevelHelpLayout);
			Write(bytes, extension + 8, i32{ 400 });
			Write(bytes, extension + 12, static_cast<i32>(HelpFontStyle::Small));
		}
	}
	std::unique_ptr<LevelDocument> document = std::make_unique<LevelDocument>(engine, pack, level, std::move(bytes));
	document->Place(ObjectKind::Player, TileAt(380, 300));
	if (version == 7)
		document->Place(ObjectKind::Player, TileAt(420, 300));
	document->History.clear();
	document->MarkSaved();
	return document;
}

void LevelDocument::SetTimeLimit(f32 seconds)
{
	if (!std::isfinite(seconds) || seconds < 0 || seconds >= static_cast<f32>(std::numeric_limits<i32>::max()))
		throw std::runtime_error("Time limit must be nonnegative and fit a signed seconds counter.");
	Change([&](bytestream& next) { Write(next, Grid(next) + ObjectsOffset - 4, seconds); });
}

void LevelDocument::SetUfoVisible(bool visible)
{
	Change([&](bytestream& next) {
		const u32 flags = Read<u32>(next, 8);
		Write(next, 8, Parsed->GetVersion() == 6 ? (visible ? u32{ 0 } : u32{ 1 }) : ((flags & ~u32{ 1 }) | (visible ? u32{ 1 } : u32{ 0 })));
	});
}

void LevelDocument::SetWeather(WeatherMode mode)
{
	if (Parsed->GetVersion() != 7 || mode < WeatherMode::None || mode > WeatherMode::Bubbles)
		throw std::runtime_error("Weather flags require an XXX v7 level.");
	Change([&](bytestream& next) { Write(next, 8, (Read<u32>(next, 8) & ~(7u << 5)) | (mode == WeatherMode::None ? 0u : 1u << (4 + static_cast<u32>(mode)))); });
}

void LevelDocument::SetBonusMode(BonusMode mode)
{
	if (Parsed->GetVersion() != 7 || (mode != BonusMode::Sven && mode != BonusMode::Farmer && mode != BonusMode::Girl && mode != BonusMode::Alien))
		throw std::runtime_error("Choose a shipped XXX bonus controller (Sven, Farmer, Girl or Alien).");
	Change([&](bytestream& next) { Write(next, 8, (Read<u32>(next, 8) & ~u32{ 0x1E }) | (mode == BonusMode::Sven ? 0u : 1u << static_cast<u32>(mode))); });
}

void LevelDocument::SetTeleporters(const std::vector<i32>& destinations)
{
	if (destinations.size() > MaxMarkers)
		throw std::runtime_error("The level supports at most 16 puddle destinations.");
	std::vector<i32> unique;
	for (i32 tile : destinations)
	{
		if (!ValidTile(tile) || Parsed->GetCells()[tile].Terrain != OccupancyCode::Puddle || std::find(unique.begin(), unique.end(), tile) != unique.end())
			throw std::runtime_error("Destinations must be distinct raw cell indices containing puddles.");
		unique.push_back(tile);
	}
	Change([&](bytestream& next) {
		const size_t offset = Grid(next) + LevelFile::CellCount * CellBytes;
		Write(next, offset, static_cast<i32>(destinations.size()));
		for (size_t index = 0; index < MaxMarkers; ++index)
			Write(next, offset + 4 + index * 4, index < destinations.size() ? destinations[index] : i32{ -1 });
	});
}

void LevelDocument::RebuildTeleporters()
{
	std::vector<i32> destinations;
	for (size_t tile = 0; tile < LevelFile::CellCount; ++tile)
		if (Parsed->GetCells()[tile].Terrain == OccupancyCode::Puddle && ValidTile(static_cast<i32>(tile)))
			destinations.push_back(static_cast<i32>(tile));
	SetTeleporters(destinations);
}

void LevelDocument::PaintTerrain(i32 tile, OccupancyCode terrain)
{
	if (!ValidTile(tile) || (terrain != OccupancyCode::Empty && terrain != OccupancyCode::Wall && terrain != OccupancyCode::Puddle && terrain != OccupancyCode::Water))
		throw std::runtime_error("Choose empty, collision, puddle or water on a visible cell.");
	const LevelCell& cell = Parsed->GetCells()[tile];
	if ((cell.Terrain != OccupancyCode::Empty && cell.Terrain != OccupancyCode::Wall && cell.Terrain != OccupancyCode::Puddle && cell.Terrain != OccupancyCode::Water) || cell.ItemCode != ItemCode::None)
		throw std::runtime_error("Move/delete the linked actor or item before painting terrain.");
	if (cell.Terrain == terrain)
		return;
	if (terrain != OccupancyCode::Empty && IsSpawner(cell.Spawner))
		throw std::runtime_error("Remove the spawner before painting blocked terrain.");
	std::vector<i32> destinations = Parsed->GetPuddles();
	destinations.erase(std::remove(destinations.begin(), destinations.end(), tile), destinations.end());
	if (terrain == OccupancyCode::Puddle)
		destinations.push_back(tile);
	if (destinations.size() > MaxMarkers)
		throw std::runtime_error("At most 16 puddle destinations are supported.");
	Change([&](bytestream& next) {
		Write(next, Grid(next) + tile * CellBytes, static_cast<i32>(terrain));
		Write(next, Grid(next) + tile * CellBytes + 4, u8{ 0 });
		if (cell.Terrain != terrain && (cell.Terrain == OccupancyCode::Puddle || terrain == OccupancyCode::Puddle))
		{
			const size_t offset = Grid(next) + LevelFile::CellCount * CellBytes;
			Write(next, offset, static_cast<i32>(destinations.size()));
			for (size_t index = 0; index < MaxMarkers; ++index)
				Write(next, offset + 4 + index * 4, index < destinations.size() ? destinations[index] : i32{ -1 });
		}
	});
}

void LevelDocument::SetSpawner(i32 tile, SpawnerCode code)
{
	if (!ValidTile(tile) || (code != SpawnerCode::None && !IsSpawner(code)))
		throw std::runtime_error("Choose a valid spawner group and visible cell.");
	size_t count = 0;
	for (size_t index = 0; index < LevelFile::CellCount; ++index)
		count += static_cast<i32>(index) != tile && Parsed->GetCells()[index].Spawner == code;
	if (code != SpawnerCode::None && count >= MaxMarkers)
		throw std::runtime_error("Each spawner group supports at most 16 cells.");
	const LevelCell& cell = Parsed->GetCells()[tile];
	if (code != SpawnerCode::None && ((cell.Terrain != OccupancyCode::Empty && !IsEnemyOccupancy(cell.Terrain) && cell.Terrain != OccupancyCode::Player) || (cell.Spawner != SpawnerCode::None && !IsSpawner(cell.Spawner))))
		throw std::runtime_error("Use an open cell or an enemy/player start for a spawner.");
	Change([&](bytestream& next) {
		const size_t offset = Grid(next) + tile * CellBytes + 20;
		Write(next, offset, (Read<u32>(next, offset) & 0xFFFF0000u) | static_cast<u32>(code));
	});
}

void LevelDocument::SetCellDirection(i32 tile, i32 direction)
{
	if (!ValidTile(tile) || direction < 0 || direction > 7)
		throw std::runtime_error("Cell direction must be 0..7.");
	Change([&](bytestream& next) { Write(next, Grid(next) + tile * CellBytes + 4, static_cast<u8>(direction)); });
}

void LevelDocument::SetCellHelp(i32 tile, i32 help)
{
	if (!ValidTile(tile) || Parsed->GetVersion() != 7 || help < 0 || help > 256)
		throw std::runtime_error("XXX help IDs are 0 (none) or 1..256.");
	Change([&](bytestream& next) {
		const size_t offset = Grid(next) + tile * CellBytes + 20;
		Write(next, offset, (Read<u32>(next, offset) & 0xFFFFu) | (static_cast<u32>(help) << 16));
	});
}

void LevelDocument::SetObjectProperty(size_t slot, ObjectProperty property, f32 value)
{
	const LevelObjectRecord& record = Parsed->GetObjects().at(slot);
	const ObjectKind kind = static_cast<ObjectKind>(record.Kind());
	if (!std::isfinite(value) || std::abs(value) > 10000 || kind == ObjectKind::Inactive)
		throw std::runtime_error("Choose a live object and finite value within 10000.");
	if (property == ObjectProperty::X || property == ObjectProperty::Y)
	{
		if (kind != ObjectKind::Scenery)
		{
			Move(slot, property == ObjectProperty::X ? static_cast<i32>(std::round(value - record.X())) : 0, property == ObjectProperty::Y ? static_cast<i32>(std::round(value - record.Y())) : 0);
			return;
		}
		if (value < -(property == ObjectProperty::X ? record.Width() : record.Height()) || value >= (property == ObjectProperty::X ? 800 : 600))
			throw std::runtime_error("Keep scenery within the level.");
		Change([&](bytestream& next) {
			const Word position = property == ObjectProperty::X ? Word::X : Word::Y;
			const Word base = property == ObjectProperty::X ? Word::BaseX : Word::BaseY;
			Set(next, slot, base, Read<f32>(next, ObjectOffset(next, slot, base)) + value - (property == ObjectProperty::X ? record.X() : record.Y()));
			Set(next, slot, position, value);
		});
		return;
	}
	if (std::floor(value) != value)
		throw std::runtime_error("This property requires an integer.");
	const i32 integer = static_cast<i32>(value);
	const CharacterDefinition* definition = FindCharacterDefinition(kind);
	const bool animal = definition && definition->Category == CharacterCategory::Animal;
	Word word = Word::Depth;
	switch (property)
	{
	case ObjectProperty::Depth:
		break;
	case ObjectProperty::Pass:
		if (kind != ObjectKind::Scenery || integer < 0 || integer > 1)
			throw std::runtime_error("Scenery render pass must be 0 (sorted) or 1 (foreground).");
		Change([&](bytestream& next) { Write(next, ObjectOffset(next, slot, Word::Flags), static_cast<u8>(integer)); });
		return;
	case ObjectProperty::Frame:
		if (kind != ObjectKind::Scenery || integer < 0 || integer >= GetSprite(record)->Rows * GetSprite(record)->Columns)
			throw std::runtime_error("Choose a valid frame of the scenery sprite.");
		word = Word::Frame;
		break;
	case ObjectProperty::Texture:
	{
		if (kind != ObjectKind::Scenery || integer < 0)
			throw std::runtime_error("Texture IDs apply to scenery.");
		const Sprite* image = GetSprite(ObjectKind::Scenery, integer);
		Change([&](bytestream& next) { Set(next, slot, Word::Texture, integer); Set(next, slot, Word::Width, image->FrameWidth); Set(next, slot, Word::Height, image->FrameHeight); Set(next, slot, Word::Frame, i32{ 0 }); Set(next, slot, Word::Frames, image->Rows * image->Columns); });
		return;
	}
	case ObjectProperty::Facing:
		if (!definition || (definition->Category != CharacterCategory::Enemy && !animal) || integer < 0 || integer >= (animal ? 4 : 8))
			throw std::runtime_error("Facing is 0..3 for animals, 0..7 for enemies.");
		Change([&](bytestream& next) { Set(next, slot, Word::Facing, integer); Set(next, slot, Word::Frame, integer); });
		return;
	case ObjectProperty::Impatience:
		if (!animal || integer < 0 || integer > 10)
			throw std::runtime_error("Animal impatience must be 0..10 (native units).");
		word = Word::Mood;
		break;
	case ObjectProperty::Permanent:
		if (!IsItem(kind) || integer < 0 || integer > 1)
			throw std::runtime_error("Item permanence must be 0 or 1.");
		word = Word::Texture;
		break;
	default:
		throw std::runtime_error("Unsupported object property.");
	}
	Change([&](bytestream& next) { Set(next, slot, word, integer); });
}

void LevelDocument::Reorder(size_t slot, bool later)
{
	const LevelObjectRecord& record = Parsed->GetObjects().at(slot);
	if (record.Kind() != static_cast<i32>(ObjectKind::Scenery))
		throw std::runtime_error("Draw earlier/later applies to scenery.");
	const f32 current = record.Y() + record.Height() - record.DepthAdjustment();
	f32 target = later ? std::numeric_limits<f32>::infinity() : -std::numeric_limits<f32>::infinity();
	for (size_t index = 0; index < LevelFile::ObjectCount; ++index)
	{
		const LevelObjectRecord& other = Parsed->GetObjects()[index];
		if (index == slot || other.Kind() == static_cast<i32>(ObjectKind::Inactive) || (other.Kind() == static_cast<i32>(ObjectKind::Scenery) ? other.RenderPass() : 0) != record.RenderPass())
			continue;
		const f32 depth = other.Y() + other.Height() - other.DepthAdjustment();
		if (later ? depth >= current : depth <= current)
			target = later ? std::min(target, depth) : std::max(target, depth);
	}
	if (!std::isfinite(target))
		target = current;
	const f32 offset = record.Y() + record.Height() - (target + (later ? 1 : -1));
	SetObjectProperty(slot, ObjectProperty::Depth, later ? std::floor(offset) : std::ceil(offset));
}

LevelInfo LevelDocument::WritePreview(const str& map) const
{
	const fpath path = EditorFiles::OutputPath(Engine.GetData(), map);
	if (std::filesystem::exists(path) && std::filesystem::exists(Engine.GetData().GetRoot() / std::filesystem::u8path(Info.Map)) && std::filesystem::equivalent(path, Engine.GetData().AssetPath(Info.Map)))
		throw std::runtime_error("Playtest cannot overwrite the source level.");
	std::filesystem::create_directories(path.parent_path());
	EditorFiles::AtomicWrite(path, Bytes);
	LevelInfo level = Info;
	level.Map = map;
	return level;
}

SpriteDraw LevelDocument::PlacementPreview(ObjectKind kind, i32 tile, i32 texture) const
{
	if (!ValidTile(tile))
		throw std::runtime_error("Choose a cell inside the level.");
	const Sprite* image = GetSprite(kind, texture);
	if (!image)
		throw std::runtime_error("Object has no preview sprite.");
	const bool scenery = kind == ObjectKind::Scenery;
	const i32 x = (tile % Stride - 1) * TileSize + (scenery ? 0 : TileSize / 2 - image->FrameWidth / 2);
	const i32 y = (tile / Stride - 1) * TileSize + (scenery ? 0 : TileSize / 2 - image->FrameHeight / 2) - (kind == ObjectKind::Farmer ? 40 : (kind == ObjectKind::Girl ? 20 : 0));
	const i32 frame = IsItem(kind) ? GetItemDefinition(kind).Frame : (kind == ObjectKind::Mermaid ? 11 : (kind == ObjectKind::BubbleEmitter || kind == ObjectKind::YellowEmitter ? 15 : 0));
	return { image, x, y, frame };
}
