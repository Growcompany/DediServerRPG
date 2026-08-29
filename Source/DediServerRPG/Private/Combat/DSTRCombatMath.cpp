#include "Combat/DSTRCombatMath.h"

bool FDSTRCombatMath::IsInFrontCone(
	const FVector& Forward,
	const FVector& ToTarget,
	const float HalfAngleDegrees)
{
	const FVector Facing = Forward.GetSafeNormal2D();
	const FVector Direction = ToTarget.GetSafeNormal2D();
	if (Facing.IsNearlyZero() || Direction.IsNearlyZero())
	{
		return true;
	}
	const float HalfAngle = FMath::Clamp(HalfAngleDegrees, 0.0f, 180.0f);
	return FVector::DotProduct(Facing, Direction)
		>= FMath::Cos(FMath::DegreesToRadians(HalfAngle)) - KINDA_SMALL_NUMBER;
}

bool FDSTRCombatMath::IsHit(
	const FVector& Forward,
	const FVector& ToTarget,
	const float HitDistance,
	const float ConeHalfAngleDegrees)
{
	if (ToTarget.SizeSquared() > FMath::Square(FMath::Max(HitDistance, 0.0f)))
	{
		return false;
	}
	return ConeHalfAngleDegrees <= 0.0f
		|| IsInFrontCone(Forward, ToTarget, ConeHalfAngleDegrees);
}
