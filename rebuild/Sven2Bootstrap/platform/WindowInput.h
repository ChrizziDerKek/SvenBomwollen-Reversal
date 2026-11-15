#pragma once
#include "GameUi.h"
#include <array>

namespace sven2 {
// Owns the temporary window hook and restores it when the game loop exits.
class WindowInput {
public:
    explicit WindowInput(HWND window,bool scaled=false);
    ~WindowInput();
    WindowInput(const WindowInput&)=delete;
    WindowInput& operator=(const WindowInput&)=delete;
    UiInput poll();
    int dx() const;
    int dy() const;
    bool actionHeld() const;
private:
    static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam);
    inline static WindowInput* current_=nullptr;
    HWND window_;
    bool scaled_;
    WNDPROC original_=nullptr;
    std::array<bool,256> previous_{};
    POINT mouse_{-1,-1};
    std::wstring typed_;
};
}
