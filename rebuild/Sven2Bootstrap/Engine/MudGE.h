#include "Types.h"
#include "Utility/Logger.h"
#pragma once
#include "GameData.h"
#include <windows.h>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

class MudGE
{
public:
	MudGE(const fpath& directory, const fpath& dataDirectory);
	~MudGE();
	MudGE(const MudGE&) = delete;
	MudGE& operator=(const MudGE&) = delete;
	void Construct();
	void InitializePlugins();
	void VerifyData();
	const GameData& GetData() const
	{
		return Data;
	}
	bytestream ReadAsset(const str& path);
	const Sprite* LoadSprite(const str& path, i16 rows, i16 columns, bool opaque = false);
	void StartVideo(bool input = true);
	void StartScaledVideo(i32 width, i32 height, ScalingFilter filter, f32 sharpness);
	bool ScaledVideo() const
	{
		return Presenter != nullptr;
	}
	void CaptureNextFrame(const fpath& path)
	{
		CaptureNext = path;
	}
	bool StartAudio();
	bool PlaySample(const str& path, bool looped = false);
	void StopLoopedSound();
	bool LoopedSoundPlaying() const;
	bool PlayMusic(const str& path);
	void StopMusic();
	void Tick();
	HWND WindowHandle() const;
	bool ExitRequested();
	void DrawFrame(const std::vector<SpriteDraw>& scene, cstr capture = nullptr);
	void Shutdown();
	any Video() const;
	u32 GetFrameCount() const
	{
		return Frames;
	}

private:
	u32 Frames = 0;
	HMODULE Core = nullptr;
	HMODULE Engine = nullptr;
	std::unique_ptr<FsrPresenter> Presenter;
	std::vector<u32> ScaledFrame;
	fpath CaptureNext;
	fpath Directory;
	GameData Data;
	std::deque<Sprite> Sprites;
	std::map<std::tuple<str, i16, i16, bool>, const Sprite*> SpriteCache;
	any SoundSystem = nullptr;
	any LoopedChannel = nullptr;
	any MusicStream = nullptr;
	std::map<std::pair<str, bool>, any> Samples;
	alignas(4) u8 Application[0xD0]{};
	bool Constructed = false;
	bool Prepared = false;
	bool VideoStarted = false;

	LegacyString String(cstr text, bool url = false);
	void DestroyString(LegacyString& text);
	bool Initialize(i32 device, cstr name);

	template <class T>
	T Symbol(HMODULE module, cstr name) const;

	template <class T>
	static T Method(any object, u32 offset);
};
