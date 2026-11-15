#include "platform/WindowInput.h"
#include "graphics/FsrPresenter.h"
#include <stdexcept>

namespace sven2 {
WindowInput::WindowInput(HWND window,bool scaled):window_(window),scaled_(scaled) {
    SetLastError(0);
    original_=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window_,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(procedure)));
    if(!original_) throw std::runtime_error("Cannot attach game text input.");
    current_=this;
    for(int i=0;i<256;++i) previous_[i]=(GetAsyncKeyState(i)&0x8000)!=0;
}

WindowInput::~WindowInput() {
    if(IsWindow(window_)) SetWindowLongPtrW(window_,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(original_));
    if(GetForegroundWindow()==window_) SetCursor(LoadCursorW(nullptr,IDC_ARROW));
    current_=nullptr;
}

UiInput WindowInput::poll() {
    UiInput input;
    std::array<bool,256> held{};
    for(int i=0;i<256;++i) held[i]=(GetAsyncKeyState(i)&0x8000)!=0;
    auto pressed=[&](int key) { return held[key] && !previous_[key]; };
    input.confirm=pressed(VK_RETURN)||pressed(VK_SPACE);
    input.back=pressed(VK_ESCAPE);
    input.click=pressed(VK_LBUTTON);
    input.left=pressed(VK_LEFT);
    input.right=pressed(VK_RIGHT);
    input.up=pressed(VK_UP);
    input.down=pressed(VK_DOWN);
    input.tab=pressed(VK_TAB);
    input.erase=pressed(VK_BACK);
    GetCursorPos(&input.mouse);
    ScreenToClient(window_,&input.mouse);
    if(scaled_) {
        RECT client{};GetClientRect(window_,&client);
        input.mouse=FsrPresenter::logicalMouse(input.mouse,client.right,client.bottom);
    }
    input.mouseMoved=input.mouse.x!=mouse_.x || input.mouse.y!=mouse_.y;
    mouse_=input.mouse;
    previous_=held;
    int length=WideCharToMultiByte(CP_UTF8,0,typed_.data(),static_cast<int>(typed_.size()),nullptr,0,nullptr,nullptr);
    input.text.resize(length);
    if(length) WideCharToMultiByte(CP_UTF8,0,typed_.data(),static_cast<int>(typed_.size()),input.text.data(),length,nullptr,nullptr);
    typed_.clear();
    return input;
}

int WindowInput::dx() const { return (previous_[VK_RIGHT]||previous_['D'])-(previous_[VK_LEFT]||previous_['A']); }

int WindowInput::dy() const { return (previous_[VK_DOWN]||previous_['S'])-(previous_[VK_UP]||previous_['W']); }

bool WindowInput::actionHeld() const { return previous_[VK_SPACE]; }

LRESULT CALLBACK WindowInput::procedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    auto* input=current_;
    if(!input) return DefWindowProcW(window,message,wParam,lParam);
    if(message==WM_SETCURSOR && LOWORD(lParam)==HTCLIENT) {
        SetCursor(nullptr);
        return TRUE;
    }
    if(message==WM_CHAR && wParam>=32 && wParam!=127) input->typed_.push_back(static_cast<wchar_t>(wParam));
    return CallWindowProcW(input->original_,window,message,wParam,lParam);
}
}
