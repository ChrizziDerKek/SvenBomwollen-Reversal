#pragma once
#include <windows.h>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace sven2 {
enum class ScalingFilter { nearest, bilinear, fsr1 };

// Presentation only: simulation, sprites and hit testing stay at 800 x 600.
class FsrPresenter {
public:
    FsrPresenter(int width, int height, ScalingFilter filter, float sharpness);
    ~FsrPresenter();
    FsrPresenter(const FsrPresenter&)=delete;
    FsrPresenter& operator=(const FsrPresenter&)=delete;
    HWND window() const;
    bool closed() const;
    void present(const std::uint32_t* bgra, const std::filesystem::path& capture={});
    void setFilter(ScalingFilter filter);
    void setSharpness(float sharpness);
    static RECT viewport(int width, int height);
    static POINT logicalMouse(POINT point, int width, int height);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
