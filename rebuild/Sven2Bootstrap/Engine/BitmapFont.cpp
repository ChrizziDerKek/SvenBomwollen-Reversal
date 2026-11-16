#include "BitmapFont.h"
#include <algorithm>
#include <limits>

constexpr i32 LargeWidths[88] = {24, 14, 17, 18, 15, 15, 19, 18, 5, 10, 19, 14, 25, 20, 22, 15, 23, 17, 14, 19, 20, 20, 28, 19, 18, 18, 23, 21, 22, 11, 15, 14, 14, 14, 14, 14, 13, 6, 6, 15, 5, 24, 14, 18, 14, 15, 10, 11, 13, 13, 15, 21, 15, 16, 14, 17, 12, 14, 5, 5, 5, 5, 20, 20, 10, 20, 5, 6, 20, 20, 20, 20, 20, 5, 10, 17, 15, 18, 14, 15, 16, 16, 14, 19, 22, 19, 20, 20};
constexpr i32 SmallWidths[88] = {10, 8, 9, 10, 9, 9, 10, 10, 5, 6, 10, 8, 12, 10, 10, 8, 10, 9, 7, 9, 10, 10, 12, 10, 9, 10, 10, 10, 10, 6, 7, 7, 7, 7, 7, 8, 7, 4, 5, 7, 3, 11, 7, 8, 8, 8, 5, 6, 7, 7, 7, 10, 8, 8, 8, 8, 7, 8, 4, 5, 5, 4, 7, 6, 6, 8, 4, 4, 8, 8, 8, 8, 8, 3, 8, 10, 9, 10, 10, 10, 10, 10, 9, 10, 11, 9, 11, 11};

static wstr Decode(const str& text)
{
	if (text.empty())
		return {};
	if (text.size() > INT_MAX)
		throw std::length_error("Bitmap text too long");
	i32 n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<i32>(text.size()), nullptr, 0);
	if (!n)
		throw std::invalid_argument("Bitmap text is not valid UTF-8");
	wstr result(n, L'\0');
	if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<i32>(text.size()), &result[0], n))
		throw std::invalid_argument("Bitmap text UTF-8 conversion failed");
	return result;
}

static i32 Glyph(wchar_t c)
{
	if (c >= L'A' && c <= L'Z')
		return c - L'A';
	if (c >= L'a' && c <= L'z')
		return c - L'a' + 29;
	if (c >= L'1' && c <= L'9')
		return c - L'1' + 74;
	switch (c)
	{
	case L'\u00c4':
		return 26;
	case L'\u00dc':
		return 27;
	case L'\u00d6':
		return 28;
	case L'\u00f6':
		return 55;
	case L'\u00e4':
		return 56;
	case L'\u00fc':
		return 57;
	case L' ':
		return -1;
	case L'\t':
		return -2;
		// Original character map: Sven2_App_ConstructGlobalInstance, 0x42470C-0x424864.
		// Frame 58 is transparent in the shipped atlas; the name caret is literal I (8).
	case L'#':
		return 58;
	case L'!':
		return 59;
	case L'?':
		return 60;
	case L'.':
		return 61;
	case L',':
		return 66;
	case L'-':
		return 62;
	case L'+':
		return 63;
	case L'*':
		return 64;
	case L'&':
		return 65;
	case L'\'':
	case L'"':
		return 67;
	case L'=':
		return 68;
	case L'(':
		return 69;
	case L')':
		return 70;
	case L'/':
		return 71;
	case L'$':
		return 72;
	case L':':
	case L';':
		return 73;
	case L'0':
		return 83;
	case L'\u00d8':
		return 84;
	case L'\u00f8':
		return 85;
	case L'\u00df':
		return 86;
	default:
		return 60;
	}
}

static i32 Checked(i64 value)
{
	if (value < INT_MIN || value > INT_MAX)
		throw std::overflow_error("Bitmap text coordinate overflow");
	return static_cast<i32>(value);
}

BitmapFont::BitmapFont(MudGE& engine, bool smallFont, bool green)
    : Sprite(engine.LoadSprite(str("Sven2/gfx/font") + (green ? "green" : "") + (smallFont ? "2.bmp" : "1.bmp"), 88, 1)), Small(smallFont)
{
	if (!Sprite)
		throw std::runtime_error("Cannot load bitmap font");
}

i32 BitmapFont::Advance(i32 g) const
{
	if (g < 0)
		return 6 * (g == -2 ? 4 : 1);
	return Small ? SmallWidths[g] : LargeWidths[g];
}

void BitmapFont::Layout(std::vector<SpriteDraw>* draws, const wstr& text, i32 x, i32 y, i32 wrapWidth, i32 lineHeight, i32& measured) const
{
	std::vector<i32> glyphs;
	for (size_t i = 0; i < text.size(); ++i)
	{
		wchar_t c = text[i];
		if (c == L'\r' || c == L'\n')
		{
			if (c == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n')
				++i;
			glyphs.push_back(-3);
		}
		else
		{
			if (c >= 0xd800 && c <= 0xdbff && i + 1 < text.size())
				++i;
			glyphs.push_back(Glyph(c));
		}
	}
	i32 offset = 0;
	auto newline = [&]()
	{ measured = (std::max)(measured, offset); offset = 0; y = Checked(static_cast<i64>(y) + lineHeight); };
	for (size_t i = 0; i < glyphs.size(); ++i)
	{
		i32 g = glyphs[i];
		if (g == -3)
		{
			newline();
			continue;
		}
		if (wrapWidth > 0 && g >= 0 && (i == 0 || glyphs[i - 1] < 0))
		{
			i64 word = 0;
			for (size_t j = i; j < glyphs.size() && glyphs[j] >= 0; ++j)
				word += Advance(glyphs[j]);
			if (offset > 0 && static_cast<i64>(offset) + word > wrapWidth)
				newline();
		}
		i32 a = Advance(g);
		if (wrapWidth > 0 && offset > 0 && static_cast<i64>(offset) + a > wrapWidth)
		{
			newline();
			if (g < 0)
				continue;
		}
		if (draws && g >= 0)
			draws->push_back({Sprite, Checked(static_cast<i64>(x) + offset), y, g});
		offset = Checked(static_cast<i64>(offset) + a);
	}
	measured = std::max(measured, offset);
}

void BitmapFont::Append(std::vector<SpriteDraw>& draws, const str& text, i32 x, i32 y) const
{
	i32 measured = 0;
	Layout(&draws, Decode(text), x, y, 0, Small ? 21 : 35, measured);
}

void BitmapFont::AppendWrapped(std::vector<SpriteDraw>& draws, const str& text, i32 x, i32 y, i32 width, i32 lineHeight) const
{
	if (width <= 0 || lineHeight <= 0)
		throw std::invalid_argument("Text wrap dimensions must be positive");
	i32 measured = 0;
	Layout(&draws, Decode(text), x, y, width, lineHeight, measured);
}

i32 BitmapFont::Width(const str& text) const
{
	i32 measured = 0;
	Layout(nullptr, Decode(text), 0, 0, 0, 0, measured);
	return measured;
}