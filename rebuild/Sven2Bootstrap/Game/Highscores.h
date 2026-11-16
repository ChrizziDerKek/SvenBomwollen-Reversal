#pragma once
#include "Types.h"
#include <filesystem>
#include <string>
#include <vector>

struct HighscoreEntry
{
	str Name;
	i32 Score;
	i32 Level = 1;
	i32 Sheep = 0;
};
class Highscores
{
public:
	// Caller supplies a user-data .tsv path. Parent directories are created on save.
	// Format: UTF-8 without BOM, score<TAB>name<TAB>level<TAB>sheep<LF>.
	// Earlier two- and three-column remake tables remain readable.
	// Defaults and persisted entries allow level 0 for original baseline scores.
	// Missing file uses defaults; malformed files and I/O failures throw.
	explicit Highscores(fpath file, std::vector<HighscoreEntry> defaults = {});
	const std::vector<HighscoreEntry>& GetEntries() const noexcept
	{
		return Entries;
	}
	// Negative scores never qualify; a full table requires strictly > bottom.
	bool Qualifies(i32 score) const noexcept;
	// Names: 1..20 Unicode scalars, no controls; scores >= 0; submitted level >= 1.
	// Invalid input throws.
	// Stable descending top 10: existing tied entries precede a new submission.
	// Saves synchronously, even if outside top 10; failure preserves entries().
	void Submit(const str& name, i32 score, i32 level = 1, i32 sheep = 0);

private:
	fpath File;
	std::vector<HighscoreEntry> Entries;
	void Save(const std::vector<HighscoreEntry>& entries) const;
};
