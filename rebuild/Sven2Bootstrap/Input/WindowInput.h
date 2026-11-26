#pragma once
#include "Types.h"
#include <array>
#include <Windows.h>

struct PlayerInput;

// Keyboard codes stored by the original XXX config/KeyConvert table.
enum class NativeKey : i32
{
	Up = 256,
	Down,
	Left,
	Right,
	Insert,
	Delete,
	Home,
	End,
	PageUp,
	PageDown,
	Pause,
	Break,
	Num5 = 320,
	Num4,
	Num6,
	Num8,
	Num2,
	Num7,
	Num1,
	Num3,
	Num9,
	Num0,
	Decimal,
	LeftAlt = 336,
	RightAlt,
	LeftControl,
	RightControl,
	LeftShift,
	RightShift,
	LeftWindows,
	RightWindows,
	Menu,
	CapsLock = 351,
	NumLock,
	ScrollLock
};

class WindowInput
{
public:
	struct Result
	{
		POINT Mouse{};
		bool MouseMoved = false;
		bool Click = false;
		bool Confirm = false;
		bool Back = false;
		bool Left = false;
		bool Right = false;
		bool Up = false;
		bool Down = false;
		bool Tab = false;
		bool Erase = false;
		bool Debug = false;
		bool Editor = false, Playtest = false, SaveEditor = false, Undo = false, Redo = false, Delete = false;
		bool MiddleHeld = false, MouseHeld = false, RightHeld = false, RightClick = false, CloseRequested = false;
		bool Shift = false, Control = false, Enter = false;
		i32 BindingKey = -1;
		i32 Wheel = 0;
		str Text;
	};

	WindowInput(HWND window, bool scaled = false);
	~WindowInput();
	WindowInput(const WindowInput&) = delete;
	WindowInput& operator=(const WindowInput&) = delete;
	Result Poll();
	void SetEditorCloseGuard(bool active)
	{
		GuardClose = active;
	}
	i32 DX() const;
	i32 DY() const;
	bool ActionHeld() const;
	PlayerInput BoundPlayerInput(const std::array<i32, 5>& bindings) const;
	static i32 NativeKeyFromVirtual(i32 key);
	static i32 VirtualKeyFromNative(i32 key);

private:
	static LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	inline static WindowInput* Current = nullptr;
	HWND Window;
	bool Scaled;
	WNDPROC Original = nullptr;
	std::array<bool, 256> Previous{};
	POINT Mouse{ -1, -1 };
	wstr Typed;
	i32 PendingBindingKey = -1;
	i32 PendingWheel = 0;
	bool GuardClose = false, ClosePending = false;
};
