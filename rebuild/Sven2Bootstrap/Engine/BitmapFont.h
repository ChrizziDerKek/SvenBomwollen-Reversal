#pragma once
#include "Types.h"
#include "MudGE.h"

class BitmapFont
{
public:
	BitmapFont(MudGE& engine, bool smallFont = false, bool green = false);
	void Append(std::vector<SpriteDraw>& draws, const str& utf8, i32 x, i32 y) const;
	// Word wrap, splitting words wider than width; explicit newlines are retained.
	void AppendWrapped(std::vector<SpriteDraw>& draws, const str& utf8, i32 x, i32 y, i32 width, i32 lineHeight) const;
	// Maximum line advance; spaces advance 6 pixels, tabs four spaces.
	// Unsupported Unicode scalars render as '?'; CRLF is one newline.
	i32 Width(const str& utf8) const;

private:
	const Sprite* Sprite;
	bool Small;

	i32 Advance(i32 glyph) const;
	void Layout(std::vector<SpriteDraw>* draws, const wstr& text, i32 x, i32 y, i32 wrapWidth, i32 lineHeight, i32& measured) const;
};