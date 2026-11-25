#include "Scene/Level/LevelFile.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

BonusMode LevelFile::GetBonusMode() const
{
	BonusMode mode = BonusMode::Sven;
	if (Version == 7)
	{
		for (BonusMode candidate : { BonusMode::Farmer, BonusMode::Dog, BonusMode::Girl, BonusMode::Alien })
			if ((Flags & (u32{ 1 } << static_cast<u32>(candidate))) != 0)
				mode = candidate;
	}
	return mode;
}

WeatherMode LevelFile::GetWeatherMode() const
{
	if (Version != 7)
		return WeatherMode::None;
	// XXX 0x4306DF..72F: later enabled flags override earlier modes.
	if (Flags & (1u << 7))
		return WeatherMode::Bubbles;
	if (Flags & (1u << 6))
		return WeatherMode::Rain;
	return Flags & (1u << 5) ? WeatherMode::Snow : WeatherMode::None;
}

template <class T>
T ReadWord(const u8* bytes, size_t offset)
{
	static_assert(sizeof(T) == 4);
	T value{};
	std::memcpy(&value, bytes + offset, 4);
	return value;
}

LevelFile::LevelFile(const bytestream& bytes, const str& path)
{
	constexpr size_t headerSize = 16, gridAndStateSize = 9048, objectBytes = 100;
	if (bytes.size() < headerSize)
		throw std::runtime_error("Truncated Sven level header.");
	Version = ReadWord<i32>(bytes.data(), 0);
	if (Version != 6 && Version != 7)
		throw std::runtime_error("Expected a Zwo v6 or XXX v7 level.");
	Flags = ReadWord<u32>(bytes.data(), 8);
	HidesUfo = Version == 6 ? Flags != 0 : (Flags & 1) == 0;
	const u32 nameSize = ReadWord<u32>(bytes.data(), 12);
	const size_t extensionBytes = Version == 7 ? sizeof(Extensions) : 0;
	if (nameSize > bytes.size() - headerSize || bytes.size() - headerSize - nameSize != gridAndStateSize + ObjectCount * objectBytes + extensionBytes)
		throw std::runtime_error("Unexpected Sven level length: " + path);
	const u8* grid = bytes.data() + headerSize + nameSize;
	Extensions.fill({ 0, 0, 400, 2 }); // XXX default layout initialization, 0x4308E0.
	for (size_t index = 0; index < Cells.size(); ++index)
	{
		const u8* cell = grid + index * 24;
		const u32 spawnerAndHelp = ReadWord<u32>(cell, 20);
		Cells[index] = { static_cast<OccupancyCode>(ReadWord<i32>(cell, 0)), cell[4], static_cast<ItemCode>(cell[12]), ReadWord<i32>(cell, 16), static_cast<SpawnerCode>(spawnerAndHelp & 0xffff), Version == 7 ? static_cast<u16>(spawnerAndHelp >> 16) : u16{ 0 } };
	}
	TimeLimit = ReadWord<f32>(grid, 9044);
	if (!std::isfinite(TimeLimit) || TimeLimit < 0)
		throw std::runtime_error("Invalid level timer.");
	const u8* destinations = grid + CellCount * 24;
	const i32 count = std::clamp(ReadWord<i32>(destinations, 0), 0, 16);
	for (size_t slot = 1; slot <= 16 && Puddles.size() < static_cast<size_t>(count); ++slot)
	{
		const i32 tile = ReadWord<i32>(destinations, slot * 4);
		if (tile >= 0 && static_cast<size_t>(tile) < Cells.size() && Cells[tile].Terrain == OccupancyCode::Puddle && std::find(Puddles.begin(), Puddles.end(), tile) == Puddles.end())
			Puddles.push_back(tile);
	}
	for (size_t slot = 0; slot < Objects.size(); ++slot)
		std::memcpy(Objects[slot].Words.data(), grid + gridAndStateSize + slot * objectBytes, objectBytes);
	if (Version == 7)
		std::memcpy(Extensions.data(), grid + gridAndStateSize + ObjectCount * objectBytes, extensionBytes);
}
