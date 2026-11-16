#include "Utility/Logger.h"
#include "MudGE.h"
#include "FSR1/FsrPresenter.h"
#include "Utility/SupportedBuild.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <fstream>
#include <wincodec.h>
#include <wrl/client.h>

MudGE::MudGE(const fpath& directory, const fpath& dataDirectory)
    : Directory(directory), Data(dataDirectory)
{
	RequireBuild(directory / L"wtnlib.dll", "48faeb2b05d608a08ae54ad1c1fcbad75a1c7bbbeb5cac1498684adc6926ac58");
	RequireBuild(directory / L"mudGE.dll", "47c34fac01ca7c7fc423c25b944be034931bf01eea404ce841f8b6723fa47a8d");
	RequireBuild(directory / L"pluginpack.dll", "bd2b5b65d5ab8cd505ffc5502c032dc50cbf750e8064e5b85afc53d03465def1");
	if (!SetDllDirectoryW(directory.c_str()))
		throw std::runtime_error("SetDllDirectory failed.");
	Core = LoadLibraryW((directory / L"wtnlib.dll").c_str());
	if (!Core)
		throw std::runtime_error("Cannot load original wtnlib.dll; Win32 error " + std::to_string(GetLastError()));
	Engine = LoadLibraryW((directory / L"mudGE.dll").c_str());
	if (!Engine)
		throw std::runtime_error("Cannot load original mudGE.dll; Win32 error " + std::to_string(GetLastError()));
	Log("Original wtnlib.dll and mudGE.dll loaded.");
}

MudGE::~MudGE()
{
	if (Constructed || Prepared)
	{
		try
		{
			Shutdown();
		}
		catch (...)
		{
			Log("Engine cleanup failed during error unwinding.");
		}
	}
}

void MudGE::Construct()
{
	Symbol<any(__thiscall*)(any, LegacyString)>(Engine, "??0cApplicationEx@@QAE@VcString@@@Z")(Application, String("Sven2 Rebuild Bootstrap"));
	Constructed = true;
	Log("cApplicationEx constructed (208-byte original layout).");
	Symbol<void(__thiscall*)(any)>(Core, "?PreInit@cApplication@@AAEXXZ")(Application);
	Prepared = true;
	Log("PreInit created original plugin and stream managers.");
}

void MudGE::InitializePlugins()
{
	LegacyString url = String((Directory / L"pluginpack.dll").string().c_str(), true);
	bool linked = Symbol<bool(__thiscall*)(any, const LegacyString&)>(Engine, "?AddPluginPack@cApplicationEx@@QAE_NABVcUrl@@@Z")(Application, url);
	DestroyString(url);
	if (!linked)
		throw std::runtime_error("Original pluginpack.dll registration failed.");
	Log("Original pluginpack.dll linked into engine registry.");
}

bytestream MudGE::ReadAsset(const str& path)
{
	return Data.ReadAsset(path);
}

void MudGE::VerifyData()
{
	if (Data.GetPacks().empty())
		throw std::runtime_error("No level packages found in " + Data.GetRoot().string());
	ReadAsset("Sven2/Sven2.xml");
	Log("Loaded folder game data from " + Data.GetRoot().string());
}

