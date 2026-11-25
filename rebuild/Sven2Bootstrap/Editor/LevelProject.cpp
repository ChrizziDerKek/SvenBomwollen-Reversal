#include "LevelProject.h"
#include "EditorFiles.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace
{
void ValidateName(const str& name)
{
	if (name.empty() || name.size() > 128 || std::any_of(name.begin(), name.end(), [](u8 c) { return c < 32; }))
		throw std::runtime_error("Use a nonempty name of at most 128 UTF-8 bytes without control characters.");
}
} // namespace

LevelProject::LevelProject(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level)
    : Engine(engine), Source(pack), Name(UniqueName(pack.Edition))
{
	Entries.push_back({ "Edited", std::make_unique<LevelDocument>(engine, pack, level) });
	SavedStructure = Structure();
}

str LevelProject::UniqueName(GameEdition edition) const
{
	const GameData catalog(Engine.GetData().GetRoot());
	for (i32 index = 1;; ++index)
	{
		const str name = "Custom " + std::to_string(index);
		if (std::none_of(catalog.GetPacks().begin(), catalog.GetPacks().end(), [&](const LevelPackInfo& pack) { return pack.Edition == edition && _stricmp(pack.Name.c_str(), name.c_str()) == 0; }))
			return name;
	}
}

str LevelProject::Structure() const
{
	str result = Name + '\n';
	for (const Entry& entry : Entries)
		result += entry.Episode + '\n';
	return result;
}

bool LevelProject::IsDirty() const
{
	return Structure() != SavedStructure || std::any_of(Entries.begin(), Entries.end(), [](const Entry& entry) { return entry.Document->IsDirty(); });
}

void LevelProject::SetName(const str& name)
{
	ValidateName(name);
	// Pack IDs also identify portable highscore files.
	if (name.find_first_of("<>:\"/\\|?*") != str::npos)
		throw std::runtime_error("Pack names cannot contain filename separators or reserved filename characters.");
	Name = name;
}

void LevelProject::SetEpisode(const str& name)
{
	ValidateName(name);
	Entries.at(Active).Episode = name;
}

void LevelProject::Select(size_t index)
{
	if (index >= Entries.size())
		throw std::out_of_range("Project level index is out of range.");
	Active = index;
}

LevelInfo LevelProject::NewInfo() const
{
	LevelInfo info = Current().GetInfo();
	info.Name = "Level " + std::to_string(Entries.size() + 1);
	info.Map = Source.AssetPrefix + "_EditorUnsaved/level.lvl";
	return info;
}

void LevelProject::Open(const LevelPackInfo& pack)
{
	std::vector<Entry> entries;
	for (const LevelSetInfo& episode : pack.LevelSets)
	{
		for (const LevelInfo& level : episode.Levels)
			entries.push_back({ episode.Name, std::make_unique<LevelDocument>(Engine, pack, level) });
	}
	if (entries.empty())
		throw std::runtime_error("That pack has no registered levels.");
	const str name = UniqueName(pack.Edition);
	Source = pack;
	Name = name;
	Entries = std::move(entries);
	Active = 0;
	Folder.clear();
	ProjectId.clear();
	SavedMaps.clear();
	SavedStructure = Structure();
}

void LevelProject::New()
{
	const LevelPackInfo pack = Current().GetPack();
	LevelInfo info = NewInfo();
	info.Name = "Level 1";
	std::unique_ptr<LevelDocument> document = LevelDocument::Blank(Engine, pack, info);
	const str name = UniqueName(Source.Edition);
	Entries.clear();
	Entries.push_back({ "My levels", std::move(document) });
	Active = 0;
	Name = name;
	Folder.clear();
	ProjectId.clear();
	SavedMaps.clear();
	SavedStructure.clear();
}

void LevelProject::AddBlank()
{
	const str episode = Entries.at(Active).Episode;
	std::unique_ptr<LevelDocument> document = LevelDocument::Blank(Engine, Current().GetPack(), NewInfo());
	Entries.push_back({ episode, std::move(document) });
	Active = Entries.size() - 1;
}

void LevelProject::Duplicate()
{
	LevelInfo info = NewInfo();
	info.Name = Current().GetInfo().Name + " copy";
	const str episode = Entries.at(Active).Episode;
	std::unique_ptr<LevelDocument> document = std::make_unique<LevelDocument>(Engine, Current().GetPack(), info, Current().GetBytes());
	Entries.push_back({ episode, std::move(document) });
	Active = Entries.size() - 1;
}

