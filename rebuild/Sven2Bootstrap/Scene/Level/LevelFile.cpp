#include "Scene/Level/LevelFile.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
template <class T>
T ReadWord(const u8* bytes, size_t offset)
{
	static_assert(sizeof(T) == 4);
	T value;
	std::memcpy(&value, bytes + offset, 4);
	return value;
}
} // namespace
LevelFile::LevelFile(const bytestream& bytes, const str& path)
{
	constexpr size_t headerSize = 16, gridAndStateSize = 9048, objectBytes = 100;
	if (bytes.size() < headerSize || ReadWord<i32>(bytes.data(), 0) != 6)
		throw std::runtime_error("Expected a Sven2 v6 level.");
	HidesUfo = ReadWord<i32>(bytes.data(), 8) != 0;
	const u32 nameSize = ReadWord<u32>(bytes.data(), 12);
	if (nameSize > bytes.size() - headerSize || bytes.size() - headerSize - nameSize != gridAndStateSize + ObjectCount * objectBytes)
		throw std::runtime_error("Unexpected v6 level length: " + path);
	const u8* grid = bytes.data() + headerSize + nameSize;
	for (size_t index = 0; index < Cells.size(); ++index)
	{
		const u8* cell = grid + index * 24;
		Cells[index] = { static_cast<OccupancyCode>(ReadWord<i32>(cell, 0)), cell[4], static_cast<ItemCode>(cell[12]), ReadWord<i32>(cell, 16), static_cast<SpawnerCode>(ReadWord<i32>(cell, 20)) };
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
}
