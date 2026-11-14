#pragma once
#include "EngineAbi.h"

namespace sven2 {
// Engine must outlive the font and any queued draws. Invalid UTF-8 throws.
class BitmapFont {
public:
    explicit BitmapFont(Engine& engine, bool smallFont=false, bool green=false);
    void append(std::vector<SpriteDraw>& draws, const std::string& utf8, int x, int y) const;
    // Word wrap, splitting words wider than width; explicit newlines are retained.
    void appendWrapped(std::vector<SpriteDraw>& draws, const std::string& utf8,
                       int x, int y, int width, int lineHeight) const;
    // Maximum line advance; spaces advance 6 pixels, tabs four spaces.
    // Unsupported Unicode scalars render as '?'; CRLF is one newline.
    int width(const std::string& utf8) const;
private:
    const Sprite* sprite_;
    bool small_;
    int advance(int glyph) const;
    void layout(std::vector<SpriteDraw>* draws, const std::wstring& text,
                int x, int y, int wrapWidth, int lineHeight, int& measured) const;
};
}