void LevelProject::Import(const fpath& path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	const std::streampos size = file ? file.tellg() : std::streampos{ -1 };
	constexpr size_t MaxFileBytes = 16 + 4096 + 9048 + 256 * 100 + 4096;
	if (size < 0 || static_cast<u64>(size) > MaxFileBytes)
		throw std::runtime_error("Cannot read this v6/v7 level file.");
	bytestream bytes(static_cast<size_t>(size));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
		throw std::runtime_error("Cannot read the imported level.");
	LevelInfo info = NewInfo();
	info.Name = path.stem().u8string();
	std::unique_ptr<LevelDocument> document = std::make_unique<LevelDocument>(Engine, Current().GetPack(), info, std::move(bytes));
	if (document->GetLevel().GetVersion() != Current().GetLevel().GetVersion())
		throw std::runtime_error("Import must use the project's v6/v7 format. Conversion is a separate operation.");
	size_t players = 0;
	for (const LevelObjectRecord& record : document->GetLevel().GetObjects())
	{
		if (record.Kind() == static_cast<i32>(ObjectKind::Inactive))
			continue;
		players += record.Kind() == static_cast<i32>(ObjectKind::Player);
		if (!document->GetSprite(record))
			throw std::runtime_error("The imported level contains an unsupported object.");
	}
	if (players < 1 || players > (Source.Edition == GameEdition::Zwo ? size_t{ 1 } : size_t{ 2 }))
		throw std::runtime_error("The imported level has invalid player starts.");
	const str episode = Entries.at(Active).Episode;
	Entries.push_back({ episode, std::move(document) });
	Active = Entries.size() - 1;
}

str LevelProject::Save()
{
	const GameData catalog(Engine.GetData().GetRoot());
	for (const LevelPackInfo& pack : catalog.GetPacks())
		if (pack.Edition == Source.Edition && _stricmp(pack.Name.c_str(), Name.c_str()) == 0 && pack.Descriptor != Source.AssetPrefix + ProjectId + "/" + ProjectId + ".xml")
			throw std::runtime_error("A pack with that name already exists. Choose another project name.");
	if (Folder.empty())
	{
		for (i32 index = 1;; ++index)
		{
			const str id = "Editor_" + std::to_string(index);
			const fpath folder = EditorFiles::OutputPath(Engine.GetData(), Source.AssetPrefix + id);
			if (!std::filesystem::create_directory(folder))
				continue;
			Folder = folder;
			ProjectId = id;
			break;
		}
	}
	const str generation = "levels/rev_" + std::to_string(GetTickCount64()) + "_" + std::to_string(++Revision);
	std::vector<LevelInfo> levels;
	std::vector<fpath> written;
	for (size_t index = 0; index < Entries.size(); ++index)
	{
		const str map = Source.AssetPrefix + ProjectId + "/" + generation + "/level_" + std::to_string(index + 1) + ".lvl";
		levels.push_back(Entries[index].Document->WritePreview(map));
		levels.back().TileSet = Entries[index].Document->GetInfo().TileSet + " #" + std::to_string(index + 1);
		written.push_back(EditorFiles::OutputPath(Engine.GetData(), map));
	}
	auto local = [&](const str& asset) { return asset.rfind(Source.AssetPrefix, 0) == 0 ? asset.substr(Source.AssetPrefix.size()) : asset; };
	const cstr root = Source.Edition == GameEdition::XXX ? "SvenXXX" : "sven2";
	std::ostringstream xml;
	xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><" << root << " version=\"1.0\"><package name=\"" << EditorFiles::Xml(Name) << "\"><levelsets>";
	std::vector<str> episodes;
	for (const Entry& entry : Entries)
		if (std::find(episodes.begin(), episodes.end(), entry.Episode) == episodes.end())
			episodes.push_back(entry.Episode);
	for (const str& episode : episodes)
	{
		xml << "<levelset name=\"" << EditorFiles::Xml(episode) << "\">";
		for (size_t index = 0; index < Entries.size(); ++index)
		{
			if (Entries[index].Episode != episode)
				continue;
			const LevelInfo& info = levels[index];
			xml << "<level name=\"" << EditorFiles::Xml(info.Name) << "\" tileset=\"" << EditorFiles::Xml(info.TileSet) << "\" music=\"" << EditorFiles::Xml(local(info.Music)) << "\" map=\"" << EditorFiles::Xml(local(info.Map)) << "\"/>";
		}
		xml << "</levelset>";
	}
	xml << "</levelsets><tilesets>";
	for (size_t index = 0; index < Entries.size(); ++index)
	{
		const LevelDocument& document = *Entries[index].Document;
		const TileSetInfo& tiles = Engine.GetData().TileSet(document.GetPack(), document.GetInfo());
		xml << "<tileset name=\"" << EditorFiles::Xml(levels[index].TileSet) << "\" background=\"" << EditorFiles::Xml(local(tiles.Background)) << "\">";
		for (const TileInfo& tile : tiles.Tiles)
			xml << "<tile file=\"" << EditorFiles::Xml(local(tile.File)) << "\" frames=\"" << tile.Frames << "\"/>";
		xml << "</tileset>";
	}
	xml << "</tilesets></package></" << root << '>';
	const str descriptor = xml.str();
	EditorFiles::AtomicWrite(Folder / std::filesystem::u8path(ProjectId + ".xml"), bytestream(descriptor.begin(), descriptor.end()));
	for (const fpath& old : SavedMaps)
	{
		std::error_code error;
		std::filesystem::remove(old, error);
		std::filesystem::remove(old.parent_path(), error);
	}
	SavedMaps = std::move(written);
	for (Entry& entry : Entries)
		entry.Document->MarkSaved();
	SavedStructure = Structure();
	return Source.AssetPrefix + ProjectId + "/" + ProjectId + ".xml";
}
