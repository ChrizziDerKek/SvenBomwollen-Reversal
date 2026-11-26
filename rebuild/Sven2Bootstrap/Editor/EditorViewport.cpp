#include "EditorViewport.h"
#include <algorithm>
#include <cmath>

POINT EditorViewport::WorldAt(POINT screen) const
{
	// Input and rasterization sample the same screen-pixel centers.
	return { static_cast<i32>(std::floor((screen.x + .5f - OffsetX) / Zoom)), static_cast<i32>(std::floor((screen.y + .5f - OffsetY) / Zoom)) };
}

POINT EditorViewport::ScreenAt(POINT world) const
{
	return { static_cast<i32>(std::floor((world.x + .5f) * Zoom + OffsetX)), static_cast<i32>(std::floor((world.y + .5f) * Zoom + OffsetY)) };
}

void EditorViewport::Clamp()
{
	// Keep a cell-width strip reachable; the sidebar may cover part of the map.
	OffsetX = std::clamp(OffsetX, 40 - 800 * Zoom, 760.0f);
	OffsetY = std::clamp(OffsetY, 40 - 600 * Zoom, 560.0f);
}

void EditorViewport::ZoomAt(POINT screen, i32 steps)
{
	const f32 zoom = std::clamp(Zoom * std::pow(1.15f, static_cast<f32>(std::clamp(steps, -32, 32))), .25f, 4.0f);
	OffsetX = screen.x + .5f - (screen.x + .5f - OffsetX) * zoom / Zoom;
	OffsetY = screen.y + .5f - (screen.y + .5f - OffsetY) * zoom / Zoom;
	Zoom = zoom;
	Clamp();
}

void EditorViewport::Pan(i32 dx, i32 dy)
{
	OffsetX += dx;
	OffsetY += dy;
	Clamp();
}

void EditorViewport::Reset()
{
	Zoom = 1;
	OffsetX = OffsetY = 0;
}

void EditorViewport::Transform(std::vector<SpriteDraw>& commands)
{
	if (Zoom == 1 && OffsetX == 0 && OffsetY == 0)
		return;
	std::fill(Surface.Pixels.begin(), Surface.Pixels.end(), 0xFF303030);
	std::array<i32, 800> columns;
	std::array<i32, 600> rows;
	for (i32 x = 0; x < 800; ++x)
		columns[x] = WorldAt({ x, 0 }).x;
	for (i32 y = 0; y < 600; ++y)
		rows[y] = WorldAt({ 0, y }).y;
	for (const SpriteDraw& draw : commands)
	{
		const Sprite& sprite = *draw.Sprite;
		if (draw.Frame < 0 || draw.Frame >= sprite.Rows * sprite.Columns)
			continue;
		const POINT start{ static_cast<i32>(std::floor(draw.X * Zoom + OffsetX)), static_cast<i32>(std::floor(draw.Y * Zoom + OffsetY)) };
		const POINT end{ static_cast<i32>(std::ceil((draw.X + sprite.FrameWidth) * Zoom + OffsetX)), static_cast<i32>(std::ceil((draw.Y + sprite.FrameHeight) * Zoom + OffsetY)) };
		const i32 sx = draw.Frame % sprite.Columns * sprite.FrameWidth, sy = draw.Frame / sprite.Columns * sprite.FrameHeight;
		for (i32 y = std::max(0L, start.y - 1); y < std::min(600L, end.y + 1); ++y)
		{
			const i32 wy = rows[y], iy = wy - draw.Y;
			if (wy < 0 || wy >= 600 || iy < 0 || iy >= sprite.FrameHeight || (draw.ClipBottom > 0 && wy >= draw.ClipBottom))
				continue;
			for (i32 x = std::max(0L, start.x - 1); x < std::min(800L, end.x + 1); ++x)
			{
				const i32 wx = columns[x], ix = wx - draw.X;
				if (wx < 0 || wx >= 800 || ix < 0 || ix >= sprite.FrameWidth)
					continue;
				const u32 pixel = sprite.Pixels[(sy + iy) * sprite.Width + sx + (draw.Mirror ? sprite.FrameWidth - ix - 1 : ix)];
				if (!sprite.Keyed || (pixel & 0xFFFFFF) != 0xFF00FF)
					Surface.Pixels[y * 800 + x] = pixel;
			}
		}
	}
	commands = { { &Surface, 0, 0, 0 } };
}
