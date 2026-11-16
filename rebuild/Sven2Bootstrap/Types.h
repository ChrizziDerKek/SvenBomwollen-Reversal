#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

using i8 = signed char;
using i16 = short;
using i32 = int;
using i64 = long long;
using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned;
using u64 = unsigned long long;
using uptr = std::uintptr_t;
using f32 = float;
using f64 = double;
using any = void*;

using bytestream = std::vector<u8>;

using str = std::string;
using wstr = std::wstring;
using cstr = const char*;
using cwstr = const wchar_t*;

using fpath = std::filesystem::path;