const Sprite* MudGE::LoadSprite(const str& path, i16 rows, i16 columns, bool opaque)
{
	const auto key = std::make_tuple(path, rows, columns, opaque);
	auto cached = SpriteCache.find(key);
	if (cached != SpriteCache.end())
		return cached->second;
	bytestream bytes = ReadAsset(path);
	using Microsoft::WRL::ComPtr;
	ComPtr<IWICImagingFactory> factory;
	ComPtr<IWICStream> stream;
	ComPtr<IWICBitmapDecoder> decoder;
	ComPtr<IWICBitmapFrameDecode> frame;
	ComPtr<IWICFormatConverter> converter;
	UINT width = 0, height = 0;
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) || FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory(bytes.data(), static_cast<DWORD>(bytes.size()))) || FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) || FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&width, &height)) || FAILED(factory->CreateFormatConverter(&converter)) || FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)))
		throw std::runtime_error("Cannot decode sprite: " + path);
	if (rows < 1 || columns < 1 || !width || !height || width > 8192 || height > 8192 || width % columns || height % rows || width * height > 16 * 1024 * 1024)
		throw std::runtime_error("Invalid sprite sheet dimensions: " + path);
	Sprite image{ {}, static_cast<i32>(width), static_cast<i32>(width / columns), static_cast<i32>(height / rows), rows, columns, !opaque };
	image.Pixels.resize(width * height);
	if (FAILED(converter->CopyPixels(nullptr, width * 4, width * height * 4, reinterpret_cast<u8*>(image.Pixels.data()))))
		throw std::runtime_error("Cannot read sprite pixels: " + path);
	// The original 16-bit surface keys RGB565 fuchsia. Some palettes (notably
	// noanim.bmp) use (255,1,255), which is the same key after RGB565 conversion.
	if (!opaque)
		for (u32& pixel : image.Pixels)
			if ((pixel & 0x00F8FCF8) == 0x00F800F8)
				pixel = 0x00FF00FF;
	// The menu's keyed background is JPEG, so its magenta varies after decoding.
	if (!opaque && path.find(".jpg") != str::npos)
		for (u32& pixel : image.Pixels)
			if (((pixel >> 16) & 255) > 185 && ((pixel >> 8) & 255) < 75 && (pixel & 255) > 185)
				pixel = 0xff00ff;
	Sprites.push_back(std::move(image));
	const Sprite* loaded = &Sprites.back();
	SpriteCache.emplace(key, loaded);
	return loaded;
}

any MudGE::Video() const
{
	return *reinterpret_cast<any const*>(Application + 0xCC);
}

void MudGE::StartVideo(bool input)
{
	if (!Initialize(4, "cDirectDraw7"))
	{
		LegacyString url = String((Directory / L"directX3.dll").string().c_str(), true);
		bool linked = Symbol<bool(__thiscall*)(any, const LegacyString&)>(Engine, "?AddPluginPack@cApplicationEx@@QAE_NABVcUrl@@@Z")(Application, url);
		DestroyString(url);
		if (!linked || !Initialize(4, "cDirectDraw3"))
			throw std::runtime_error("Neither original DirectDraw backend could initialize.");
	}
	if (input)
	{
		RequireBuild(Directory / L"directX3.dll", "cef253664432719da7d7a6a5cf45a6d25d21bda7882112c42d8f9cbc647d113d");
		LegacyString url = String((Directory / L"directX3.dll").string().c_str(), true);
		bool linked = Symbol<bool(__thiscall*)(any, const LegacyString&)>(Engine, "?AddPluginPack@cApplicationEx@@QAE_NABVcUrl@@@Z")(Application, url);
		DestroyString(url);
		if (!linked || !Initialize(2, "cDirectInputMouse3") || !Initialize(1, "cDirectInputKeyboard3"))
			throw std::runtime_error("Original DirectInput3 backend is unavailable.");
	}
	// StartVideo also initializes the already-selected input devices against its native window.
	Symbol<void(__thiscall*)(any, u16, u16, u8, bool, i32, LegacyString)>(Engine, "?StartVideo@cApplicationEx@@QAEXGGE_NHVcString@@@Z")(Application, 800, 600, 32, false, 104, String("Sven2 rebuild - original engine bootstrap"));
	VideoStarted = true;
	Log("StartVideo returned: 800x600, 32-bit requested, windowed.");
}

bool MudGE::StartAudio()
{
	if (!std::filesystem::exists(Directory / L"DX_SoundSystemPlugin.dll"))
	{
		Log("Optional sound plugin is missing.");
		return false;
	}
	RequireBuild(Directory / L"DX_SoundSystemPlugin.dll", "17221b94502670c37e426750457baa55e96ae883109f98f62a4d5b6bc2bc5546");
	LegacyString url = String((Directory / L"DX_SoundSystemPlugin.dll").string().c_str(), true);
	bool linked = Symbol<bool(__thiscall*)(any, const LegacyString&)>(Engine, "?AddPluginPack@cApplicationEx@@QAE_NABVcUrl@@@Z")(Application, url);
	DestroyString(url);
	if (!linked || !Initialize(6, "cDX_SoundSystem"))
		return false;
	bool result = Symbol<bool(__thiscall*)(any, i32, i32, bool)>(Engine, "?StartSound@cApplicationEx@@QAE_NHH_N@Z")(Application, 44100, 32, true);
	if (result)
		SoundSystem = *reinterpret_cast<any*>(Application + 0xC4);
	Log(str("StartSound: ") + (result ? "OK" : "unavailable"));
	return result;
}

