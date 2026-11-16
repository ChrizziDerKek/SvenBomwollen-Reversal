#include "FsrPresenter.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

namespace
{
#include "FsrPresenterUtils.h"
}

RECT FsrPresenter::ViewPort(i32 width, i32 height)
{
	if (width <= 0 || height <= 0)
		return {};
	i32 w = width;
	i32 h = static_cast<i32>(static_cast<i64>(width) * 3 / 4);
	if (h > height)
	{
		h = height;
		w = static_cast<i32>(static_cast<i64>(height) * 4 / 3);
	}
	return {(width - w) / 2, (height - h) / 2, (width - w) / 2 + w, (height - h) / 2 + h};
}

POINT FsrPresenter::LogicalMouse(POINT p, i32 width, i32 height)
{
	const RECT r = ViewPort(width, height);
	if (r.right <= r.left || r.bottom <= r.top || p.x < r.left || p.x >= r.right || p.y < r.top || p.y >= r.bottom)
		return {-1, -1};
	return {static_cast<LONG>((p.x - r.left) * 800LL / (r.right - r.left)), static_cast<LONG>((p.y - r.top) * 600LL / (r.bottom - r.top))};
}

struct FsrPresenter::Impl
{
	HWND Window = nullptr;
	bool Closed = false, Fullscreen = false;
	WINDOWPLACEMENT Placement{sizeof(WINDOWPLACEMENT)};
	ScalingFilter Filter;
	f32 Sharpness;
	u32 Width = 0, Height = 0, ScaledWidth = 0, ScaledHeight = 0;
	ComPtr<ID3D11Device> Device;
	ComPtr<ID3D11DeviceContext> Context;
	ComPtr<IDXGISwapChain> Swap;
	ComPtr<ID3D11RenderTargetView> Backbuffer, EasuTarget;
	ComPtr<ID3D11Texture2D> Input, EasuTexture;
	ComPtr<ID3D11ShaderResourceView> InputView, EasuView;
	ComPtr<ID3D11VertexShader> Vertex;
	ComPtr<ID3D11PixelShader> Easu, Rcas, Sample;
	ComPtr<ID3D11SamplerState> Linear, Point;
	ComPtr<ID3D11Buffer> Constants;

	struct Settings
	{
		f32 Size[2];
		f32 Sharpness;
		u32 Filter;
		f32 Origin[2];
		f32 Pad[2];
	};

	Impl(ScalingFilter f, f32 s)
	    : Filter(f), Sharpness(s)
	{
	}

	~Impl()
	{
		if (Context)
		{
			Context->ClearState();
			Context->Flush();
		}
		if (Window)
			DestroyWindow(Window);
	}

	void Title() const
	{
		wstr name = Filter == ScalingFilter::FSR1 ? L"FSR1" : Filter == ScalingFilter::Nearest ? L"Nearest"
		                                                                                       : L"Bilinear";
		const wstr text = wstr(L"Sven Zwo - ") + name + L" | sharpness " + std::to_wstring(Sharpness).substr(0, 4) + L" | F6 filter, F7/F8 sharpness, F11 fullscreen";
		SetWindowTextW(Window, text.c_str());
	}

