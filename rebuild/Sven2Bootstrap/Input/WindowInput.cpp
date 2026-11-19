#include "WindowInput.h"
#include "FSR1/FsrPresenter.h"
#include "Engine/Structures.h"
#include <stdexcept>

struct KeyMapping
{
	NativeKey Native;
	i32 Virtual;
};

constexpr KeyMapping Keys[] = {
	{ NativeKey::Up, VK_UP },
	{ NativeKey::Down, VK_DOWN },
	{ NativeKey::Left, VK_LEFT },
	{ NativeKey::Right, VK_RIGHT },
	{ NativeKey::Insert, VK_INSERT },
	{ NativeKey::Delete, VK_DELETE },
	{ NativeKey::Home, VK_HOME },
	{ NativeKey::End, VK_END },
	{ NativeKey::PageUp, VK_PRIOR },
	{ NativeKey::PageDown, VK_NEXT },
	{ NativeKey::Pause, VK_PAUSE },
	{ NativeKey::Break, VK_CANCEL },
	{ NativeKey::Num0, VK_NUMPAD0 },
	{ NativeKey::Num1, VK_NUMPAD1 },
	{ NativeKey::Num2, VK_NUMPAD2 },
	{ NativeKey::Num3, VK_NUMPAD3 },
	{ NativeKey::Num4, VK_NUMPAD4 },
	{ NativeKey::Num5, VK_NUMPAD5 },
	{ NativeKey::Num6, VK_NUMPAD6 },
	{ NativeKey::Num7, VK_NUMPAD7 },
	{ NativeKey::Num8, VK_NUMPAD8 },
	{ NativeKey::Num9, VK_NUMPAD9 },
	{ NativeKey::Decimal, VK_DECIMAL },
	{ NativeKey::LeftAlt, VK_LMENU },
	{ NativeKey::RightAlt, VK_RMENU },
	{ NativeKey::LeftControl, VK_LCONTROL },
	{ NativeKey::RightControl, VK_RCONTROL },
	{ NativeKey::LeftShift, VK_LSHIFT },
	{ NativeKey::RightShift, VK_RSHIFT },
	{ NativeKey::LeftWindows, VK_LWIN },
	{ NativeKey::RightWindows, VK_RWIN },
	{ NativeKey::Menu, VK_APPS },
	{ NativeKey::CapsLock, VK_CAPITAL },
	{ NativeKey::NumLock, VK_NUMLOCK },
	{ NativeKey::ScrollLock, VK_SCROLL }
};

i32 WindowInput::NativeKeyFromVirtual(i32 key)
{
	for (const KeyMapping& mapping : Keys)
		if (mapping.Virtual == key)
			return static_cast<i32>(mapping.Native);
	if (key >= 'A' && key <= 'Z')
		return key - 'A' + 'a';
	if ((key >= '0' && key <= '9') || key == VK_SPACE || key == VK_RETURN || key == VK_TAB || key == VK_BACK)
		return key;
	const i32 character = static_cast<i32>(MapVirtualKeyW(key, MAPVK_VK_TO_CHAR));
	return character >= 33 && character <= 126 ? character : -1;
}

i32 WindowInput::VirtualKeyFromNative(i32 key)
{
	for (const KeyMapping& mapping : Keys)
		if (static_cast<i32>(mapping.Native) == key)
			return mapping.Virtual;
	if (key >= 'a' && key <= 'z')
		return key - 'a' + 'A';
	if ((key >= '0' && key <= '9') || key == VK_SPACE || key == VK_RETURN || key == VK_TAB || key == VK_BACK)
		return key;
	if (key >= 33 && key <= 126)
	{
		const i16 mapped = VkKeyScanW(static_cast<wchar_t>(key));
		if (mapped != -1 && HIBYTE(mapped) == 0)
			return LOBYTE(mapped);
	}
	return -1;
}

PlayerInput WindowInput::BoundPlayerInput(const std::array<i32, 5>& bindings) const
{
	auto held = [&](i32 code) { const i32 key = VirtualKeyFromNative(code); return key >= 0 && key < static_cast<i32>(Previous.size()) && Previous[key]; };
	return { static_cast<i32>(held(bindings[3])) - static_cast<i32>(held(bindings[2])), static_cast<i32>(held(bindings[1])) - static_cast<i32>(held(bindings[0])), held(bindings[4]) };
}

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
	input.BindingKey = PendingBindingKey;
	PendingBindingKey = -1;
	std::array<bool, 256> held{};
	for (i32 i = 0; i < 256; ++i)
		held[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
	auto pressed = [&](i32 key)
	{ return held[key] && !Previous[key]; };
	input.Confirm = pressed(VK_RETURN) || pressed(VK_SPACE);
	input.Back = pressed(VK_ESCAPE);
	input.Click = pressed(VK_LBUTTON);
	input.Left = pressed(VK_LEFT);
	input.Right = pressed(VK_RIGHT);
	input.Up = pressed(VK_UP);
	input.Down = pressed(VK_DOWN);
	input.Tab = pressed(VK_TAB);
	input.Erase = pressed(VK_BACK);
	input.Debug = pressed(VK_F2);
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
	if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && (lParam & (LPARAM{ 1 } << 30)) == 0)
	{
		i32 key = static_cast<i32>(wParam);
		const bool extended = (lParam & (LPARAM{ 1 } << 24)) != 0;
		if (key == VK_CONTROL)
			key = extended ? VK_RCONTROL : VK_LCONTROL;
		else if (key == VK_MENU)
			key = extended ? VK_RMENU : VK_LMENU;
		else if (key == VK_SHIFT)
			key = static_cast<i32>(MapVirtualKeyW((static_cast<uptr>(lParam) >> 16) & 0xff, MAPVK_VSC_TO_VK_EX));
		input->PendingBindingKey = NativeKeyFromVirtual(key);
	}
	return CallWindowProcW(input->Original, window, message, wParam, lParam);
}
