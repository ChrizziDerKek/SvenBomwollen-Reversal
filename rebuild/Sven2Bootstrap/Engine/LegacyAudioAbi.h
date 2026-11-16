#pragma once
#include "Types.h"

// Verified slots of the supported original DLL build. These are binary ABI
// requirements, not gameplay policy or values inferred from a resource name.
namespace LegacyAudioAbi
{
inline constexpr u32 LoadSample = 0x68;
inline constexpr u32 LoadStream = 0x70;
inline constexpr u32 SetSampleFlags = 0x40;
inline constexpr i32 LoopingSampleFlag = 2;
inline constexpr u32 PlaySample = 0x54;
inline constexpr u32 StopChannel = 0x10;
inline constexpr u32 ChannelPlaying = 0x0C;
inline constexpr u32 StreamPlaying = 0x20;
inline constexpr u32 StopStream = 0x1C;
inline constexpr u32 DestroyStream = 0;
inline constexpr u32 DeleteObject = 1;
} // namespace LegacyAudioAbi
