#pragma once

#include "CoreMinimal.h"
#include "DSTRBossSkillRules.generated.h"

UENUM()
enum class EDSTRBossSkill : uint8
{
	None,
	Swing,
	ColossalBlow,
	PhantomRush,
	Siphon,
	Subjugate
};

struct FDSTRBossSkillInput
{
	bool bHasTarget = false;
	float Distance2D = 0.0f;
	float AttackRange = 0.0f;
	bool bSwingReady = true;
	bool bColossalReady = true;
	bool bRushReady = true;
	bool bSiphonReady = true;
	bool bSubjugateReady = true;
	int32 SwingsSinceColossal = 0;
	int32 SwingsSinceSubjugate = 0;
	int32 PlayersInCrowdRadius = 0;
};

struct DEDISERVERRPG_API FDSTRBossSkillRules
{
	static constexpr float RushMinDistance = 600.0f;
	static constexpr float RushMaxDistance = 1500.0f;
	static constexpr float RushDistance = 900.0f;
	static constexpr float RushDuration = 0.6f;
	static constexpr float RushDamageMultiplier = 1.0f;
	static constexpr float RushSweepRadius = 120.0f;

	static constexpr float SiphonMinDistance = 300.0f;
	static constexpr float SiphonMaxDistance = 800.0f;
	static constexpr float SiphonWarningSeconds = 0.4f;
	static constexpr float SiphonPullSeconds = 1.0f;
	static constexpr float SiphonPullDistance = 400.0f;
	static constexpr float SiphonMinPullDistance = 150.0f;
	static constexpr float SiphonDamageMultiplier = 1.25f;

	static constexpr int32 ColossalEveryNthSwing = 3;
	static constexpr float ColossalWarningSeconds = 0.6f;
	static constexpr float ColossalReach = 260.0f;
	static constexpr float ColossalConeHalfAngleDegrees = 60.0f;
	static constexpr float ColossalDamageMultiplier = 1.667f;
	static constexpr float ColossalKnockbackSpeed = 400.0f;

	static constexpr int32 SubjugateEveryNthSwing = 4;
	static constexpr int32 SubjugateCrowdCount = 2;
	static constexpr float SubjugateCrowdRadius = 300.0f;
	static constexpr float SubjugateRadius = 500.0f;
	static constexpr float SubjugateWarningSeconds = 1.0f;
	static constexpr float SubjugateDamageMultiplier = 2.5f;
	static constexpr float SubjugateKnockbackSpeed = 500.0f;
	static constexpr float SubjugateStaggerSeconds = 0.8f;

	static EDSTRBossSkill Select(const FDSTRBossSkillInput& In);
	static FVector PullStep(
		const FVector& TargetLocation,
		const FVector& BossLocation,
		float PullSpeed,
		float DeltaSeconds,
		float MinDistance);
	static constexpr float PullSpeed() { return SiphonPullDistance / SiphonPullSeconds; }
	static const TCHAR* ToString(EDSTRBossSkill Skill);
};
