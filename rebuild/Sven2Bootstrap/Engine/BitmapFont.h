#pragma once
#include "Types.h"
#include "MudGE.h"

class BitmapFont
{
public:
	BitmapFont(MudGE& engine, bool smallFont = false, bool green = false, GameEdition edition = GameEdition::Zwo);
	void Append(std::vector<SpriteDraw>& draws, const str& utf8, i32 x, i32 y) const;
	// Word wrap, splitting words wider than width; explicit newlines are retained.
	void AppendWrapped(std::vector<SpriteDraw>& draws, const str& utf8, i32 x, i32 y, i32 width, i32 lineHeight) const;
	// Maximum line advance; Zwo spaces advance 6 pixels, XXX spacing comes from FDD.
	// Unsupported Unicode scalars render as '?'; CRLF is one newline.
	i32 Width(const str& utf8) const;

private:
	const Sprite* Sprite;
	bool Small;
	bool NativeFont = false;
	i32 SpaceAdvance = 6;
	i32 LineAdvance = 35;
	struct NativeGlyph
	{
		::Sprite Image;
		i32 Advance;
	};
	std::map<i32, NativeGlyph> Glyphs;
	void LoadNative(MudGE& engine, bool green);
	i32 GetGlyph(wchar_t character) const;

	i32 Advance(i32 glyph) const;
	void Layout(std::vector<SpriteDraw>* draws, const wstr& text, i32 x, i32 y, i32 wrapWidth, i32 lineHeight, i32& measured) const;
};
