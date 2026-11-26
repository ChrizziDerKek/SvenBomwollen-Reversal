#include "PlacementPalette.h"
#include "EditorGraphics.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace
{
str Lowercase(str value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](u8 c) { return static_cast<char>(std::tolower(c)); });
	return value;
}
bool Contains(RECT bounds, POINT point)
{
	return point.x >= bounds.left && point.x < bounds.right && point.y >= bounds.top && point.y < bounds.bottom;
}
} // namespace

const std::vector<PlacementPalette::TileBrush>& PlacementPalette::GetTileBrushes()
{
	static const std::vector<TileBrush> brushes = {
		{ "Clear terrain", OccupancyCode::Empty, SpawnerCode::None, false },
		{ "Collision", OccupancyCode::Wall, SpawnerCode::None, false },
		{ "Puddle destination", OccupancyCode::Puddle, SpawnerCode::None, false },
		{ "Pond / water entry", OccupancyCode::Water, SpawnerCode::None, false },
		{ "Random distraction spawner", OccupancyCode::Empty, SpawnerCode::Distraction, true },
		{ "Random powerup spawner", OccupancyCode::Empty, SpawnerCode::Powerup, true },
		{ "Random mixed spawner", OccupancyCode::Empty, SpawnerCode::Mixed, true },
		{ "Remove spawner", OccupancyCode::Empty, SpawnerCode::None, true }
	};
	return brushes;
}

PlacementPalette::PlacementPalette()
    : Panel(EditorGraphics::Rectangle(Width, 1, 0xFFEDE4CE, false)), Header(EditorGraphics::Rectangle(Width, HeaderHeight, 0xFFB8D498, false)), Tab(EditorGraphics::Rectangle(Width / GroupCount, TabHeight, 0xFFB8D498, false)), Tile(EditorGraphics::Rectangle(CellWidth - 4, CellHeight - 4, 0xFFD8CCA8, false)), Highlight(EditorGraphics::Rectangle(CellWidth - 4, CellHeight - 4, 0xFFA8C880, false)), SearchBar(EditorGraphics::Rectangle(Width, SearchHeight, 0xFFF7F0DE, false))
{
	for (const TileBrush& brush : GetTileBrushes())
	{
		const u32 color = brush.SpawnerOnly ? 0xFFFFB000 : (brush.Terrain == OccupancyCode::Wall ? 0xFFFF3030 : (brush.Terrain == OccupancyCode::Puddle ? 0xFF00DDDD : 0xFF2266FF));
		BrushIcons.push_back(EditorGraphics::Rectangle(40, 40, color, brush.SpawnerOnly ? brush.Spawner == SpawnerCode::None : brush.Terrain == OccupancyCode::Empty));
	}
}

void PlacementPalette::Refresh(const LevelDocument& document)
{
	if (LastDocument == &document && LastTheme == document.GetInfo().TileSet)
		return;
	Entries.clear();
	Thumbnails.clear();
	LastDocument = &document;
	LastTheme = document.GetInfo().TileSet;
	const TileSetInfo& tiles = document.GetTileSet();
	for (size_t index = 0; index < tiles.Tiles.size(); ++index)
		Entries.push_back({ ObjectKind::Scenery, static_cast<i32>(index), std::to_string(index) + " " + std::filesystem::u8path(tiles.Tiles[index].File).stem().u8string(), Group::Scenery });
	for (const CharacterDefinition& character : GetCharacterDefinitions())
	{
		if (character.Category == CharacterCategory::Effect || !document.Supports(character.Kind))
			continue;
		Entries.push_back({ character.Kind, 0, character.Name, Group::Actors });
	}
	for (const ItemDefinition& item : GetItemDefinitions())
		Entries.push_back({ item.Kind, 0, item.Name, Group::Items });
	for (size_t index = 0; index < GetTileBrushes().size(); ++index)
		Entries.push_back({ ObjectKind::Inactive, 0, GetTileBrushes()[index].Name, Group::Tiles, static_cast<i32>(index) });
	FirstRow = 0;
}

