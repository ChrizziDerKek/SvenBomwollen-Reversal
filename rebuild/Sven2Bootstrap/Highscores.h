#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace sven2 {
struct HighscoreEntry { std::string name; int score; int level=1; int sheep=0; };
class Highscores {
public:
    // Caller supplies a user-data .tsv path. Parent directories are created on save.
    // Format: UTF-8 without BOM, score<TAB>name<TAB>level<TAB>sheep<LF>.
    // Earlier two- and three-column remake tables remain readable.
    // Defaults and persisted entries allow level 0 for original baseline scores.
    // Missing file uses defaults; malformed files and I/O failures throw.
    explicit Highscores(std::filesystem::path file, std::vector<HighscoreEntry> defaults={});
    const std::vector<HighscoreEntry>& entries() const noexcept { return entries_; }
    // Negative scores never qualify; a full table requires strictly > bottom.
    bool qualifies(int score) const noexcept;
    // Names: 1..20 Unicode scalars, no controls; scores >= 0; submitted level >= 1.
    // Invalid input throws.
    // Stable descending top 10: existing tied entries precede a new submission.
    // Saves synchronously, even if outside top 10; failure preserves entries().
    void submit(const std::string& name, int score, int level=1, int sheep=0);
private:
    std::filesystem::path file_;
    std::vector<HighscoreEntry> entries_;
    void save(const std::vector<HighscoreEntry>& entries) const;
};

}
