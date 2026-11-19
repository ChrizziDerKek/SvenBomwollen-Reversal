#include "GameData.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <xmllite.h>
#include <shlwapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

#pragma comment(lib, "xmllite.lib")
#pragma comment(lib, "shlwapi.lib")

#include "GameDataUtils.h"

GameData::GameData(fpath root)
    : Root(std::filesystem::canonical(root))
{
	if (!std::filesystem::is_directory(root))
		throw std::runtime_error("Data root is not a directory.");
	LoadEdition(GameEdition::Zwo, "Sven2", std::filesystem::is_directory(Root / L"ZWO") ? "ZWO/" : "");
	if (std::filesystem::is_directory(Root / L"XXX" / L"SvenXXX"))
		LoadEdition(GameEdition::XXX, "SvenXXX", "XXX/");
}

void GameData::LoadEdition(GameEdition edition, const str& coreDirectory, const str& prefix)
{
	EditionResources& resources = Editions[edition];
	const Node core = ReadXml(ReadAsset(prefix + coreDirectory + "/" + coreDirectory + ".xml"));
	for (const Node& section : core.Children)
	{
		for (const Node& item : section.Children)
		{
			if (section.Name == "sounds" && item.Name == "sound")
				resources.Sounds[item.Attribute("name")] = prefix + item.Attribute("file");
			else if (section.Name == "parameters" && item.Name == "parameter")
			{
				const str& name = item.Attribute("name");
				const str& rawValue = item.Attribute("value");
				resources.StringParams[name] = rawValue;
				f64 value = 0;
				if (Number(rawValue, value))
					resources.NumericParams[name] = value;
			}
			else if (section.Name == "sprites" && item.Name == "sprite")
			{
				str file = prefix + section.Attribute("path") + item.Attribute("file");
				std::replace(file.begin(), file.end(), '\\', '/');
				f64 rows = 0, columns = 0;
				if (!Number(item.Attribute("rows"), rows) || !Number(item.Attribute("cols"), columns) || rows < 1 || columns < 1 || rows > SHRT_MAX || columns > SHRT_MAX || std::floor(rows) != rows || std::floor(columns) != columns)
					throw std::runtime_error("Invalid core sprite dimensions: " + item.Attribute("name"));
				resources.Sprites[item.Attribute("name")] = { file, static_cast<i16>(rows), static_cast<i16>(columns) };
			}
		}
	}
	const Node table = ReadXml(ReadAsset(prefix + coreDirectory + "/StringTable.xml"));
	for (const Node& section : table.Children)
		if (section.Name == "stringtable")
			for (const Node& item : section.Children)
				if (item.Name == "string")
					resources.Strings[item.Attribute("name")] = item.Attribute("value");
	const fpath catalog = Root / std::filesystem::u8path(prefix);
	std::vector<fpath> directories;
	for (const auto& entry : std::filesystem::directory_iterator(catalog))
		if (entry.is_directory() && !EqualWindows(entry.path().filename().native(), std::filesystem::u8path(coreDirectory).native()))
			directories.push_back(entry.path().filename());
	std::sort(directories.begin(), directories.end(), [](const fpath& a, const fpath& b) { const i32 order = CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE); return order == CSTR_LESS_THAN || (order == CSTR_EQUAL && a.native() < b.native()); });
	for (const fpath& directory : directories)
	{
		const wstr expected = directory.native() + L".xml";
		for (const auto& entry : std::filesystem::directory_iterator(catalog / directory))
		{
			if (entry.is_regular_file() && EqualWindows(entry.path().filename().native(), expected))
			{
				const str descriptor = prefix + (directory / fpath(expected)).generic_u8string();
				const Node document = ReadXml(ReadAsset(descriptor));
				for (const Node& package : document.Children)
				{
					if (package.Name == "package")
					{
						LevelPackInfo pack = ReadPack(package, descriptor);
						pack.Edition = edition;
						pack.AssetPrefix = prefix;
						pack.Id = (edition == GameEdition::XXX ? "XXX-" : "") + pack.Name;
						for (LevelSetInfo& set : pack.LevelSets)
						{
							for (LevelInfo& level : set.Levels)
							{
								level.Map = prefix + level.Map;
								if (!level.Music.empty())
									level.Music = prefix + level.Music;
							}
						}
						for (TileSetInfo& set : pack.TileSets)
						{
							set.Background = prefix + set.Background;
							for (TileInfo& tile : set.Tiles)
								tile.File = prefix + tile.File;
						}
						Packs.push_back(std::move(pack));
					}
				}
				break;
			}
		}
	}
}

const fpath& GameData::GetRoot() const noexcept
{
	return Root;
}

