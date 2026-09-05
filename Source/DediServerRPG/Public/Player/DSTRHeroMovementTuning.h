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
	static constexpr float SprintMoveScale = 1.5f;
	// 스태미나 재생이 +15/초라 소모가 그보다 커야 실제로 줄어든다(순 -10/초).
	static constexpr float SprintStaminaPerSecond = 25.0f;
	static constexpr float SprintCostPeriod = 0.25f;
};
