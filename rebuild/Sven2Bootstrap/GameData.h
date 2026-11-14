#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace sven2 {

struct LevelInfo {
    std::string name, map, tileset, music;
};

struct LevelSetInfo {
    std::string name;
    std::vector<LevelInfo> levels;
};

struct TileInfo {
    std::string file;
    short frames = 1;
};

struct TileSetInfo {
    std::string name, background;
    std::vector<TileInfo> tiles;
};

struct LevelPackInfo {
    std::string name, descriptor;
    std::vector<LevelSetInfo> levelsets;
    std::vector<TileSetInfo> tilesets;
};

// Folder data only. All narrow strings (including asset names) are UTF-8.
// Construction loads core tables and immediate-child package descriptors.
// I/O, invalid XML/schema, and invalid asset paths throw standard exceptions.
class GameData {
public:
    explicit GameData(std::filesystem::path root);
    const std::filesystem::path& root() const noexcept;
    // Requires an existing asset; resolves Windows casing and checks containment.
    std::filesystem::path assetPath(std::string name) const;
    std::vector<unsigned char> readAsset(const std::string& name) const;
    const std::vector<LevelPackInfo>& packs() const noexcept;
    std::string text(const std::string& key) const;
    std::string soundPath(const std::string& name) const;
    double parameter(const std::string& name, double defaultValue) const;
    std::string parameterString(const std::string& name, const std::string& defaultValue) const;
    const TileSetInfo& tileset(const LevelPackInfo& pack, const LevelInfo& level) const;

private:
    std::filesystem::path root_;
    std::vector<LevelPackInfo> packs_;
    std::unordered_map<std::string, std::string> strings_;
    std::unordered_map<std::string, std::string> sounds_;
    std::unordered_map<std::string, double> parameters_;
    std::unordered_map<std::string, std::string> parameterStrings_;
};

} // namespace sven2
