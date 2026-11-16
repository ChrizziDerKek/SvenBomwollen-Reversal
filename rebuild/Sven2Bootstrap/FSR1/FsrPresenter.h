#pragma once
#include "Types.h"
#include <windows.h>
#include <memory>

enum class ScalingFilter
{
	Nearest,
	Bilinear,
	FSR1,
};

class FsrPresenter
{
public:
	FsrPresenter(i32 width, i32 height, ScalingFilter filter, f32 sharpness);
	~FsrPresenter();
	FsrPresenter(const FsrPresenter&) = delete;
	FsrPresenter& operator=(const FsrPresenter&) = delete;
	HWND Window() const;
	bool Closed() const;
	void Present(const u32* bgra, const fpath& capture = {});
	void SetFilter(ScalingFilter filter);
	void SetSharpness(f32 sharpness);
	static RECT ViewPort(i32 with, i32 height);
	static POINT LogicalMouse(POINT point, i32 width, i32 height);

private:
	struct Impl;
	std::unique_ptr<Impl> ImplPtr;
};