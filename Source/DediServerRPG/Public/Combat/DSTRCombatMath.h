#pragma once

#include "CoreMinimal.h"

struct DEDISERVERRPG_API FDSTRCombatMath
{
	static constexpr float MeleeConeHalfAngleDegrees = 60.0f;

	static bool IsInFrontCone(const FVector& Forward, const FVector& ToTarget, float HalfAngleDegrees);

	static bool IsHit(const FVector& Forward, const FVector& ToTarget, float HitDistance, float ConeHalfAngleDegrees);
};
