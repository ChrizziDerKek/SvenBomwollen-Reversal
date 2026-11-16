#include "FsrPresenter.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

#include "FsrPresenterUtils.h"

RECT FsrPresenter::ViewPort(i32 width, i32 height)
{
    if (width <= 0 || height <= 0)
        return {};
    i32 w = width;
    i32 h = static_cast<i32>(static_cast<i64>(width) * 3 / 4);
    if (h > height) {
        h = height;
        w = static_cast<i32>(static_cast<i64>(height) * 4 / 3);
    }
    return { (width - w) / 2, (height - h) / 2, (width - w) / 2 + w, (height - h) / 2 + h };
}

POINT FsrPresenter::LogicalMouse(POINT p, i32 width, i32 height)
{
    const RECT r = ViewPort(width, height);
    if (r.right <= r.left || r.bottom <= r.top || p.x < r.left || p.x >= r.right || p.y < r.top || p.y >= r.bottom)
        return { -1, -1 };
    return { static_cast<LONG>((p.x - r.left) * 800LL / (r.right - r.left)), static_cast<LONG>((p.y - r.top) * 600LL / (r.bottom - r.top)) };
}

struct FsrPresenter::Impl
{
    HWND window = nullptr;
    bool closed = false, fullscreen = false;
    WINDOWPLACEMENT placement{ sizeof(WINDOWPLACEMENT) };
    ScalingFilter filter;
    f32 sharpness;
    u32 width = 0, height = 0, scaledWidth = 0, scaledHeight = 0;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11RenderTargetView> backbuffer, easuTarget;
    ComPtr<ID3D11Texture2D> input, easuTexture;
    ComPtr<ID3D11ShaderResourceView> inputView, easuView;
    ComPtr<ID3D11VertexShader> vertex;
    ComPtr<ID3D11PixelShader> easu, rcas, sample;
    ComPtr<ID3D11SamplerState> linear, point;
    ComPtr<ID3D11Buffer> constants;

    struct Settings
    {
        f32 size[2];
        f32 sharpness;
        u32 filter;
        f32 origin[2];
        f32 pad[2];
    };

    Impl(ScalingFilter f, f32 s) :
        filter(f), sharpness(s)
    { }

    ~Impl()
    {
        if (context) {
            context->ClearState();
            context->Flush();
        }
        if (window)
            DestroyWindow(window);
    }

    void Title() const
    {
        wstr name = filter == ScalingFilter::FSR1 ? L"FSR1" : filter == ScalingFilter::Nearest ? L"Nearest" : L"Bilinear";
        const wstr text = std::wstring(L"Sven Zwo - ") + name + L" | sharpness " + std::to_wstring(sharpness).substr(0, 4) + L" | F6 filter, F7/F8 sharpness, F11 fullscreen";
        SetWindowTextW(window, text.c_str());
    }

