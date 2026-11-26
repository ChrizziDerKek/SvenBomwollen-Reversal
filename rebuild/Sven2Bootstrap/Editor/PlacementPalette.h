#pragma once
#include "LevelDocument.h"
#include "Input/WindowInput.h"
#include "Engine/BitmapFont.h"
#include <map>
#include <optional>

class PlacementPalette
{
public:
	enum class Group
	{
		All,
		Scenery,
		Actors,
		Items,
		Tiles,
		Properties,
		View,
		Raw,
		Count
	};
	struct TileBrush
	{
		cstr Name;
		OccupancyCode Terrain;
		SpawnerCode Spawner;
		bool SpawnerOnly;
	};
	static const std::vector<TileBrush>& GetTileBrushes();
	struct Entry
	{
		ObjectKind Kind;
		i32 Texture;
		str Name;
		Group Category;
		i32 Brush = -1;
	};
	struct Result
	{
		bool Consumed = false;
		std::optional<Entry> Selected;
	};
	static constexpr i32 GroupCount = static_cast<i32>(Group::Count), GridX = 274;
	static constexpr i32 Width = 320, HeaderHeight = 24, TabRowHeight = 24, TabsPerRow = 4, TabHeight = 48, SearchHeight = 24, FooterHeight = 28, Columns = 3, CellWidth = Width / Columns, CellHeight = 88, ThumbnailSize = 64;
	PlacementPalette();
	static Sprite MakeThumbnail(const Sprite& source, i32 frame);
	Result Handle(const WindowInput::Result& input, const LevelDocument& document, RECT bounds);
	void Append(std::vector<SpriteDraw>& commands, const LevelDocument& document, RECT bounds, const BitmapFont& font);
	bool HasSearchFocus() const
	{
		return SearchFocused;
	}
	void Blur()
	{
		SearchFocused = false;
	}
	void Select(ObjectKind kind, i32 texture);
	const LevelDocument::AnimalPlacement& GetAnimalPlacement() const
	{
		return AnimalDefaults;
	}
	LevelDocument::SceneryOrder GetSceneryOrder() const
	{
		return SceneryDepth;
	}
	void Reset();
	void Show(Group group);
	bool IsRaw() const
	{
		return Category == Group::Raw;
	}
	bool IsView() const
	{
		return Category == Group::View;
	}
	bool IsProperties() const
	{
		return Category == Group::Properties;
	}

private:
	bool HasAnimalDefaults() const;
	bool HasSceneryDepth() const
	{
		return Category == Group::Scenery || (Category == Group::All && SelectedKind == ObjectKind::Scenery);
	}
	void Refresh(const LevelDocument& document);
	std::vector<size_t> Filtered() const;
	const Sprite* Thumbnail(const LevelDocument& document, const Entry& entry);
	static i32 VisibleRows(RECT bounds);
	std::vector<Entry> Entries;
	std::map<std::pair<const Sprite*, i32>, Sprite> Thumbnails;
	const LevelDocument* LastDocument = nullptr;
	str LastTheme, Search;
	Group Category = Group::All;
	i32 FirstRow = 0;
	bool SearchFocused = false;
	ObjectKind SelectedKind = ObjectKind::Inactive;
	i32 SelectedTexture = 0;
	LevelDocument::SceneryOrder SceneryDepth = LevelDocument::SceneryOrder::Natural;
	LevelDocument::AnimalPlacement AnimalDefaults;
	std::vector<Sprite> BrushIcons;
	Sprite Panel, Header, Tab, Tile, Highlight, SearchBar;
};
