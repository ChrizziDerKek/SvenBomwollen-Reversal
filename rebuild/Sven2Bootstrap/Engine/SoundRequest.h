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
	SoundRequest(str asset, SoundPlayback playback = SoundPlayback::OneShot)
	    : Asset(std::move(asset)), Playback(playback)
	{
	}
	SoundRequest(cstr asset)
	    : SoundRequest(str(asset))
	{
	}
	str Asset;
	SoundPlayback Playback;
};