bool MudGE::PlaySample(const str& path, bool looped)
{
	if (!SoundSystem)
		return false;
	auto [sample, inserted] = Samples.try_emplace(std::make_pair(path, looped), nullptr);
	if (inserted)
	{
		LegacyString url = String(Data.AssetPath(path).string().c_str(), true);
		// Sven2_Audio_LoadSoundsFromXml uses the sample loader at slot +0x68.
		try
		{
			sample->second = Method<any(__thiscall*)(any, const LegacyString&)>(SoundSystem, 0x68)(SoundSystem, url);
		}
		catch (...)
		{
			DestroyString(url);
			throw;
		}
		DestroyString(url);
		if (!sample->second)
			Log("Sound sample unavailable: " + path);
	}
	if (!sample->second)
		return false;
	if (looped)
	{
		// Original 0x422D23..0x422D2E: sample slot +0x40 ORs flag 2,
		// which DX_SoundSystemPlugin maps to DSBPLAY_LOOPING.
		StopLoopedSound();
		Method<void(__thiscall*)(any, i32)>(sample->second, 0x40)(sample->second, 0x02);
	}
	any channel = Method<any(__thiscall*)(any)>(sample->second, 0x54)(sample->second);
	if (looped)
		LoopedChannel = channel;
	if (looped && channel)
		Log("Started looping sound.");
	return channel != nullptr;
}

void MudGE::StopLoopedSound()
{
	if (!LoopedChannel)
		return;
	// Original Sven2_Audio_StopLoopedSound, 0x424070: channel slot +0x10.
	Method<void(__thiscall*)(any)>(LoopedChannel, 0x10)(LoopedChannel);
	LoopedChannel = nullptr;
	Log("Stopped looping sound.");
}

bool MudGE::LoopedSoundPlaying() const
{
	return LoopedChannel && Method<bool(__thiscall*)(any)>(LoopedChannel, 0x0C)(LoopedChannel);
}

bool MudGE::PlayMusic(const str& path)
{
	if (!SoundSystem)
		return false;
	StopMusic();
	LegacyString url = String(Data.AssetPath(path).string().c_str(), true);
	any loaded = nullptr;
	try
	{
		loaded = Method<any(__thiscall*)(any, const LegacyString&, i32)>(SoundSystem, 0x70)(SoundSystem, url, 1);
	}
	catch (...)
	{
		DestroyString(url);
		throw;
	}
	DestroyString(url);
	MusicStream = loaded;
	if (!MusicStream)
	{
		Log("Music stream unavailable: " + path);
		return false;
	}
	if (!Method<any(__thiscall*)(any)>(MusicStream, 0x20)(MusicStream))
	{
		Log("Music playback unavailable: " + path);
		StopMusic();
		return false;
	}
	Log("Started music stream: " + path);
	return true;
}

void MudGE::StopMusic()
{
	if (!MusicStream)
		return;
	Method<void(__thiscall*)(any)>(MusicStream, 0x1C)(MusicStream);
	// DX_SoundSystemPlugin::LoadStream permits only four allocated streams
	// (0x100039FF). Stop alone leaves that slot occupied. Its deleting
	// destructor unregisters the stream and its channel (0x10002290).
	Method<any(__thiscall*)(any, u32)>(MusicStream, 0x00)(MusicStream, 0x01);
	MusicStream = nullptr;
	Log("Stopped music stream.");
}

void MudGE::Tick()
{
	Symbol<void(__thiscall*)(any)>(Core, "?Tick@cApplication@@UAEXXZ")(Application);
}

HWND MudGE::WindowHandle() const
{
	if (Presenter)
		return Presenter->Window();
	any device = Video();
	any window = Method<any(__thiscall*)(any)>(device, 0x18)(device);
	return Symbol<HWND(__thiscall*)(const any)>(Core, "?GetHandle@cWindow@@QBEPAUHWND__@@XZ")(window);
}

