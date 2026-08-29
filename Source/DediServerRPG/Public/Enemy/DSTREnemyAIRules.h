#pragma once

#include "CoreMinimal.h"
#include "DSTREnemyAIRules.generated.h"

UENUM()
enum class EDSTREnemyAIState : uint8
{
	Idle,
	Approach,
	DirectSteer,
	Attack,
	Recover,
	Telegraph,
	Rush,
	Siphon,
	Stunned,
	Dead
};

struct DEDISERVERRPG_API FDSTREnemyAIRules
{
	static constexpr float DirectSteerRange = 400.0f;
	static constexpr float StuckSeconds = 1.0f;
	static constexpr float StuckSpeed = 5.0f;
	static constexpr float RetargetRatio = 0.7f;
	static constexpr float FacingToleranceDegrees = 25.0f;
	static constexpr float AttackExitMargin = 30.0f;
	static constexpr float DirectSteerHoldSeconds = 0.5f;
	static constexpr float DecisionInterval = 0.15f;

	static constexpr float ThreatDecayPerSecond = 0.15f;
	static constexpr float ThreatScoreScale = 50.0f;

	static constexpr float ApproachAcceptanceMargin = 10.0f;
	static constexpr float AgentRadiusReachPct = 1.1f;
	static constexpr float RepathGoalDistance = 150.0f;

	static bool IsFacingTarget(const FVector& Forward, const FVector& ToTarget);
	static bool ShouldSwitchTarget(
		float CurrentScore,
		float CandidateScore,
		float CurrentDistance = TNumericLimits<float>::Max(),
		float AttackRange = 0.0f);
	static float DecayThreat(float Threat, float DeltaSeconds);
	static float ThreatScore(float Distance, float Threat);
	static float ApproachAcceptanceRadius(float EngageRange);
	static float MoveStopDistance(
		float AcceptanceRadius,
		float AgentRadius,
		float GoalRadius,
		bool bIncludeAgentRadius,
		bool bIncludeGoalRadius);
	static bool ShouldRepath(const FVector& RequestedGoal, const FVector& CurrentGoal);
	static const TCHAR* ToString(EDSTREnemyAIState State);
};
