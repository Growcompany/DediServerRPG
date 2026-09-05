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

	// 도약 시간을 몽타주 임팩트에 묶으면 600유닛을 그 안에 이동해 순간이동으로 보인다.

	static constexpr float ChargeDistance = 600.0f;
	static constexpr float ChargeDuration = 0.5f;
	// RMB 대지 강타: 제자리 광역 + 넉백.
	static constexpr float SmashRadius = 320.0f;
	static constexpr float SmashDamageMultiplier = 1.8f;
	static constexpr float SmashKnockbackSpeed = 420.0f;
	static constexpr float SmashStaggerSeconds = 0.5f;
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

	static FVector KnockbackVelocity(
		const FVector& Center, const FVector& TargetLocation, const FVector& Forward, float Speed);
};