    void ToggleFullscreen()
    {
        if (!fullscreen) {
            GetWindowPlacement(window, &placement);
            MONITORINFO monitor{ sizeof(MONITORINFO) }; GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);
            SetWindowLongPtrW(window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowPos(window, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top, monitor.rcMonitor.right - monitor.rcMonitor.left, monitor.rcMonitor.bottom - monitor.rcMonitor.top, SWP_FRAMECHANGED);
        }
        else {
            SetWindowLongPtrW(window, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            SetWindowPlacement(window, &placement);
            SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
        fullscreen = !fullscreen;
    }

    static LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wp, LPARAM lp)
    {
        Impl* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (self) {
            if (message == WM_CLOSE) {
                self->closed = true;
                ShowWindow(window, SW_HIDE);
                return 0;
            }
            if (message == WM_GETMINMAXINFO) {
                reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize = { 400,300 };
                return 0;
            }
            if (message == WM_KEYDOWN && !(lp & (1LL << 30))) {
                if (wp == VK_F6)
                    self->filter = static_cast<ScalingFilter>((static_cast<i32>(self->filter) + 1) % 3);
                else if (wp == VK_F7)
                    self->sharpness = std::min(2.0f, self->sharpness + 0.25f);
                else if (wp == VK_F8)
                    self->sharpness = std::max(0.0f, self->sharpness - 0.25f);
                else if (wp == VK_F11)
                    self->ToggleFullscreen();
                self->Title();
            }
            if (message == WM_ERASEBKGND)
                return 1;
        }
        return DefWindowProcW(window, message, wp, lp);
    }

    void Initialize(i32 requestedWidth, i32 requestedHeight)
    {
        WNDCLASSW cls{};
        cls.lpfnWndProc = Procedure;
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpszClassName = L"SvenFsrPresentation"; cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Cannot register FSR window.");
        RECT rect{ 0,0,requestedWidth,requestedHeight }; AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        window = CreateWindowExW(0, cls.lpszClassName, L"Sven Zwo", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, cls.hInstance, this);
        if (!window) throw std::runtime_error("Cannot create FSR presentation window.");
        DXGI_SWAP_CHAIN_DESC desc{}; desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount = 2;
        desc.OutputWindow = window; desc.Windowed = TRUE; desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
        auto result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &level, 1,
            D3D11_SDK_VERSION, &desc, &swap, &device, nullptr, &context);
        if (FAILED(result)) Check(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &level, 1,
            D3D11_SDK_VERSION, &desc, &swap, &device, nullptr, &context), "D3D11 initialization (hardware/WARP)");
        ComPtr<IDXGIFactory> factory; Check(swap->GetParent(IID_PPV_ARGS(&factory)), "DXGI factory");
        Check(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER), "Disable DXGI fullscreen shortcut");
        auto vs = Compile("vertex", "vs_5_0"); Check(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &vertex), "Vertex shader");
        auto makePixel = [&](const char* pass, ComPtr<ID3D11PixelShader>& shader) {
            auto code = Compile("pixel", "ps_5_0", pass);
            Check(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &shader), "Pixel shader");
        };
        makePixel("PASS_EASU", easu); makePixel("PASS_RCAS", rcas); makePixel(nullptr, sample);
        D3D11_SAMPLER_DESC sampler{}; sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; sampler.MaxLOD = D3D11_FLOAT32_MAX;
        Check(device->CreateSamplerState(&sampler, &linear), "Linear sampler");
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; Check(device->CreateSamplerState(&sampler, &point), "Point sampler");
        D3D11_BUFFER_DESC buffer{}; buffer.ByteWidth = sizeof(Settings); buffer.Usage = D3D11_USAGE_DEFAULT; buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Check(device->CreateBuffer(&buffer, nullptr, &constants), "Shader constants");
        D3D11_TEXTURE2D_DESC texture{}; texture.Width = 800; texture.Height = 600; texture.MipLevels = texture.ArraySize = 1;
        texture.Format = DXGI_FORMAT_B8G8R8A8_UNORM; texture.SampleDesc.Count = 1;
        texture.Usage = D3D11_USAGE_DEFAULT; texture.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        Check(device->CreateTexture2D(&texture, nullptr, &input), "800x600 upload texture");
        Check(device->CreateShaderResourceView(input.Get(), nullptr, &inputView), "Upload view");
        Title(); ShowWindow(window, SW_SHOW); UpdateWindow(window); SetForegroundWindow(window);
    }

    void resize(UINT w, UINT h)
    {
        if (w == width && h == height) return;
        context->OMSetRenderTargets(0, nullptr, nullptr); backbuffer.Reset();
        Check(swap->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0), "Resize swap chain");
        ComPtr<ID3D11Texture2D> back; Check(swap->GetBuffer(0, IID_PPV_ARGS(&back)), "Backbuffer");
        Check(device->CreateRenderTargetView(back.Get(), nullptr, &backbuffer), "Backbuffer view");
        width = w; height = h;
        auto r = FsrPresenter::ViewPort(w, h); scaledWidth = r.right - r.left; scaledHeight = r.bottom - r.top;
        easuView.Reset(); easuTarget.Reset(); easuTexture.Reset();
        D3D11_TEXTURE2D_DESC desc{}; desc.Width = scaledWidth; desc.Height = scaledHeight;
        desc.ArraySize = desc.MipLevels = 1; desc.SampleDesc.Count = 1; desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        Check(device->CreateTexture2D(&desc, nullptr, &easuTexture), "EASU intermediate");
        Check(device->CreateRenderTargetView(easuTexture.Get(), nullptr, &easuTarget), "EASU target");
        Check(device->CreateShaderResourceView(easuTexture.Get(), nullptr, &easuView), "EASU view");
    }

    void draw(ID3D11RenderTargetView* target, ID3D11ShaderResourceView* source, ID3D11PixelShader* shader, f32 left, f32 top) const
    {
        Settings settings{ { static_cast<f32>(scaledWidth), static_cast<f32>(scaledHeight) }, sharpness, 0, { left, top }, {} };
        context->UpdateSubresource(constants.Get(), 0, nullptr, &settings, 0, 0);
        D3D11_VIEWPORT viewport{ left, top, static_cast<f32>(scaledWidth), static_cast<f32>(scaledHeight), 0, 1 };
        context->RSSetViewports(1, &viewport);
        context->OMSetRenderTargets(1, &target, nullptr);
        context->PSSetShaderResources(0, 1, &source);
        context->PSSetShader(shader, nullptr, 0);
        context->Draw(3, 0);
        ID3D11ShaderResourceView* empty = nullptr;
        context->PSSetShaderResources(0, 1, &empty);
    }

    void Capture(const fpath& path) const
    {
        ComPtr<ID3D11Texture2D> back, staging; Check(swap->GetBuffer(0, IID_PPV_ARGS(&back)), "Capture backbuffer");
        D3D11_TEXTURE2D_DESC desc{}; back->GetDesc(&desc); desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = desc.MiscFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        Check(device->CreateTexture2D(&desc, nullptr, &staging), "Capture staging");
        context->CopyResource(staging.Get(), back.Get()); D3D11_MAPPED_SUBRESOURCE map{};
        Check(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &map), "Capture readback");
        BITMAPFILEHEADER file{}; file.bfType = 0x4d42; file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
        file.bfSize = file.bfOffBits + width * height * 4;
        BITMAPINFOHEADER info{}; info.biSize = sizeof(info); info.biWidth = width; info.biHeight = -static_cast<LONG>(height);
        info.biPlanes = 1; info.biBitCount = 32; info.biSizeImage = width * height * 4;
        std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char*>(&file), sizeof(file)); out.write(reinterpret_cast<const char*>(&info), sizeof(info));
        for (UINT y = 0; y < height; ++y) for (UINT x = 0; x < width; ++x) {
            const auto rgba = reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(map.pData) + y * map.RowPitch)[x];
            const auto bgra = (rgba & 0xff00ff00) | ((rgba & 255) << 16) | ((rgba >> 16) & 255);
            out.write(reinterpret_cast<const char*>(&bgra), 4);
        }
        context->Unmap(staging.Get(), 0);
        if (!out) throw std::runtime_error("Cannot write scaled frame capture.");
    }

    void Present(const u32* pixels, const fpath& path)
    {
        RECT client{}; GetClientRect(window, &client);
        if (closed || IsIconic(window) || client.right < 4 || client.bottom < 3) return;
        resize(client.right, client.bottom);
        context->UpdateSubresource(input.Get(), 0, nullptr, pixels, 800 * 4, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertex.Get(), nullptr, 0); context->PSSetConstantBuffers(0, 1, constants.GetAddressOf());
        auto* sampler = filter == ScalingFilter::Nearest ? point.Get() : linear.Get(); context->PSSetSamplers(0, 1, &sampler);
        const float black[4] = { 0,0,0,1 }; context->ClearRenderTargetView(backbuffer.Get(), black);
        auto rect = FsrPresenter::ViewPort(width, height);
        // FSR1 is an upscaler. At native size or below, use ordinary sampling.
        if (filter == ScalingFilter::FSR1 && scaledWidth > 800 && scaledHeight > 600) {
            draw(easuTarget.Get(), inputView.Get(), easu.Get(), 0, 0);
            draw(backbuffer.Get(), easuView.Get(), rcas.Get(), static_cast<float>(rect.left), static_cast<float>(rect.top));
        }
        else draw(backbuffer.Get(), inputView.Get(), sample.Get(), static_cast<float>(rect.left), static_cast<float>(rect.top));
        if (!path.empty()) Capture(path);
        const auto result = swap->Present(1, 0);
        if (result != DXGI_STATUS_OCCLUDED) Check(result, "D3D11 present");
    }
};

FsrPresenter::FsrPresenter(i32 width, i32 height, ScalingFilter filter, f32 sharpness)
    : ImplPtr(std::make_unique<Impl>(filter, sharpness))
{
    if (width < 320 || height < 240 || width > 7680 || height > 4320 || !std::isfinite(sharpness) || sharpness < 0 || sharpness > 2)
        throw std::runtime_error("Invalid scaled window size or sharpness (0..2).");
    ImplPtr->Initialize(width, height);
}

HWND FsrPresenter::Window() const
{
    return ImplPtr->window;
}

bool FsrPresenter::Closed() const
{
    return ImplPtr->closed;
}

void FsrPresenter::Present(const u32* bgra, const fpath& path)
{
    ImplPtr->Present(bgra, path);
}

void FsrPresenter::SetFilter(ScalingFilter filter)
{
    ImplPtr->filter = filter;
    ImplPtr->Title();
}

void FsrPresenter::SetSharpness(f32 sharpness)
{
    if (!std::isfinite(sharpness) || sharpness < 0 || sharpness > 2)
        throw std::runtime_error("Sharpness must be 0..2 stops.");
    ImplPtr->sharpness = sharpness;
    ImplPtr->Title();
}
