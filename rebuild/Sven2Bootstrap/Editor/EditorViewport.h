#pragma once
#include "Engine/Structures.h"
#include <windows.h>

class EditorViewport
{
public:
	POINT WorldAt(POINT screen) const;
	POINT ScreenAt(POINT world) const;
	void ZoomAt(POINT screen, i32 steps);
	void Pan(i32 dx, i32 dy);
	void Reset();
	f32 GetZoom() const
	{
		return Zoom;
	}
	void Transform(std::vector<SpriteDraw>& commands);

private:
	void Clamp();
	f32 Zoom = 1, OffsetX = 0, OffsetY = 0;
	Sprite Surface{ std::vector<u32>(800 * 600), 800, 800, 600, 1, 1, false };
};