std::vector<size_t> PlacementPalette::Filtered() const
{
	std::vector<size_t> result;
	const str search = Lowercase(Search);
	for (size_t index = 0; index < Entries.size(); ++index)
	{
		const Entry& entry = Entries[index];
		if ((Category == Group::All || Category == entry.Category) && Lowercase(entry.Name).find(search) != str::npos)
			result.push_back(index);
	}
	return result;
}

i32 PlacementPalette::VisibleRows(RECT bounds)
{
	return std::max(1, (static_cast<i32>(bounds.bottom - bounds.top) - HeaderHeight - TabHeight - SearchHeight - FooterHeight) / CellHeight);
}

PlacementPalette::Result PlacementPalette::Handle(const WindowInput::Result& input, const LevelDocument& document, RECT bounds)
{
	Refresh(document);
	Result result;
	const bool inside = Contains(bounds, input.Mouse);
	if (SearchFocused && !input.Click)
	{
		if (input.Back || input.Editor)
		{
			SearchFocused = false;
			return { true, {} };
		}
		const str before = Search;
		if (input.Control && input.BindingKey == 'a')
			Search.clear();
		if (input.Erase && !Search.empty())
		{
			size_t end = Search.size() - 1;
			while (end > 0 && (static_cast<u8>(Search[end]) & 0xC0) == 0x80)
				--end;
			Search.resize(end);
		}
		if (!input.Control && !input.Text.empty() && Search.size() + input.Text.size() <= 256)
			Search += input.Text;
		if (Search != before)
			FirstRow = 0;
		if (input.Enter)
		{
			const std::vector<size_t> entries = Filtered();
			if (!entries.empty())
				result.Selected = Entries[entries.front()];
			SearchFocused = false;
		}
		result.Consumed = !input.SaveEditor && !input.Playtest;
		if (input.Wheel == 0)
			return result;
	}
	if (!inside)
	{
		if (input.Click)
			SearchFocused = false;
		return result;
	}
	const i32 top = bounds.top + HeaderHeight;
	if (input.Click)
	{
		SearchFocused = !IsProperties() && input.Mouse.y >= top + TabHeight && input.Mouse.y < top + TabHeight + SearchHeight;
		if (input.Mouse.y >= top && input.Mouse.y < top + TabHeight)
		{
			Category = static_cast<Group>(std::min(GroupCount - 1, static_cast<i32>(input.Mouse.x - bounds.left) / (Width / GroupCount)));
			Search.clear();
			FirstRow = 0;
		}
	}
	if (IsProperties())
		return { input.Click || input.RightClick || input.Wheel != 0, {} };
	const std::vector<size_t> entries = Filtered();
	const i32 rows = VisibleRows(bounds);
	const i32 totalRows = (static_cast<i32>(entries.size()) + Columns - 1) / Columns;
	FirstRow = std::clamp(FirstRow - input.Wheel, 0, std::max(0, totalRows - rows));
	const i32 gridTop = top + TabHeight + SearchHeight;
	if (input.Click && input.Mouse.y >= gridTop && input.Mouse.y < gridTop + rows * CellHeight)
	{
		const size_t index = (FirstRow + (input.Mouse.y - gridTop) / CellHeight) * Columns + std::min(Columns - 1, static_cast<i32>(input.Mouse.x - bounds.left) / CellWidth);
		if (index < entries.size())
			result.Selected = Entries[entries[index]];
	}
	result.Consumed = input.Click || input.RightClick || input.Wheel != 0 || result.Consumed;
	return result;
}

