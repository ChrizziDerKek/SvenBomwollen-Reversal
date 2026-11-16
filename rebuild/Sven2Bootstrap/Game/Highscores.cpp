#include "Game/Highscores.h"
#include <windows.h>
#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

namespace
{
[[noreturn]] void IoError(cstr operation, DWORD error = GetLastError())
{
	throw std::system_error(static_cast<i32>(error), std::system_category(), operation);
}
struct Handle
{
	HANDLE Value = INVALID_HANDLE_VALUE;
	~Handle()
	{
		if (Value != INVALID_HANDLE_VALUE)
			CloseHandle(Value);
	}
};
void Validate(const HighscoreEntry& entry)
{
	if (entry.Level < 0)
		throw std::invalid_argument("Highscore level must be nonnegative");
	if (entry.Sheep < 0)
		throw std::invalid_argument("Highscore animal count must be nonnegative");
	if (entry.Score < 0 || entry.Name.empty() || entry.Name.size() > 80)
		throw std::invalid_argument("Highscore requires nonnegative score and a name of 1..20 Unicode characters");
	i32 n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, entry.Name.data(), static_cast<i32>(entry.Name.size()), nullptr, 0);
	if (!n)
		throw std::invalid_argument("Highscore name is not valid UTF-8");
	wstr wide(n, L'\0');
	if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, entry.Name.data(), static_cast<i32>(entry.Name.size()), &wide[0], n))
		throw std::invalid_argument("Highscore name conversion failed");
	i32 count = 0;
	for (size_t i = 0; i < wide.size(); ++i)
	{
		u32 c = wide[i];
		if (c < 32 || (c >= 127 && c <= 159) || c == 0x2028 || c == 0x2029)
			throw std::invalid_argument("Highscore names cannot contain control characters or line separators");
		if (c >= 0xd800 && c <= 0xdbff)
			++i;
		++count;
	}
	if (count > 20)
		throw std::invalid_argument("Highscore name exceeds 20 Unicode characters");
}
void Rank(std::vector<HighscoreEntry>& entries)
{
	std::stable_sort(entries.begin(), entries.end(), [](const HighscoreEntry& a, const HighscoreEntry& b)
	                 { return a.Score > b.Score; });
	if (entries.size() > 10)
		entries.resize(10);
}
void SaveTextFile(const fpath& file, const str& data)
{
	std::filesystem::create_directories(file.parent_path());
	// CREATE_NEW prevents collisions and never truncates another writer's temp.
	fpath temp;
	Handle output;
	for (u32 attempt = 0; attempt < 100; ++attempt)
	{
		temp = file;
		temp += L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64()) + L"." + std::to_wstring(attempt);
		output.Value = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (output.Value != INVALID_HANDLE_VALUE)
			break;
		DWORD error = GetLastError();
		if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
			IoError("Create highscore temporary file", error);
	}
	if (output.Value == INVALID_HANDLE_VALUE)
		IoError("Highscore temporary file collisions", ERROR_FILE_EXISTS);
	try
	{
		DWORD written = 0;
		if (!WriteFile(output.Value, data.data(), static_cast<DWORD>(data.size()), &written, nullptr))
			IoError("Write highscores");
		if (written != data.size())
			IoError("Short highscore write", ERROR_WRITE_FAULT);
		if (!FlushFileBuffers(output.Value))
			IoError("Flush highscores");
		HANDLE handle = output.Value;
		output.Value = INVALID_HANDLE_VALUE;
		if (!CloseHandle(handle))
			IoError("Close highscore temporary file");
		// Same-directory rename is atomic; no COPY_ALLOWED fallback across volumes.
		if (!MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			IoError("Replace highscores");
	}
	catch (...)
	{
		if (output.Value != INVALID_HANDLE_VALUE)
		{
			CloseHandle(output.Value);
			output.Value = INVALID_HANDLE_VALUE;
		}
		DeleteFileW(temp.c_str());
		throw;
	}
}

} // namespace
Highscores::Highscores(fpath file, std::vector<HighscoreEntry> defaults)
    : File(std::filesystem::absolute(std::move(file)))
{
	Handle input;
	input.Value = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (input.Value == INVALID_HANDLE_VALUE)
	{
		DWORD error = GetLastError();
		if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
			IoError("Open highscores", error);
		for (const HighscoreEntry& entry : defaults)
			Validate(entry);
		Rank(defaults);
		Entries = std::move(defaults);
		return;
	}
	LARGE_INTEGER size{};
	if (!GetFileSizeEx(input.Value, &size))
		IoError("Read highscore size");
	if (size.QuadPart < 0 || size.QuadPart > 4096)
		throw std::runtime_error("Corrupt highscores: file exceeds 4096 bytes");
	str data(static_cast<size_t>(size.QuadPart), '\0');
	DWORD read = 0;
	if (!ReadFile(input.Value, data.data(), static_cast<DWORD>(data.size()), &read, nullptr))
		IoError("Read highscores");
	if (read != data.size())
		throw std::runtime_error("Incomplete highscore read");
	try
	{
		size_t start = 0;
		while (start < data.size())
		{
			size_t end = data.find('\n', start);
			if (end == str::npos)
				end = data.size();
			str line = data.substr(start, end - start);
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			size_t tab = line.find('\t');
			if (tab == str::npos || tab == 0)
				throw std::invalid_argument("Expected score TAB name");
			i32 score = 0;
			cstr first = line.data();
			cstr last = first + tab;
			std::from_chars_result parsed = std::from_chars(first, last, score);
			if (parsed.ec != std::errc() || parsed.ptr != last || line[0] < '0' || line[0] > '9')
				throw std::invalid_argument("Invalid decimal score");
			size_t levelTab = line.find('\t', tab + 1);
			const size_t sheepTab = levelTab == str::npos ? str::npos : line.find('\t', levelTab + 1);
			i32 level = 1;
			if (levelTab != str::npos)
			{
				cstr levelFirst = line.data() + levelTab + 1;
				cstr levelLast = line.data() + (sheepTab == str::npos ? line.size() : sheepTab);
				std::from_chars_result parsedLevel = std::from_chars(levelFirst, levelLast, level);
				if (parsedLevel.ec != std::errc() || parsedLevel.ptr != levelLast || level < 0)
					throw std::invalid_argument("Invalid level");
			}
			i32 sheep = 0;
			if (sheepTab != str::npos)
			{
				cstr firstSheep = line.data() + sheepTab + 1;
				cstr lastSheep = line.data() + line.size();
				std::from_chars_result parsedSheep = std::from_chars(firstSheep, lastSheep, sheep);
				if (parsedSheep.ec != std::errc() || parsedSheep.ptr != lastSheep || sheep < 0)
					throw std::invalid_argument("Invalid animal count");
			}
			HighscoreEntry entry{line.substr(tab + 1, levelTab == str::npos ? str::npos : levelTab - tab - 1), score, level, sheep};
			Validate(entry);
			Entries.push_back(std::move(entry));
			if (Entries.size() > 10)
				throw std::invalid_argument("More than 10 highscore entries");
			start = end == data.size() ? end : end + 1;
		}
	}
	catch (const std::invalid_argument& error)
	{
		throw std::runtime_error(str("Corrupt highscore file (preserved): ") + error.what());
	}
	Rank(Entries);
}
bool Highscores::Qualifies(i32 score) const noexcept
{
	return score >= 0 && (Entries.size() < 10 || score > Entries.back().Score);
}
void Highscores::Submit(const str& name, i32 score, i32 level, i32 sheep)
{
	if (level < 1)
		throw std::invalid_argument("Submitted highscore level must be at least 1");
	HighscoreEntry entry{name, score, level, sheep};
	Validate(entry);
	auto next = Entries;
	next.push_back(std::move(entry));
	Rank(next);
	Save(next);
	Entries.swap(next);
}
void Highscores::Save(const std::vector<HighscoreEntry>& entriesToSave) const
{
	str data;
	for (const HighscoreEntry& entry : entriesToSave)
		data += std::to_string(entry.Score) + "\t" + entry.Name + "\t" + std::to_string(entry.Level) + "\t" + std::to_string(entry.Sheep) + "\n";
	SaveTextFile(File, data);
}
