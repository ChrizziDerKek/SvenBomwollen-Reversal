#include "Utility/Logger.h"
#include "Engine/MudGE.h"
#include "Game/GameApp.h"
#include "FSR1/FsrPresenter.h"
#include <algorithm>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <objbase.h>

static i32 Bootstrap(i32 argc, char** argv)
{
	wchar_t executable[32768]{};
	GetModuleFileNameW(nullptr, executable, 32768);
	fpath root = fpath(executable).parent_path().parent_path().parent_path().parent_path().parent_path();
	fpath game = root / L"games" / L"sven2";
	fpath dataDirectory = root / L"assets";
	bool probe = false, audio = true, nativeInput = false, directTutorial = false;
	u32 seconds = 0;
	bool scaled = true;
	i32 outputWidth = 1280, outputHeight = 960;
	f32 sharpness = 0.5f;
	ScalingFilter filter = ScalingFilter::FSR1;
	fpath capture;
	for (i32 i = 1; i < argc; ++i)
	{
		const str option = argv[i];
		if (option == "--probe")
			probe = true;
		else if (option == "--upscaler" && i + 1 < argc)
		{
			const str mode = argv[++i];
			scaled = mode != "native";
			if (mode == "fsr1")
				filter = ScalingFilter::FSR1;
			else if (mode == "nearest")
				filter = ScalingFilter::Nearest;
			else if (mode == "bilinear")
				filter = ScalingFilter::Bilinear;
			else if (mode != "native")
				throw std::runtime_error("Upscaler must be native, nearest, bilinear or fsr1.");
		}
		else if (option == "--output-width" && i + 1 < argc)
			outputWidth = std::stoi(argv[++i]);
		else if (option == "--output-height" && i + 1 < argc)
			outputHeight = std::stoi(argv[++i]);
		else if (option == "--sharpness" && i + 1 < argc)
			sharpness = std::stof(argv[++i]);
		else if (option == "--capture" && i + 1 < argc)
			capture = std::filesystem::absolute(argv[++i]);
		else if (option == "--tutorial")
			directTutorial = true;
		else if (option == "--no-audio")
			audio = false;
		else if (option == "--no-input")
			nativeInput = false;
		else if (option == "--native-input")
			nativeInput = true;
		else if (option == "--seconds" && i + 1 < argc)
			seconds = std::stoul(argv[++i]);
		else if (option == "--data-dir" && i + 1 < argc)
			dataDirectory = std::filesystem::absolute(argv[++i]);
		else if (option == "--game-dir" && i + 1 < argc)
			game = std::filesystem::absolute(argv[++i]);
		else if (option == "--help")
		{
			std::cout << "Sven2Bootstrap [--tutorial] [--data-dir PATH] [--game-dir PATH] [--no-audio]\n"
			          << "Graphics: --upscaler native|fsr1|nearest|bilinear (default: fsr1) --output-width N --output-height N --sharpness 0..2\n"
			          << "Scaled mode: F6 filter; F7 softer; F8 sharper; F11 borderless fullscreen. --capture FILE writes the first scaled frame.\n"
			          << "Diagnostics: --probe, --seconds N, --native-input\n";
			return 0;
		}
		else
		{
			throw std::runtime_error("Unknown or incomplete option: " + option);
		}
	}
	const fpath runtime = root / L"rebuild" / L"runtime" / L"sven2";
	std::filesystem::create_directories(runtime);
	std::filesystem::current_path(runtime);
	InitLogger(runtime, probe);
	Log("Sven2 rebuild / Win32");
	HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(com))
		throw std::runtime_error("Cannot initialize COM for game devices.");
	struct ComLifetime
	{
		~ComLifetime()
		{
			CoUninitialize();
		}
	} apartment;
	MudGE engine(game, dataDirectory);
	engine.Construct();
	engine.InitializePlugins();
	engine.VerifyData();
	if (!probe)
	{
		if (scaled)
			SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
		engine.StartVideo(nativeInput);
		if (scaled)
			engine.StartScaledVideo(outputWidth, outputHeight, filter, sharpness);
		if (!capture.empty())
		{
			if (!scaled)
				throw std::runtime_error("--capture requires a scaled presentation mode.");
			engine.CaptureNextFrame(capture);
		}
		const bool audioReady = audio && engine.StartAudio();
		if (audio && !audioReady)
			Log("Audio device unavailable.");
		RunGame(engine, runtime / L"saves", audioReady, seconds, directTutorial);
	}
	engine.Shutdown();
	Log("PASS");
	return 0;
}
static LONG Crash(EXCEPTION_POINTERS* e)
{
	if (e->ExceptionRecord->ExceptionCode == 0xe06d7363)
		return EXCEPTION_CONTINUE_SEARCH;
	char text[256];
	std::snprintf(text, sizeof(text), "Native failure: code=0x%08lX address=%p", e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress);
	Log(text);
	return EXCEPTION_EXECUTE_HANDLER;
}
static i32 Guarded(i32 argc, char** argv)
{
	__try
	{
		return Bootstrap(argc, argv);
	}
	__except (Crash(GetExceptionInformation()))
	{
		return 2;
	}
}
i32 main(i32 argc, char** argv)
{
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
	try
	{
		return Guarded(argc, argv);
	}
	catch (const std::exception& e)
	{
		Log(str("ERROR: ") + e.what());
		return 1;
	}
	catch (...)
	{
		Log("ERROR: legacy engine exception; see the engine HTML log in rebuild/runtime/sven2.");
		return 2;
	}
}
