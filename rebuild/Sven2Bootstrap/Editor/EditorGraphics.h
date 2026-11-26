#pragma once
#include "Engine/BitmapFont.h"
#include <algorithm>
#include <cstdlib>

namespace EditorGraphics
{
inline void Line(Sprite& canvas, POINT from, POINT to, u32 color, bool dashed = true)
{
	const i32 dx = std::abs(to.x - from.x), dy = -std::abs(to.y - from.y);
	const i32 stepX = from.x < to.x ? 1 : -1, stepY = from.y < to.y ? 1 : -1;
	i32 error = dx + dy, distance = 0;
	for (;;)
	{
		if (from.x >= 0 && from.x < canvas.Width && from.y >= 0 && from.y < canvas.FrameHeight && (!dashed || distance % 6 < 3))
			canvas.Pixels[from.y * canvas.Width + from.x] = color;
		if (from.x == to.x && from.y == to.y)
			break;
		const i32 twice = error * 2;
		if (twice >= dy)
		{
			error += dy;
			from.x += stepX;
		}
		if (twice <= dx)
		{
			error += dx;
			from.y += stepY;
		}
		++distance;
	}
}

inline Sprite Rectangle(i32 width, i32 height, u32 color, bool outline)
{
	Sprite sprite{ std::vector<u32>(width * height, 0xFFFF00FF), width, width, height, 1, 1, true };
	for (i32 y = 0; y < height; ++y)
	{
		for (i32 x = 0; x < width; ++x)
		{
			if (!outline || x == 0 || y == 0 || x == width - 1 || y == height - 1)
				sprite.Pixels[y * width + x] = color;
		}
	}
	return sprite;
}
inline str FitText(const BitmapFont& font, str text, i32 width)
{
	if (font.Width(text) <= width)
		return text;
	while (!text.empty() && font.Width(text + "...") > width)
	{
		size_t end = text.size() - 1;
		while (end > 0 && (static_cast<u8>(text[end]) & 0xC0) == 0x80)
			--end;
		text.resize(end);
	}
	return text + "...";
}
} // namespace EditorGraphics
