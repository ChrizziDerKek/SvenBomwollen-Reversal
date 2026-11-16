#include "WindowInput.h"
#include "FSR1/FsrPresenter.h"
#include <stdexcept>

WindowInput::WindowInput(HWND window, bool scaled)
    : Window(window), Scaled(scaled)
{
	SetLastError(0);
	Original = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(Procedure)));
	if (!Original)
		throw std::runtime_error("Cannot attach game text input.");
	Current = this;
	for (i32 i = 0; i < 256; ++i)
		Previous[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
}

WindowInput::~WindowInput()
{
	if (IsWindow(Window))
		SetWindowLongPtrW(Window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(Original));
	if (GetForegroundWindow() == Window)
		SetCursor(LoadCursorW(nullptr, IDC_ARROW));
	Current = nullptr;
}

WindowInput::Result WindowInput::Poll()
{
	Result input;
	std::array<bool, 256> held{};
	for (i32 i = 0; i < 256; ++i)
		held[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
	auto pressed = [&](i32 key) { return held[key] && !Previous[key]; };
	input.Confirm = pressed(VK_RETURN) || pressed(VK_SPACE);
	input.Back = pressed(VK_ESCAPE);
	input.Click = pressed(VK_LBUTTON);
	input.Left = pressed(VK_LEFT);
	input.Right = pressed(VK_RIGHT);
	input.Up = pressed(VK_UP);
	input.Down = pressed(VK_DOWN);
	input.Tab = pressed(VK_TAB);
	input.Erase = pressed(VK_BACK);
	GetCursorPos(&input.Mouse);
	ScreenToClient(Window, &input.Mouse);
	if (Scaled)
	{
		RECT client{};
		GetClientRect(Window, &client);
		input.Mouse = FsrPresenter::LogicalMouse(input.Mouse, client.right, client.bottom);
	}
	input.MouseMoved = input.Mouse.x != Mouse.x || input.Mouse.y != Mouse.y;
	Mouse = input.Mouse;
	Previous = held;
	i32 length = WideCharToMultiByte(CP_UTF8, 0, Typed.data(), static_cast<i32>(Typed.size()), nullptr, 0, nullptr, nullptr);
	input.Text.resize(length);
	if (length)
		WideCharToMultiByte(CP_UTF8, 0, Typed.data(), static_cast<i32>(Typed.size()), input.Text.data(), length, nullptr, nullptr);
	Typed.clear();
	return input;
}

i32 WindowInput::DX() const
{
	return (Previous[VK_RIGHT] || Previous['D']) - (Previous[VK_LEFT] || Previous['A']);
}

i32 WindowInput::DY() const
{
	return (Previous[VK_DOWN] || Previous['S']) - (Previous[VK_UP] || Previous['W']);
}

bool WindowInput::ActionHeld() const
{
	return Previous[VK_SPACE];
}

LRESULT CALLBACK WindowInput::Procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	WindowInput* input = Current;
	if (!input)
		return DefWindowProcW(window, message, wParam, lParam);
	if (message == WM_SETCURSOR && LOWORD(lParam) == HTCLIENT)
	{
		SetCursor(nullptr);
		return TRUE;
	}
	if (message == WM_CHAR && wParam >= 32 && wParam != 127)
		input->Typed.push_back(static_cast<wchar_t>(wParam));
	return CallWindowProcW(input->Original, window, message, wParam, lParam);
}
