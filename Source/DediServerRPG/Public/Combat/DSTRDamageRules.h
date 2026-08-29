#pragma once

#include "CoreMinimal.h"

struct DEDISERVERRPG_API FDSTRDamageRules
{
	static constexpr float FortifiedDamageScale = 0.4f;
	static float FortifiedMultiplier(bool bFortified);

	static bool IsImmuneWhileDormant(bool bTargetDormant, bool bTargetBoss);

	static constexpr float FortifyDurationSeconds = 3.0f;
	static constexpr float FortifyTauntRadius = 800.0f;
	static constexpr float FortifyTauntThreat = 100.0f;

	static constexpr float MakeWayLeapDistance = 600.0f;
	static constexpr float MakeWayTargetSearchRadius = 700.0f;
	static constexpr float MakeWayBotAimToleranceDegrees = 15.0f;
	static constexpr float MakeWayBotMinDistance = 400.0f;
	static constexpr float MakeWayLeapHeight = 250.0f;
	static constexpr float MakeWayLandingRadius = 250.0f;
	static constexpr float MakeWayKnockbackSpeed = 300.0f;
	static constexpr float MakeWayDamageMultiplier = 2.5f;
	static constexpr float MakeWayStaggerSeconds = 0.6f;

	static constexpr float ChargeDistance = 600.0f;
	static constexpr float ChargeDuration = 0.5f;
	static constexpr float ChargeDamageMultiplier = 1.5f;
	static constexpr float ChargeStaggerSeconds = 0.4f;
	static constexpr float ChargeSweepRadius = 90.0f;
	static bool IsInChargeSweep(float Distance2D, float TargetCapsuleRadius);
	static constexpr float ChargeBotEnemyRadius = 600.0f;
	static constexpr int32 ChargeBotEnemyCount = 3;
	static bool ShouldBotCharge(int32 EnemiesInRadius, bool bChargeReady);

	static constexpr float ReckoningRadius = 450.0f;
	static constexpr float ReckoningDamageMultiplier = 6.0f;
	static constexpr float ReckoningStunSeconds = 1.5f;
	static constexpr float ReckoningWarningSeconds = 1.0f;

	static constexpr float InteractReach = 250.0f;

	static constexpr float SlowedMoveScale = 0.5f;
	static constexpr float SlowDurationSeconds = 2.0f;

	static bool ShouldBotLeap(float Distance2D, float AimErrorDegrees, bool bLeapReady);
	static bool IsInLandingRadius(float Distance2D, float TargetCapsuleRadius);
	static FVector KnockbackVelocity(
		const FVector& Center, const FVector& TargetLocation, const FVector& Forward, float Speed);
};
