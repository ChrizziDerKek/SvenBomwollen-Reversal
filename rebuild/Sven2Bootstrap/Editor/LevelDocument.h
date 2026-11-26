#pragma once
#include "Scene/Level/LevelFile.h"
#include "Scene/Objects/ObjectCatalog.h"
#include "Engine/MudGE.h"
#include <memory>
#include <functional>

class LevelDocument
{
public:
	enum class ObjectProperty
	{
		X,
		Y,
		Depth,
		Pass,
		Frame,
		Texture,
		Facing,
		Impatience,
		Permanent
	};
	LevelDocument(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level);
	LevelDocument(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, bytestream bytes);
	static std::unique_ptr<LevelDocument> Blank(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level);
	const std::vector<TileSetInfo>& GetThemes() const
	{
		return Themes;
	}
	size_t GetThemeIndex() const;
	void MarkSaved()
	{
		SavedBytes = Bytes;
		SavedInfo = Info;
	}
	const LevelFile& GetLevel() const
	{
		return *Parsed;
	}
	const bytestream& GetBytes() const
	{
		return Bytes;
	}
	const LevelPackInfo& GetPack() const
	{
		return Pack;
	}
	const LevelInfo& GetInfo() const
	{
		return Info;
	}
	const TileSetInfo& GetTileSet() const;
	bool Supports(ObjectKind kind) const;
	bool IsDirty() const
	{
		return Bytes != SavedBytes || Info.Name != SavedInfo.Name || Info.TileSet != SavedInfo.TileSet || Info.Music != SavedInfo.Music;
	}
	bool Undo();
	bool Redo();
	i32 Place(ObjectKind kind, i32 tile, i32 texture = 0, const POINT* sceneryPosition = nullptr);
	void Move(size_t slot, i32 dx, i32 dy, bool snapScenery = true);
	void Delete(size_t slot);
	void Rotate(size_t slot);
	void SetObjectProperty(size_t slot, ObjectProperty property, f32 value);
	void Reorder(size_t slot, bool later);
	void SetMetadata(const str& name, const str& music);
	void SetTileSet(size_t index);
	void SetTimeLimit(f32 seconds);
	void SetUfoVisible(bool visible);
	void SetWeather(WeatherMode mode);
	void SetBonusMode(BonusMode mode);
	void PaintTerrain(i32 tile, OccupancyCode terrain);
	void SetSpawner(i32 tile, SpawnerCode code);
	void SetCellDirection(i32 tile, i32 direction);
	void SetCellHelp(i32 tile, i32 help);
	void SetTeleporters(const std::vector<i32>& destinations);
	void RebuildTeleporters();
	void ToggleCollision(i32 tile);
	const Sprite* GetSprite(const LevelObjectRecord& record) const;
	const Sprite* GetSprite(ObjectKind kind, i32 texture = 0) const;
	SpriteDraw PlacementPreview(ObjectKind kind, i32 tile, i32 texture = 0) const;
	LevelInfo WritePreview(const str& map) const;
	static i32 TileAt(i32 x, i32 y);

private:
	struct Snapshot
	{
		bytestream Bytes;
		LevelInfo Info;
	};
	void Change(const std::function<void(bytestream&)>& operation, const LevelInfo* info = nullptr);
	void ClearLinks(bytestream& bytes, size_t slot) const;
	void Link(bytestream& bytes, size_t slot, ObjectKind kind, i32 tile) const;
	void CheckDestination(ObjectKind kind, i32 tile, i32 ignore = -1) const;
	MudGE& Engine;
	LevelPackInfo Pack;
	std::vector<TileSetInfo> Themes;
	LevelInfo Info, SavedInfo;
	bytestream Bytes, SavedBytes;
	std::unique_ptr<LevelFile> Parsed;
	std::vector<Snapshot> History, Future;
};
