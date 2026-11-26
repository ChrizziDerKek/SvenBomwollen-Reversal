#pragma once
#include "LevelDocument.h"

class LevelProject
{
public:
	struct Entry
	{
		str Episode;
		// Structural history retains documents and their independent edit history.
		std::shared_ptr<LevelDocument> Document;
		u64 Id;
	};
	LevelProject(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level);
	LevelDocument& Current()
	{
		return *Entries.at(Active).Document;
	}
	const LevelDocument& Current() const
	{
		return *Entries.at(Active).Document;
	}
	const std::vector<Entry>& GetEntries() const
	{
		return Entries;
	}
	size_t GetActive() const
	{
		return Active;
	}
	const str& GetName() const
	{
		return Name;
	}
	void SetName(const str& name);
	void SetEpisode(const str& name);
	void Select(size_t index);
	void Open(const LevelPackInfo& pack);
	void New();
	void AddBlank();
	void Duplicate();
	void Import(const fpath& path);
	bool MoveLevel(bool later);
	void DeleteLevel();
	bool Undo();
	bool Redo();
	bool CanUndo() const
	{
		return !History.empty();
	}
	bool CanRedo() const
	{
		return !Future.empty();
	}
	bool IsDirty() const;
	str Save();

private:
	struct Snapshot
	{
		std::vector<Entry> Entries;
		size_t Active;
		str Name;
	};
	void Remember();
	bool Restore(std::vector<Snapshot>& from, std::vector<Snapshot>& to);
	str Structure() const;
	str UniqueName(GameEdition edition) const;
	LevelInfo NewInfo() const;
	MudGE& Engine;
	LevelPackInfo Source;
	std::vector<Entry> Entries;
	size_t Active = 0;
	u64 Revision = 0;
	u64 NextId = 0;
	std::vector<Snapshot> History, Future;
	str Name, SavedStructure, ProjectId;
	fpath Folder;
	std::vector<fpath> SavedMaps;
};
