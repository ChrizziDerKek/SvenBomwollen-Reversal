#pragma once
#include "Types.h"

enum class PlayerActionState : i32
{
	Idle = 0,
	Interaction = 1,
	Whistle = 2,
	Kicked = 3,
	Frigida = 4,
	Distraction,
	BonusExit
};

enum class PlayerAnimation
{
	Walk,
	Whistle,
	WhistleDiagonal,
	Teleport,
	ElectricShock,
	KickFront,
	KickBack,
	Frigida,
	BeamUp,
	BeamDown
};

// Render-facing order; native motion codes are mapped in player/README.md.
enum class PlayerMotion : i32
{
	Stopped = -1,
	North,
	NorthEast,
	East,
	SouthEast,
	South,
	SouthWest,
	West,
	NorthWest
};
