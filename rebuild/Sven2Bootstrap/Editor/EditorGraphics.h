#pragma once
#include "Engine/BitmapFont.h"

namespace EditorGraphics
{
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
