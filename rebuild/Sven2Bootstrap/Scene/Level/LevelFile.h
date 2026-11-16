#pragma once
#include "Types.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// Serialized v6 records stay separate from mutable simulation state. Unknown
// words are retained for the few original behaviors that still consume them.
class LevelObjectRecord
{
public:
	i32 Kind() const
	{
		return Integer(11);
	}
	i32 RenderPass() const
	{
		return Integer(4);
	}
	i32 Width() const
	{
		return Integer(5);
	}
	i32 Height() const
	{
		return Integer(6);
	}
	i32 Frame() const
	{
		return Integer(12);
	}
	i32 TextureOrPermanent() const
	{
		return Integer(17);
	}
	i32 State() const
	{
		return Integer(18);
	}
	i32 Facing() const
	{
		return Integer(19);
	}
	i32 Tile() const
	{
		return Integer(20);
	}
	i32 Impatience() const
	{
		return Integer(21);
	}
	i32 DepthAdjustment() const
	{
		return Integer(23);
	}
	f32 X() const
	{
		return Real(0);
	}
	f32 Y() const
	{
		return Real(1);
	}
	f32 Idle() const
	{
		return Real(13);
	}
	f32 FrameRate() const
	{
		return Real(14);
	}
	f32 Pulse() const
	{
		return Real(22);
	}
	u8 AngerTicks() const
	{
		return Byte(64);
	}
	u8 Temperament() const
	{
		return Byte(65);
	}
	u8 Attempts() const
	{
		return Byte(66);
	}
	u8 MoodBlink() const
	{
		return Byte(67);
	}
	bool Hunting() const
	{
		return Byte(96) != 0;
	}
	const std::array<u32, 25>& GetWords() const
	{
		return Words;
	}

private:
	friend class LevelFile;
	i32 Integer(size_t index) const
	{
		i32 value;
		std::memcpy(&value, &Words.at(index), 4);
		return value;
	}
	f32 Real(size_t index) const
	{
		f32 value;
		std::memcpy(&value, &Words.at(index), 4);
		return value;
	}
	u8 Byte(size_t offset) const
	{
		return static_cast<u8>(Words.at(offset / 4) >> (8 * (offset % 4)));
	}
	std::array<u32, 25> Words{};
};

struct LevelCell
{
	i32 Terrain = 0;
	u8 Direction = 0, ItemCode = 0;
	i32 ItemObject = -1, Spawner = 0;
};

class LevelFile
{
public:
	static constexpr size_t CellCount = 22 * 17, ObjectCount = 256;
	LevelFile(const bytestream& bytes, const str& path);
	bool GetHidesUfo() const
	{
		return HidesUfo;
	}
	f32 GetTimeLimit() const
	{
		return TimeLimit;
	}
	const std::array<LevelCell, CellCount>& GetCells() const
	{
		return Cells;
	}
	const std::array<LevelObjectRecord, ObjectCount>& GetObjects() const
	{
		return Objects;
	}
	const std::vector<i32>& GetPuddles() const
	{
		return Puddles;
	}

private:
	bool HidesUfo = false;
	f32 TimeLimit = 0;
	std::array<LevelCell, CellCount> Cells{};
	std::array<LevelObjectRecord, ObjectCount> Objects{};
	std::vector<i32> Puddles;
};
