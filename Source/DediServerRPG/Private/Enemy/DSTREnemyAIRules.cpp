#include "Enemy/DSTREnemyAIRules.h"

#include "Combat/DSTRCombatMath.h"

bool FDSTREnemyAIRules::IsFacingTarget(const FVector& Forward, const FVector& ToTarget)
{
	return FDSTRCombatMath::IsInFrontCone(Forward, ToTarget, FacingToleranceDegrees);
}

bool FDSTREnemyAIRules::ShouldSwitchTarget(
	const float CurrentScore,
	const float CandidateScore,
	const float CurrentDistance,
	const float AttackRange)
{
	if (CurrentDistance <= AttackRange)
	{
		return false;
	}
	return CandidateScore < CurrentScore * RetargetRatio;
}

float FDSTREnemyAIRules::DecayThreat(const float Threat, const float DeltaSeconds)
{
	if (Threat <= 0.0f || DeltaSeconds <= 0.0f)
	{
		return FMath::Max(0.0f, Threat);
	}
	return Threat * FMath::Pow(1.0f - ThreatDecayPerSecond, DeltaSeconds);
}

float FDSTREnemyAIRules::ThreatScore(const float Distance, const float Threat)
{
	return FMath::Max(0.0f, Distance) / (1.0f + FMath::Max(0.0f, Threat) / ThreatScoreScale);
}

float FDSTREnemyAIRules::ApproachAcceptanceRadius(const float EngageRange)
{
	return FMath::Max(0.0f, EngageRange - ApproachAcceptanceMargin);
}

float FDSTREnemyAIRules::MoveStopDistance(
	const float AcceptanceRadius,
	const float AgentRadius,
	const float GoalRadius,
	const bool bIncludeAgentRadius,
	const bool bIncludeGoalRadius)
{
	return AcceptanceRadius
		+ (bIncludeGoalRadius ? GoalRadius : 0.0f)
		+ (bIncludeAgentRadius ? AgentRadius * AgentRadiusReachPct : 0.0f);
}

bool FDSTREnemyAIRules::ShouldRepath(const FVector& RequestedGoal, const FVector& CurrentGoal)
{
	return FVector::Dist2D(RequestedGoal, CurrentGoal) >= RepathGoalDistance;
}

const TCHAR* FDSTREnemyAIRules::ToString(const EDSTREnemyAIState State)
{
	switch (State)
	{
	case EDSTREnemyAIState::Idle: return TEXT("Idle");
	case EDSTREnemyAIState::Approach: return TEXT("Approach");
	case EDSTREnemyAIState::DirectSteer: return TEXT("DirectSteer");
	case EDSTREnemyAIState::Attack: return TEXT("Attack");
	case EDSTREnemyAIState::Recover: return TEXT("Recover");
	case EDSTREnemyAIState::Telegraph: return TEXT("Telegraph");
	case EDSTREnemyAIState::Rush: return TEXT("Rush");
	case EDSTREnemyAIState::Siphon: return TEXT("Siphon");
	case EDSTREnemyAIState::Stunned: return TEXT("Stunned");
	case EDSTREnemyAIState::Dead: return TEXT("Dead");
	}
	return TEXT("Unknown");
}
