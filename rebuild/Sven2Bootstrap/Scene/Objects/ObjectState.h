#pragma once
#include "Types.h"

// The native object record reuses the same state word across object classes.
// Aliases describe that word in its owning character/effect's vocabulary.
enum class ActorState : i32
{
	Idle = 0,
	Walking = 1,
	Chewing = 2,
	Turning = 3,
	AnimalWalking = 4,
	Sleeping = 10,
	Resolved = 10,
	FarmerWaking = 11,
	DemonTransforming = 11,
	DistractionPrimary = 12,
	DogDigging = 12,
	FarmerDrinking = 12,
	GirlTeddyBack = 12,
	DemonWalking = 12,
	DistractionSecondary = 13,
	GirlTeddyFront = 13,
	FarmerLyingDown = 13,
	DogStunned = 13,
	DemonShocking = 13,
	FarmerGrumblingBack = 14,
	DemonRecovering = 14,
	FarmerGrumblingFront = 15,
	AwaitingUfo = 16,
	BeamingUp = 17,
	GirlAboardUfo = 18,
	GirlBeamingDown = 19,
	FightingBack = 20,
	FightingFront = 21,
	GirlPlayingDogBack = 22,
	GirlPlayingDogFront = 23,
	SceneryMoving = 1,
	DigAppearing = 0,
	DigHolding = 1,
	DigDisappearing = 2
};

inline bool IsDemonState(ActorState state)
{
	return state == ActorState::DemonTransforming || state == ActorState::DemonWalking || state == ActorState::DemonShocking || state == ActorState::DemonRecovering;
}

enum class UfoState : i32
{
	Inactive = -1,
	Arriving = 0,
	Beaming = 1,
	Departing = 2,
	SelectingAnimal = 3,
	MovingToAnimal = 4
};
