#include "Combat/DSTRDamageRules.h"

float FDSTRDamageRules::FortifiedMultiplier(const bool bFortified)
{
	return bFortified ? FortifiedDamageScale : 1.0f;
}

bool FDSTRDamageRules::IsImmuneWhileDormant(const bool bTargetDormant, const bool bTargetBoss)
{
	return bTargetDormant && bTargetBoss;
}

bool FDSTRDamageRules::IsInChargeSweep(const float Distance2D, const float TargetCapsuleRadius)
{
	return Distance2D <= ChargeSweepRadius + FMath::Max(0.0f, TargetCapsuleRadius);
}

bool FDSTRDamageRules::ShouldBotCharge(const int32 EnemiesInRadius, const bool bChargeReady)
{
	return bChargeReady && EnemiesInRadius >= ChargeBotEnemyCount;
}

bool FDSTRDamageRules::ShouldBotLeap(
	const float Distance2D,
	const float AimErrorDegrees,
	const bool bLeapReady)
{
	return bLeapReady
		&& Distance2D > MakeWayBotMinDistance
		&& Distance2D <= MakeWayTargetSearchRadius
		&& FMath::Abs(AimErrorDegrees) <= MakeWayBotAimToleranceDegrees;
}

bool FDSTRDamageRules::IsInLandingRadius(const float Distance2D, const float TargetCapsuleRadius)
{
	return Distance2D <= MakeWayLandingRadius + FMath::Max(0.0f, TargetCapsuleRadius);
}

FVector FDSTRDamageRules::KnockbackVelocity(
	const FVector& Center,
	const FVector& TargetLocation,
	const FVector& Forward,
	const float Speed)
{
	FVector Direction = (TargetLocation - Center).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Forward.GetSafeNormal2D();
	}
	return Direction * FMath::Max(0.0f, Speed);
}
