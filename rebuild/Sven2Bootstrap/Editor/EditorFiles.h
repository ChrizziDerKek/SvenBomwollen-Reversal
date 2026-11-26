#pragma once
#include "Engine/GameData.h"
#include <fstream>
#include <algorithm>
#include <limits>
#include <windows.h>

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
	fpath temporary;
	HANDLE file = INVALID_HANDLE_VALUE;
	for (u32 attempt = 0; file == INVALID_HANDLE_VALUE; ++attempt)
	{
		temporary = path;
		temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(attempt);
		file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS)
			throw std::runtime_error("Cannot create editor export; previous file retained.");
	}
	size_t offset = 0;
	bool success = true;
	while (success && offset < bytes.size())
	{
		DWORD written = 0;
		const DWORD count = static_cast<DWORD>(std::min<size_t>(bytes.size() - offset, std::numeric_limits<DWORD>::max()));
		success = WriteFile(file, bytes.data() + offset, count, &written, nullptr) && written > 0;
		offset += written;
	}
	success = success && FlushFileBuffers(file);
	success = CloseHandle(file) && success;
	if (success && MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		return;
	DeleteFileW(temporary.c_str());
	throw std::runtime_error("Cannot save editor file; previous file retained.");
}
inline void Export(const GameData& data, const fpath& path, const bytestream& bytes, cwstr extension)
{
	if (path.empty() || !path.has_filename() || path.native().find(L'\0') != wstr::npos || _wcsicmp(path.extension().c_str(), extension) != 0)
		throw std::runtime_error("Choose an export filename with the requested extension.");
	const fpath destination = std::filesystem::weakly_canonical(std::filesystem::absolute(path)).make_preferred();
	const fpath root = std::filesystem::weakly_canonical(data.GetRoot());
	for (fpath current = destination; std::filesystem::exists(root) && !current.empty(); current = current.parent_path())
	{
		if (std::filesystem::exists(current) && std::filesystem::equivalent(current, root))
			throw std::runtime_error("Export outside the installed assets directory; Ctrl+S manages folder packs.");
		if (current.parent_path() == current)
			break;
	}
	AtomicWrite(destination, bytes);
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
