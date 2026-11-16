#pragma once
#include "Structures.h"
#include <unordered_map>

class GameData
{
public:
	GameData(fpath root);
	const fpath& GetRoot() const noexcept;
	fpath AssetPath(str name) const;
	bytestream ReadAsset(const str& name) const;
	const std::vector<LevelPackInfo>& GetPacks() const noexcept;
	str Text(const str& key) const;
	str SoundPath(const str& name) const;
	f64 NumericParam(const str& name, f64 defaultValue) const;
	str StringParam(const str& name, const str& defaultValue) const;
	const TileSetInfo& TileSet(const LevelPackInfo& pack, const LevelInfo& level) const;

private:
	fpath Root;
	std::vector<LevelPackInfo> Packs;
	std::unordered_map<str, str> Strings;
	std::unordered_map<str, str> Sounds;
	std::unordered_map<str, f64> NumericParams;
	std::unordered_map<str, str> StringParams;
};