	void ToggleFullscreen()
	{
		if (!Fullscreen)
		{
			GetWindowPlacement(Window, &Placement);
			MONITORINFO monitor{sizeof(MONITORINFO)};
			GetMonitorInfoW(MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST), &monitor);
			SetWindowLongPtrW(Window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
			SetWindowPos(Window, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top, monitor.rcMonitor.right - monitor.rcMonitor.left, monitor.rcMonitor.bottom - monitor.rcMonitor.top, SWP_FRAMECHANGED);
		}
		else
		{
			SetWindowLongPtrW(Window, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
			SetWindowPlacement(Window, &Placement);
			SetWindowPos(Window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		}
		Fullscreen = !Fullscreen;
	}

	static LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wp, LPARAM lp)
	{
		Impl* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
		if (message == WM_NCCREATE)
		{
			self = static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
			SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		}
		if (self)
		{
			if (message == WM_CLOSE)
			{
				self->Closed = true;
				ShowWindow(window, SW_HIDE);
				return 0;
			}
			if (message == WM_GETMINMAXINFO)
			{
				reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize = {400, 300};
				return 0;
			}
			if (message == WM_KEYDOWN && !(lp & (1LL << 30)))
			{
				if (wp == VK_F6)
					self->Filter = static_cast<ScalingFilter>((static_cast<i32>(self->Filter) + 1) % 3);
				else if (wp == VK_F7)
					self->Sharpness = std::min(2.0f, self->Sharpness + 0.25f);
				else if (wp == VK_F8)
					self->Sharpness = std::max(0.0f, self->Sharpness - 0.25f);
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
		cls.lpszClassName = L"SvenFsrPresentation";
		cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
			throw std::runtime_error("Cannot register FSR window.");
		RECT rect{0, 0, requestedWidth, requestedHeight};
		AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
		Window = CreateWindowExW(0, cls.lpszClassName, L"Sven Zwo", WS_OVERLAPPEDWINDOW,
		                         CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, cls.hInstance, this);
		if (!Window)
			throw std::runtime_error("Cannot create FSR presentation window.");
		DXGI_SWAP_CHAIN_DESC desc{};
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = 2;
		desc.OutputWindow = Window;
		desc.Windowed = TRUE;
		desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
		HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &level, 1,
		                                               D3D11_SDK_VERSION, &desc, &Swap, &Device, nullptr, &Context);
		if (FAILED(result))
			Check(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &level, 1,
			                                    D3D11_SDK_VERSION, &desc, &Swap, &Device, nullptr, &Context),
			      "D3D11 initialization (hardware/WARP)");
		ComPtr<IDXGIFactory> factory;
		Check(Swap->GetParent(IID_PPV_ARGS(&factory)), "DXGI factory");
		Check(factory->MakeWindowAssociation(Window, DXGI_MWA_NO_ALT_ENTER), "Disable DXGI fullscreen shortcut");
		auto vs = Compile("vertex", "vs_5_0");
		Check(Device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &Vertex), "Vertex shader");
		auto makePixel = [&](cstr pass, ComPtr<ID3D11PixelShader>& shader)
		{
			auto code = Compile("pixel", "ps_5_0", pass);
			Check(Device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &shader), "Pixel shader");
		};
		makePixel("PASS_EASU", Easu);
		makePixel("PASS_RCAS", Rcas);
		makePixel(nullptr, Sample);
		D3D11_SAMPLER_DESC sampler{};
		sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler.MaxLOD = D3D11_FLOAT32_MAX;
		Check(Device->CreateSamplerState(&sampler, &Linear), "Linear sampler");
		sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		Check(Device->CreateSamplerState(&sampler, &Point), "Point sampler");
		D3D11_BUFFER_DESC buffer{};
		buffer.ByteWidth = sizeof(Settings);
		buffer.Usage = D3D11_USAGE_DEFAULT;
		buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		Check(Device->CreateBuffer(&buffer, nullptr, &Constants), "Shader constants");
		D3D11_TEXTURE2D_DESC texture{};
		texture.Width = 800;
		texture.Height = 600;
		texture.MipLevels = texture.ArraySize = 1;
		texture.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		texture.SampleDesc.Count = 1;
		texture.Usage = D3D11_USAGE_DEFAULT;
		texture.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		Check(Device->CreateTexture2D(&texture, nullptr, &Input), "800x600 upload texture");
		Check(Device->CreateShaderResourceView(Input.Get(), nullptr, &InputView), "Upload view");
		Title();
		ShowWindow(Window, SW_SHOW);
		UpdateWindow(Window);
		SetForegroundWindow(Window);
	}

	void Resize(UINT w, UINT h)
	{
		if (w == Width && h == Height)
			return;
		Context->OMSetRenderTargets(0, nullptr, nullptr);
		Backbuffer.Reset();
		Check(Swap->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0), "Resize swap chain");
		ComPtr<ID3D11Texture2D> back;
		Check(Swap->GetBuffer(0, IID_PPV_ARGS(&back)), "Backbuffer");
		Check(Device->CreateRenderTargetView(back.Get(), nullptr, &Backbuffer), "Backbuffer view");
		Width = w;
		Height = h;
		RECT r = FsrPresenter::ViewPort(w, h);
		ScaledWidth = r.right - r.left;
		ScaledHeight = r.bottom - r.top;
		EasuView.Reset();
		EasuTarget.Reset();
		EasuTexture.Reset();
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = ScaledWidth;
		desc.Height = ScaledHeight;
		desc.ArraySize = desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		Check(Device->CreateTexture2D(&desc, nullptr, &EasuTexture), "EASU intermediate");
		Check(Device->CreateRenderTargetView(EasuTexture.Get(), nullptr, &EasuTarget), "EASU target");
		Check(Device->CreateShaderResourceView(EasuTexture.Get(), nullptr, &EasuView), "EASU view");
	}

	void Draw(ID3D11RenderTargetView* target, ID3D11ShaderResourceView* source, ID3D11PixelShader* shader, f32 left, f32 top) const
	{
		Settings settings{{static_cast<f32>(ScaledWidth), static_cast<f32>(ScaledHeight)}, Sharpness, 0, {left, top}, {}};
		Context->UpdateSubresource(Constants.Get(), 0, nullptr, &settings, 0, 0);
		D3D11_VIEWPORT viewport{left, top, static_cast<f32>(ScaledWidth), static_cast<f32>(ScaledHeight), 0, 1};
		Context->RSSetViewports(1, &viewport);
		Context->OMSetRenderTargets(1, &target, nullptr);
		Context->PSSetShaderResources(0, 1, &source);
		Context->PSSetShader(shader, nullptr, 0);
		Context->Draw(3, 0);
		ID3D11ShaderResourceView* empty = nullptr;
		Context->PSSetShaderResources(0, 1, &empty);
	}

	void Capture(const fpath& path) const
	{
		ComPtr<ID3D11Texture2D> back, staging;
		Check(Swap->GetBuffer(0, IID_PPV_ARGS(&back)), "Capture backbuffer");
		D3D11_TEXTURE2D_DESC desc{};
		back->GetDesc(&desc);
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = desc.MiscFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		Check(Device->CreateTexture2D(&desc, nullptr, &staging), "Capture staging");
		Context->CopyResource(staging.Get(), back.Get());
		D3D11_MAPPED_SUBRESOURCE map{};
		Check(Context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &map), "Capture readback");
		BITMAPFILEHEADER file{};
		file.bfType = 0x4d42;
		file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
		file.bfSize = file.bfOffBits + Width * Height * 4;
		BITMAPINFOHEADER info{};
		info.biSize = sizeof(info);
		info.biWidth = Width;
		info.biHeight = -static_cast<LONG>(Height);
		info.biPlanes = 1;
		info.biBitCount = 32;
		info.biSizeImage = Width * Height * 4;
		std::ofstream out(path, std::ios::binary);
		out.write(reinterpret_cast<cstr>(&file), sizeof(file));
		out.write(reinterpret_cast<cstr>(&info), sizeof(info));
		for (UINT y = 0; y < Height; ++y)
			for (UINT x = 0; x < Width; ++x)
			{
				const u32 rgba = reinterpret_cast<const u32*>(static_cast<cstr>(map.pData) + y * map.RowPitch)[x];
				const u32 bgra = (rgba & 0xff00ff00) | ((rgba & 255) << 16) | ((rgba >> 16) & 255);
				out.write(reinterpret_cast<cstr>(&bgra), 4);
			}
		Context->Unmap(staging.Get(), 0);
		if (!out)
			throw std::runtime_error("Cannot write scaled frame capture.");
	}

	void Present(const u32* pixels, const fpath& path)
	{
		RECT client{};
		GetClientRect(Window, &client);
		if (Closed || IsIconic(Window) || client.right < 4 || client.bottom < 3)
			return;
		Resize(client.right, client.bottom);
		Context->UpdateSubresource(Input.Get(), 0, nullptr, pixels, 800 * 4, 0);
		Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Context->VSSetShader(Vertex.Get(), nullptr, 0);
		Context->PSSetConstantBuffers(0, 1, Constants.GetAddressOf());
		ID3D11SamplerState* sampler = Filter == ScalingFilter::Nearest ? Point.Get() : Linear.Get();
		Context->PSSetSamplers(0, 1, &sampler);
		const f32 black[4] = {0, 0, 0, 1};
		Context->ClearRenderTargetView(Backbuffer.Get(), black);
		RECT rect = FsrPresenter::ViewPort(Width, Height);
		// FSR1 is an upscaler. At native size or below, use ordinary sampling.
		if (Filter == ScalingFilter::FSR1 && ScaledWidth > 800 && ScaledHeight > 600)
		{
			Draw(EasuTarget.Get(), InputView.Get(), Easu.Get(), 0, 0);
			Draw(Backbuffer.Get(), EasuView.Get(), Rcas.Get(), static_cast<f32>(rect.left), static_cast<f32>(rect.top));
		}
		else
			Draw(Backbuffer.Get(), InputView.Get(), Sample.Get(), static_cast<f32>(rect.left), static_cast<f32>(rect.top));
		if (!path.empty())
			Capture(path);
		const HRESULT result = Swap->Present(1, 0);
		if (result != DXGI_STATUS_OCCLUDED)
			Check(result, "D3D11 present");
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
	return ImplPtr->Window;
}

bool FsrPresenter::Closed() const
{
	return ImplPtr->Closed;
}

void FsrPresenter::Present(const u32* bgra, const fpath& path)
{
	ImplPtr->Present(bgra, path);
}

void FsrPresenter::SetFilter(ScalingFilter filter)
{
	ImplPtr->Filter = filter;
	ImplPtr->Title();
}

void FsrPresenter::SetSharpness(f32 sharpness)
{
	if (!std::isfinite(sharpness) || sharpness < 0 || sharpness > 2)
		throw std::runtime_error("Sharpness must be 0..2 stops.");
	ImplPtr->Sharpness = sharpness;
	ImplPtr->Title();
}

FsrPresenter::~FsrPresenter() = default;