fpath GameData::AssetPath(str name) const
{
	if (name.empty() || name.find('\0') != str::npos)
		throw std::invalid_argument("Empty or invalid asset path.");
	// Legacy Zwo callers and editor fixtures retain paths relative to that edition.
	std::replace(name.begin(), name.end(), '\\', '/');
	if (std::filesystem::is_directory(Root / L"ZWO") && name.rfind("ZWO/", 0) != 0 && name.rfind("XXX/", 0) != 0)
		name = "ZWO/" + name;
	const fpath relative = std::filesystem::u8path(name);
	if (relative.has_root_path())
		throw std::invalid_argument("Asset path must be relative.");
	fpath resolved = Root;
	for (const fpath& part : relative)
	{
		const wstr& component = part.native();
		if (component.empty() || component == L"." || component == L".." || component.find_first_of(L"<>:\"|?*") != wstr::npos || component.back() == L'.' || component.back() == L' ' || std::any_of(component.begin(), component.end(), [](wchar_t c) { return c < 32; }))
			throw std::invalid_argument("Invalid relative asset component.");
		bool found = false;
		for (const auto& entry : std::filesystem::directory_iterator(resolved))
		{
			if (!EqualWindows(entry.path().filename().native(), component))
				continue;
			if (found)
				throw std::runtime_error("Ambiguous asset path: " + name);
			resolved = entry.path();
			found = true;
		}
		if (!found)
			throw std::runtime_error("Asset not found: " + name);
		resolved = std::filesystem::canonical(resolved);
		if (!Contained(Root, resolved))
			throw std::invalid_argument("Asset escapes data root.");
	}
	if (!std::filesystem::is_regular_file(resolved))
		throw std::runtime_error("Asset is not a file: " + name);
	return resolved;
}

bytestream GameData::ReadAsset(const str& name) const
{
	std::ifstream file(AssetPath(name), std::ios::binary | std::ios::ate);
	if (!file)
		throw std::runtime_error("Cannot open asset: " + name);
	const std::streampos length = file.tellg();
	if (length < 0 || static_cast<u64>(length) > static_cast<u64>((std::numeric_limits<std::streamsize>::max)()) || static_cast<u64>(length) > (std::numeric_limits<size_t>::max)())
		throw std::runtime_error("Invalid asset size: " + name);
	bytestream bytes(static_cast<size_t>(length));
	file.seekg(0);
	if (!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
		throw std::runtime_error("Cannot read asset: " + name);
	return bytes;
}

const std::vector<LevelPackInfo>& GameData::GetPacks() const noexcept
{
	return Packs;
}

str GameData::Text(const str& key, GameEdition edition) const
{
	const auto& values = Editions.at(edition).Strings;
	const auto found = values.find(key);
	return found == values.end() ? key : found->second;
}

str GameData::SoundPath(const str& name, GameEdition edition) const
{
	const auto& values = Editions.at(edition).Sounds;
	const auto found = values.find(name);
	return found == values.end() ? str{} : found->second;
}

f64 GameData::NumericParam(const str& name, f64 defaultValue, GameEdition edition) const
{
	const auto& values = Editions.at(edition).NumericParams;
	const auto found = values.find(name);
	return found == values.end() ? defaultValue : found->second;
}

str GameData::StringParam(const str& name, const str& defaultValue, GameEdition edition) const
{
	const auto& values = Editions.at(edition).StringParams;
	const auto found = values.find(name);
	return found == values.end() ? defaultValue : found->second;
}

const TileSetInfo& GameData::TileSet(const LevelPackInfo& pack, const LevelInfo& level) const
{
	for (const TileSetInfo& set : pack.TileSets)
		if (_stricmp(set.Name.c_str(), level.TileSet.c_str()) == 0)
			return set;
	throw std::out_of_range("Unknown tileset '" + level.TileSet + "' in " + pack.Descriptor);
}

bool GameData::HasEdition(GameEdition edition) const
{
	return Editions.find(edition) != Editions.end();
}

GameEdition GameData::GetThemeEdition() const
{
	return GetGameplayEdition();
}

GameEdition GameData::GetGameplayEdition() const
{
	// One installed runtime serves all source level formats.
	return HasEdition(GameEdition::XXX) ? GameEdition::XXX : GameEdition::Zwo;
}

str GameData::EditionAsset(GameEdition edition, str path) const
{
	std::replace(path.begin(), path.end(), '\\', '/');
	if (path.rfind("ZWO/", 0) == 0 || path.rfind("XXX/", 0) == 0)
		return path;
	if (edition == GameEdition::Zwo)
		return (std::filesystem::is_directory(Root / L"ZWO") ? "ZWO/" : "") + path;
	if (path.size() >= 6 && _strnicmp(path.c_str(), "Sven2/", 6) == 0)
		path = "SvenXXX/" + path.substr(6);
	return "XXX/" + path;
}

str GameData::ThemeAsset(const str& path) const
{
	return EditionAsset(GetThemeEdition(), path);
}

const GameData::SpriteDefinition& GameData::GetSpriteDefinition(const str& name, GameEdition edition) const
{
	return Editions.at(edition).Sprites.at(name);
}
