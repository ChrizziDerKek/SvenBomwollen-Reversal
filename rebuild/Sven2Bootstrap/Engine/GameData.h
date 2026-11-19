#pragma once
#include "Types.h"
#include "Structures.h"
#include <unordered_map>
#include <map>

class GameData
{
public:
	GameData(fpath root);
	const fpath& GetRoot() const noexcept;
	fpath AssetPath(str name) const;
	bytestream ReadAsset(const str& name) const;
	const std::vector<LevelPackInfo>& GetPacks() const noexcept;
	str Text(const str& key, GameEdition edition = GameEdition::Zwo) const;
	str SoundPath(const str& name, GameEdition edition = GameEdition::Zwo) const;
	f64 NumericParam(const str& name, f64 defaultValue, GameEdition edition = GameEdition::Zwo) const;
	str StringParam(const str& name, const str& defaultValue, GameEdition edition = GameEdition::Zwo) const;
	bool HasEdition(GameEdition edition) const;
	GameEdition GetThemeEdition() const;
	GameEdition GetGameplayEdition() const;
	str EditionAsset(GameEdition edition, str path) const;
	str ThemeAsset(const str& path) const;
	struct SpriteDefinition
	{
		str File;
		i16 Rows = 1;
		i16 Columns = 1;
	};
	const SpriteDefinition& GetSpriteDefinition(const str& name, GameEdition edition) const;
	const TileSetInfo& TileSet(const LevelPackInfo& pack, const LevelInfo& level) const;

private:
	fpath Root;
	std::vector<LevelPackInfo> Packs;
	struct EditionResources
	{
		std::unordered_map<str, str> Strings;
		std::unordered_map<str, str> Sounds;
		std::unordered_map<str, f64> NumericParams;
		std::unordered_map<str, str> StringParams;
		std::unordered_map<str, SpriteDefinition> Sprites;
	};
	void LoadEdition(GameEdition edition, const str& coreDirectory, const str& prefix);
	std::map<GameEdition, EditionResources> Editions;
};