Sprite PlacementPalette::MakeThumbnail(const Sprite& source, i32 frame)
{
	if (frame < 0 || frame >= source.Rows * source.Columns)
		throw std::out_of_range("Thumbnail frame is outside the sprite sheet.");
	i32 left = source.FrameWidth, right = -1, top = source.FrameHeight, bottom = -1;
	const i32 sourceX = frame % source.Columns * source.FrameWidth, sourceY = frame / source.Columns * source.FrameHeight;
	for (i32 y = 0; y < source.FrameHeight; ++y)
	{
		for (i32 x = 0; x < source.FrameWidth; ++x)
		{
			if (source.Keyed && (source.Pixels[(sourceY + y) * source.Width + sourceX + x] & 0xFFFFFF) == 0xFF00FF)
				continue;
			left = std::min(left, x);
			right = std::max(right, x);
			top = std::min(top, y);
			bottom = std::max(bottom, y);
		}
	}
	if (right < left)
		return EditorGraphics::Rectangle(1, 1, 0xFFFF00FF, false);
	const i32 width = right - left + 1, height = bottom - top + 1;
	const f32 scale = std::min({ 1.0f, static_cast<f32>(ThumbnailSize) / width, static_cast<f32>(ThumbnailSize) / height });
	const i32 outputWidth = std::max(1, static_cast<i32>(std::round(width * scale))), outputHeight = std::max(1, static_cast<i32>(std::round(height * scale)));
	Sprite thumbnail = EditorGraphics::Rectangle(outputWidth, outputHeight, 0xFFFF00FF, false);
	for (i32 y = 0; y < outputHeight; ++y)
	{
		for (i32 x = 0; x < outputWidth; ++x)
		{
			const f32 sx = left + (x + .5f) * width / outputWidth - .5f, sy = top + (y + .5f) * height / outputHeight - .5f;
			const i32 ix = static_cast<i32>(std::floor(sx)), iy = static_cast<i32>(std::floor(sy));
			const f32 fx = sx - ix, fy = sy - iy;
			f32 weight = 0, red = 0, green = 0, blue = 0;
			for (i32 dy = 0; dy < 2; ++dy)
			{
				for (i32 dx = 0; dx < 2; ++dx)
				{
					const i32 px = std::clamp(ix + dx, left, right), py = std::clamp(iy + dy, top, bottom);
					const u32 color = source.Pixels[(sourceY + py) * source.Width + sourceX + px];
					if (source.Keyed && (color & 0xFFFFFF) == 0xFF00FF)
						continue;
					const f32 amount = (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
					weight += amount;
					red += ((color >> 16) & 255) * amount;
					green += ((color >> 8) & 255) * amount;
					blue += (color & 255) * amount;
				}
			}
			if (weight >= .5f)
				thumbnail.Pixels[y * outputWidth + x] = 0xFF000000 | (static_cast<u32>(std::round(red / weight)) << 16) | (static_cast<u32>(std::round(green / weight)) << 8) | static_cast<u32>(std::round(blue / weight));
		}
	}
	return thumbnail;
}

const Sprite* PlacementPalette::Thumbnail(const LevelDocument& document, const Entry& entry)
{
	if (entry.Brush >= 0)
		return &BrushIcons.at(entry.Brush);
	const SpriteDraw draw = document.PlacementPreview(entry.Kind, LevelDocument::TileAt(20, 20), entry.Texture);
	const auto key = std::make_pair(draw.Sprite, draw.Frame);
	auto found = Thumbnails.find(key);
	if (found == Thumbnails.end())
	{
		Sprite image = MakeThumbnail(*draw.Sprite, draw.Frame);
		for (i32 frame = 0; image.FrameWidth == 1 && image.Pixels.front() == 0xFFFF00FF && frame < draw.Sprite->Rows * draw.Sprite->Columns; ++frame)
			image = MakeThumbnail(*draw.Sprite, frame);
		found = Thumbnails.emplace(key, std::move(image)).first;
	}
	return &found->second;
}

void PlacementPalette::Append(std::vector<SpriteDraw>& commands, const LevelDocument& document, RECT bounds, const BitmapFont& font)
{
	Refresh(document);
	if (Panel.FrameHeight != bounds.bottom - bounds.top)
		Panel = EditorGraphics::Rectangle(Width, bounds.bottom - bounds.top, 0xFFEDE4CE, false);
	commands.push_back({ &Panel, bounds.left, bounds.top, 0 });
	commands.push_back({ &Header, bounds.left, bounds.top, 0 });
	font.Append(commands, "Editor - drag to move", bounds.left + 6, bounds.top + 4);
	font.Append(commands, "Grid", bounds.left + GridX, bounds.top + 4);
	constexpr cstr groups[] = { "All", "Scenery", "Actors", "Items", "Tiles", "Props" };
	commands.push_back({ &Tab, bounds.left + static_cast<i32>(Category) * (Width / GroupCount), bounds.top + HeaderHeight, 0 });
	for (i32 index = 0; index < GroupCount; ++index)
		font.Append(commands, groups[index], bounds.left + index * (Width / GroupCount) + 1, bounds.top + HeaderHeight + 4);
	if (IsProperties())
		return;
	commands.push_back({ &SearchBar, bounds.left, bounds.top + HeaderHeight + TabHeight, 0 });
	font.Append(commands, EditorGraphics::FitText(font, "Find: " + Search + (SearchFocused ? "_" : ""), Width - 12), bounds.left + 6, bounds.top + HeaderHeight + TabHeight + 4);
	const std::vector<size_t> entries = Filtered();
	const i32 rows = VisibleRows(bounds), gridTop = bounds.top + HeaderHeight + TabHeight + SearchHeight;
	FirstRow = std::clamp(FirstRow, 0, std::max(0, (static_cast<i32>(entries.size()) + Columns - 1) / Columns - rows));
	for (i32 cell = 0; cell < rows * Columns; ++cell)
	{
		const size_t index = FirstRow * Columns + cell;
		if (index >= entries.size())
			break;
		const Entry& entry = Entries[entries[index]];
		const i32 x = bounds.left + cell % Columns * CellWidth, y = gridTop + cell / Columns * CellHeight;
		const bool selected = entry.Brush >= 0 ? SelectedKind == ObjectKind::Inactive && entry.Brush == SelectedTexture : entry.Kind == SelectedKind && (entry.Kind != ObjectKind::Scenery || entry.Texture == SelectedTexture);
		commands.push_back({ selected ? &Highlight : &Tile, x + 2, y + 2, 0 });
		const Sprite* thumbnail = Thumbnail(document, entry);
		commands.push_back({ thumbnail, x + (CellWidth - thumbnail->FrameWidth) / 2, y + 4 + (entry.Brush >= 0 ? 0 : (ThumbnailSize - thumbnail->FrameHeight) / 2), 0 });
		str label = entry.Name;
		if (entry.Brush >= 0)
		{
			const TileBrush& brush = GetTileBrushes()[entry.Brush];
			if (brush.SpawnerOnly && IsSpawner(brush.Spawner))
			{
				label.erase(0, str("Random ").size());
				font.Append(commands, brush.Spawner == SpawnerCode::Distraction ? "D" : (brush.Spawner == SpawnerCode::Powerup ? "P" : "M"), x + CellWidth / 2 - 4, y + 16);
			}
		}
		if (entry.Brush >= 0)
		{
			size_t split = label.rfind(' ');
			while (split != str::npos && font.Width(label.substr(0, split)) > CellWidth - 8)
				split = label.rfind(' ', split - 1);
			font.Append(commands, EditorGraphics::FitText(font, label.substr(0, split), CellWidth - 8), x + 4, y + 50);
			if (split != str::npos)
				font.Append(commands, EditorGraphics::FitText(font, label.substr(split + 1), CellWidth - 8), x + 4, y + 66);
		}
		else
			font.Append(commands, EditorGraphics::FitText(font, label, CellWidth - 8), x + 4, y + ThumbnailSize + 7);
	}
	font.Append(commands, std::to_string(entries.size()) + " choices | wheel scrolls", bounds.left + 6, bounds.bottom - FooterHeight + 4);
}

void PlacementPalette::Select(ObjectKind kind, i32 texture)
{
	SelectedKind = kind;
	SelectedTexture = texture;
}

void PlacementPalette::Reset()
{
	LastDocument = nullptr;
	Search.clear();
	FirstRow = 0;
	SearchFocused = false;
}

void PlacementPalette::Show(Group group)
{
	Category = group;
	Search.clear();
	FirstRow = 0;
	SearchFocused = false;
}