void MudGE::StartScaledVideo(i32 width, i32 height, ScalingFilter filter, f32 sharpness)
{
	if (!VideoStarted || Presenter)
		throw std::runtime_error("Start native video once before enabling scaled presentation.");
	HWND original = WindowHandle();
	Presenter = std::make_unique<FsrPresenter>(width, height, filter, sharpness);
	ScaledFrame.resize(800 * 600);
	ShowWindow(original, SW_HIDE);
	Log("Scaled presentation: D3D11, AMD FSR1 EASU + RCAS available; logical frame stays 800x600.");
}

bool MudGE::ExitRequested()
{
	return (Presenter && Presenter->Closed()) || Symbol<bool(__thiscall*)(any)>(Core, "?GetExitRequest@cApplication@@QAE_NXZ")(Application);
}

void MudGE::DrawFrame(const std::vector<SpriteDraw>& scene, cstr captureOverride)
{
	any device = Video();
	// Original game at 0x410DD0 asks video slot +0x1C for surface 2.
	// Slot +0x50 is pixel-format metadata, not a drawing surface.
	any surface = Presenter ? nullptr : Method<any(__thiscall*)(any, i32)>(device, 0x1c)(device, 2);
	auto get = [&](cstr name)
	{ return Symbol<i32(__thiscall*)(any)>(Engine, name)(surface); };
	i32 width = Presenter ? 800 : get("?GetWidth@cBitmap@@QBEHXZ");
	i32 height = Presenter ? 600 : get("?GetHeight@cBitmap@@QBEHXZ");
	i32 bpp = Presenter ? 32 : get("?GetBPP@cBitmap@@QBEHXZ");
	if (Presenter)
		std::fill(ScaledFrame.begin(), ScaledFrame.end(), 0);
	u8* pixels = Presenter ? reinterpret_cast<u8*>(ScaledFrame.data()) : static_cast<u8*>(Symbol<any(__thiscall*)(any)>(Engine, "?Lock@cSurface@@QAEPAXXZ")(surface));
	i32 pitch = Presenter ? 800 * 4 : get("?GetPitch@cBitmap@@QBEHXZ");
	if (!pixels || width < 1 || width > 8192 || height < 1 || height > 8192 || (bpp != 16 && bpp != 32) || pitch < width * (bpp / 8))
		throw std::runtime_error("Unexpected native drawing surface layout.");
	if (Frames == 0)
		Log(str(Presenter ? "Composition surface: " : "Native surface: ") + std::to_string(width) + "x" + std::to_string(height) + ", bpp=" + std::to_string(bpp) + ", pitch=" + std::to_string(pitch));
	for (const SpriteDraw& draw : scene)
	{
		const Sprite& sprite = *draw.Sprite;
		if (draw.Frame < 0 || draw.Frame >= sprite.Rows * sprite.Columns)
			continue;
		const i32 sourceX = draw.Frame % sprite.Columns * sprite.FrameWidth;
		const i32 sourceY = draw.Frame / sprite.Columns * sprite.FrameHeight;
		const i32 left = std::max(0, -draw.X);
		const i32 right = std::min(sprite.FrameWidth, width - draw.X);
		const i32 clipBottom = draw.ClipBottom > 0 ? std::min(height, draw.ClipBottom) : height;
		const i32 top = std::max(0, -draw.Y);
		const i32 bottom = std::min(sprite.FrameHeight, clipBottom - draw.Y);
		if (left >= right || top >= bottom)
			continue;
		for (i32 y = top; y < bottom; ++y)
		{
			const u32* source = sprite.Pixels.data() + (sourceY + y) * sprite.Width + sourceX;
			u8* destination = pixels + (draw.Y + y) * pitch;
			if (!sprite.Keyed && !draw.Mirror && bpp == 32)
			{
				std::memcpy(destination + (draw.X + left) * 4, source + left, (right - left) * 4);
				continue;
			}
			for (i32 x = left; x < right; ++x)
			{
				const i32 sample = draw.Mirror ? sprite.FrameWidth - 1 - x : x;
				const u32 rgb = source[sample] & 0xFFFFFF;
				if (sprite.Keyed && rgb == 0xFF00FF)
					continue;
				if (bpp == 32)
					reinterpret_cast<u32*>(destination)[draw.X + x] = rgb;
				else
					reinterpret_cast<u16*>(destination)[draw.X + x] = static_cast<u16>(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
			}
		}
	}
	if (captureOverride)
	{
		BITMAPFILEHEADER header{};
		header.bfType = 0x4D42;
		header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
		header.bfSize = header.bfOffBits + width * height * 4;
		BITMAPINFOHEADER info{};
		info.biSize = sizeof(info);
		info.biWidth = width;
		info.biHeight = -height;
		info.biPlanes = 1;
		info.biBitCount = 32;
		info.biSizeImage = width * height * 4;
		cstr capturePath = captureOverride;
		std::ofstream capture(capturePath, std::ios::binary);
		capture.write(reinterpret_cast<cstr>(&header), sizeof(header));
		capture.write(reinterpret_cast<cstr>(&info), sizeof(info));
		for (i32 y = 0; y < height; ++y)
		{
			for (i32 x = 0; x < width; ++x)
			{
				u32 color;
				if (bpp == 32)
					color = reinterpret_cast<const u32*>(pixels + y * pitch)[x];
				else
				{
					const u16 c = reinterpret_cast<const u16*>(pixels + y * pitch)[x];
					color = (((c >> 11) & 31) * 255 / 31 << 16) | (((c >> 5) & 63) * 255 / 63 << 8) | ((c & 31) * 255 / 31);
				}
				capture.write(reinterpret_cast<cstr>(&color), 4);
			}
		}
		if (!capture)
			throw std::runtime_error("Cannot write native-surface capture.");
		Log(str("Captured first native drawing surface to ") + capturePath + ".");
	}
	if (Presenter)
	{
		Presenter->Present(ScaledFrame.data(), CaptureNext);
		CaptureNext.clear();
	}
	else
	{
		Symbol<void(__thiscall*)(any)>(Engine, "?Unlock@cSurface@@QAEXXZ")(surface);
		Method<void(__thiscall*)(any)>(device, 0x34)(device);
		Method<void(__thiscall*)(any, i32)>(device, 0x38)(device, 1);
	}
	++Frames;
}

void MudGE::Shutdown()
{
	Presenter.reset();
	StopLoopedSound();
	StopMusic();
	if (VideoStarted)
		Symbol<void(__thiscall*)(any)>(Engine, "?Exit@cApplicationEx@@UAEXXZ")(Application);
	if (Prepared)
	{
		Symbol<void(__thiscall*)(any)>(Core, "?Shutdown@cApplication@@QAEXXZ")(Application);
		Symbol<void(__thiscall*)(any)>(Core, "?PostExit@cApplication@@AAEXXZ")(Application);
		Prepared = false;
	}
	if (Constructed)
	{
		Symbol<void(__thiscall*)(any)>(Engine, "??1cApplicationEx@@UAE@XZ")(Application);
		Constructed = false;
	}
	Log("Engine shutdown completed.");
	// Keep DLLs resident until process exit: registered legacy static destructors refer to them.
}

LegacyString MudGE::String(cstr text, bool url)
{
	LegacyString result;
	Symbol<any(__thiscall*)(any, cstr)>(Core, url ? "??0cUrl@@QAE@PBD@Z" : "??0cString@@QAE@PBD@Z")(&result, text);
	return result; // Bit-copy transfers engine-owned payload; no host-side destructor.
}

void MudGE::DestroyString(LegacyString& text)
{
	Symbol<void(__thiscall*)(any)>(Core, "??1cString@@UAE@XZ")(&text);
}

bool MudGE::Initialize(i32 device, cstr name)
{
	bool ok = Symbol<bool(__thiscall*)(any, i32, LegacyString)>(Engine, "?Initialize@cApplicationEx@@QAE_NW4EDeviceTypes@1@VcString@@@Z")(Application, device, String(name));
	Log(str("Initialize ") + name + ": " + (ok ? "OK" : "unavailable"));
	return ok;
}

template <class T>
T MudGE::Symbol(HMODULE module, cstr name) const
{
	FARPROC address = GetProcAddress(module, name);
	if (!address)
		throw std::runtime_error(str("Missing engine export: ") + name);
	return reinterpret_cast<T>(address);
}

template <class T>
T MudGE::Method(any object, u32 offset)
{
	if (!object)
		throw std::runtime_error("Engine returned a null interface.");
	return reinterpret_cast<T>((*reinterpret_cast<any**>(object))[offset / sizeof(any)]);
}
