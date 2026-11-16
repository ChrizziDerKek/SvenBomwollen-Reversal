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

namespace
{
#include "GameDataUtils.h"
}

GameData::GameData(fpath root)
    : Root(std::filesystem::canonical(root))
{
	if (!std::filesystem::is_directory(root))
		throw std::runtime_error("Data root is not a directory.");
	const auto core = ReadXml(ReadAsset("Sven2/Sven2.xml"));
	for (const auto& section : core.Children)
	{
		if (section.Name == "sounds")
			for (const auto& item : section.Children)
				if (item.Name == "sound")
					Sounds[item.Attribute("name")] = item.Attribute("file");
		if (section.Name != "parameters")
			continue;
		for (const auto& item : section.Children)
		{
			if (item.Name != "parameter")
				continue;
			const str& name = item.Attribute("name");
			const str& rawValue = item.Attribute("value");
			StringParams[name] = rawValue;
			f64 value = 0;
			// Core also contains nonnumeric parameters (e.g. highscore names).
			if (Number(rawValue, value))
				NumericParams[name] = value;
		}
	}
	const auto table = ReadXml(ReadAsset("Sven2/StringTable.xml"));
	for (const auto& section : table.Children)
	{
		if (section.Name != "stringtable")
			continue;
		for (const auto& item : section.Children)
			if (item.Name == "string")
				Strings[item.Attribute("name")] = item.Attribute("value");
	}
	std::vector<fpath> directories;
	for (const auto& entry : std::filesystem::directory_iterator(root))
		if (entry.is_directory() && !EqualWindows(entry.path().filename().native(), L"Sven2"))
			directories.push_back(entry.path().filename());
	std::sort(directories.begin(), directories.end(), [](const fpath& a, const fpath& b)
	          {
        const i32 order = CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE);
        return order == CSTR_LESS_THAN || (order == CSTR_EQUAL && a.native() < b.native()); });
	for (const fpath& directory : directories)
	{
		const wstr expected = directory.native() + L".xml";
		bool found = false;
		for (const auto& entry : std::filesystem::directory_iterator(root / directory))
		{
			if (entry.is_regular_file() && EqualWindows(entry.path().filename().native(), expected))
			{
				found = true;
				break;
			}
		}
		if (!found)
			continue;
		const str descriptor = (directory / fpath(expected)).generic_u8string();
		const auto document = ReadXml(ReadAsset(descriptor));
		for (const auto& package : document.Children)
			if (package.Name == "package")
				Packs.push_back(ReadPack(package, descriptor));
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
	const fpath relative = std::filesystem::u8path(name);
	if (relative.has_root_path())
		throw std::invalid_argument("Asset path must be relative.");
	fpath resolved = Root;
	for (const auto& part : relative)
	{
		const auto& component = part.native();
		if (component.empty() || component == L"." || component == L".." || component.find_first_of(L"<>:\"|?*") != wstr::npos || component.back() == L'.' || component.back() == L' ' || std::any_of(component.begin(), component.end(), [](wchar_t c)
		                                                                                                                                                                                              { return c < 32; }))
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
	const auto length = file.tellg();
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

str GameData::Text(const str& key) const
{
	const auto found = Strings.find(key);
	return found == Strings.end() ? key : found->second;
}

str GameData::SoundPath(const str& name) const
{
	const auto found = Sounds.find(name);
	return found == Sounds.end() ? str{} : found->second;
}

f64 GameData::NumericParam(const str& name, f64 defaultValue) const
{
	const auto found = NumericParams.find(name);
	return found == NumericParams.end() ? defaultValue : found->second;
}

str GameData::StringParam(const str& name, const str& defaultValue) const
{
	const auto found = StringParams.find(name);
	return found == StringParams.end() ? defaultValue : found->second;
}

const TileSetInfo& GameData::TileSet(const LevelPackInfo& pack, const LevelInfo& level) const
{
	for (const auto& set : pack.TileSets)
		if (set.Name == level.TileSet)
			return set;
	throw std::out_of_range("Unknown tileset '" + level.TileSet + "' in " + pack.Descriptor);
}