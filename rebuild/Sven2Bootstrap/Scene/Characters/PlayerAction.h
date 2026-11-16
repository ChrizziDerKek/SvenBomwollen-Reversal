#pragma once
#include "Types.h"

enum class PlayerActionState : i32
{
	Idle = 0,
	Interaction = 1,
	Whistle = 2,
	Kicked = 3,
	Frigida = 4
};

namespace PlayerActions
{
inline constexpr i32 Idle = static_cast<i32>(PlayerActionState::Idle);
inline constexpr i32 Interaction = static_cast<i32>(PlayerActionState::Interaction);
inline constexpr i32 Whistle = static_cast<i32>(PlayerActionState::Whistle);
inline constexpr i32 Kicked = static_cast<i32>(PlayerActionState::Kicked);
inline constexpr i32 Frigida = static_cast<i32>(PlayerActionState::Frigida);
} // namespace PlayerActions
