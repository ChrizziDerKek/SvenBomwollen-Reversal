#pragma once
#include "Engine/GameData.h"
#include <fstream>

namespace EditorFiles
{
inline fpath OutputPath(const GameData& data, const str& name)
{
	const fpath relative = std::filesystem::u8path(name);
	if (relative.empty() || relative.has_root_path() || name.find('\0') != str::npos)
		throw std::runtime_error("Editor output must be relative to the data directory.");
	fpath path = data.GetRoot();
	for (const fpath& part : relative)
	{
		if (part == L".." || part == L".")
			throw std::runtime_error("Editor output cannot traverse directories.");
		path /= part;
		if (std::filesystem::exists(path))
		{
			const fpath checked = std::filesystem::relative(std::filesystem::canonical(path), data.GetRoot());
			for (const fpath& component : checked)
				if (component == L"..")
					throw std::runtime_error("Editor output cannot escape through a directory link.");
		}
	}
	return path;
}
inline void AtomicWrite(const fpath& path, const bytestream& bytes)
{
	fpath temporary = path;
	temporary += L".tmp";
	std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
	file.write(reinterpret_cast<cstr>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	file.close();
	if (!file || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		throw std::runtime_error("Cannot save editor file; previous file retained.");
}
inline str Xml(const str& text)
{
	str result;
	for (char c : text)
	{
		switch (c)
		{
		case '&':
			result += "&amp;";
			break;
		case '<':
			result += "&lt;";
			break;
		case '>':
			result += "&gt;";
			break;
		case '"':
			result += "&quot;";
			break;
		default:
			result += c;
		}
	}
	return result;
}
} // namespace EditorFiles
