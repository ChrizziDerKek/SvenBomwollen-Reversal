#pragma once
#include "Types.h"
#include <utility>

enum class SoundPlayback
{
	OneShot,
	Loop
};

struct SoundRequest
{
	SoundRequest(str asset, SoundPlayback playback = SoundPlayback::OneShot, i32 frequency = 0)
	    : Asset(std::move(asset)), Playback(playback), Frequency(frequency)
	{}

	SoundRequest(cstr asset)
	    : SoundRequest(str(asset))
	{}

	str Asset;
	SoundPlayback Playback;
	i32 Frequency; // Zero keeps the sample's native rate; otherwise Hz.
};
