#pragma once
#include "Types.h"
#include <array>
#include <Windows.h>

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
		str Text;
	};

	WindowInput(HWND window, bool scaled = false);
	~WindowInput();
	WindowInput(const WindowInput&) = delete;
	WindowInput& operator=(const WindowInput&) = delete;
	Result Poll();
	i32 DX() const;
	i32 DY() const;
	bool ActionHeld() const;

private:
	static LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	inline static WindowInput* Current = nullptr;
	HWND Window;
	bool Scaled;
	WNDPROC Original = nullptr;
	std::array<bool, 256> Previous{};
	POINT Mouse{ -1,-1 };
	wstr Typed;
};