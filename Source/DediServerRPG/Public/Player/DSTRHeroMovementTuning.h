#pragma once

#include "CoreMinimal.h"

struct DEDISERVERRPG_API FDSTRHeroMovementTuning
{
	static constexpr float MaxWalkSpeed = 500.0f;
	static constexpr float MaxAcceleration = 4096.0f;
	static constexpr float BrakingDecelerationWalking = 4096.0f;
	static constexpr float GroundFriction = 10.0f;
	static constexpr bool bUseSeparateBrakingFriction = true;
	static constexpr float BrakingFriction = 8.0f;
	static constexpr float RotationRateYaw = 900.0f;
};
