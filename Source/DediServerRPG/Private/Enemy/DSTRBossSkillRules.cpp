#include "Enemy/DSTRBossSkillRules.h"

EDSTRBossSkill FDSTRBossSkillRules::Select(const FDSTRBossSkillInput& In)
{
	if (!In.bHasTarget)
	{
		return EDSTRBossSkill::None;
	}

	const bool bCrowded = In.PlayersInCrowdRadius >= SubjugateCrowdCount;
	const bool bOverdue = In.SwingsSinceSubjugate >= SubjugateEveryNthSwing;
	if (In.bSubjugateReady && In.Distance2D <= SubjugateRadius && (bCrowded || bOverdue))
	{
		return EDSTRBossSkill::Subjugate;
	}

	if (In.Distance2D <= In.AttackRange)
	{
		if (!In.bSwingReady)
		{
			return EDSTRBossSkill::None;
		}
		return In.bColossalReady && In.SwingsSinceColossal >= ColossalEveryNthSwing - 1
			? EDSTRBossSkill::ColossalBlow
			: EDSTRBossSkill::Swing;
	}

	if (In.bRushReady && In.Distance2D >= RushMinDistance && In.Distance2D <= RushMaxDistance)
	{
		return EDSTRBossSkill::PhantomRush;
	}
	if (In.bSiphonReady && In.Distance2D >= SiphonMinDistance && In.Distance2D <= SiphonMaxDistance)
	{
		return EDSTRBossSkill::Siphon;
	}
	return EDSTRBossSkill::None;
}

FVector FDSTRBossSkillRules::PullStep(
	const FVector& TargetLocation,
	const FVector& BossLocation,
	const float PullSpeedPerSecond,
	const float DeltaSeconds,
	const float MinDistance)
{
	const FVector ToBoss = FVector(BossLocation.X - TargetLocation.X, BossLocation.Y - TargetLocation.Y, 0.0f);
	const float Distance = static_cast<float>(ToBoss.Size());
	const float Floor = FMath::Max(0.0f, MinDistance);
	if (Distance <= Floor || Distance <= KINDA_SMALL_NUMBER)
	{
		return TargetLocation;
	}
	const float Step = FMath::Min(FMath::Max(0.0f, PullSpeedPerSecond) * FMath::Max(0.0f, DeltaSeconds), Distance - Floor);
	return TargetLocation + ToBoss / Distance * Step;
}

const TCHAR* FDSTRBossSkillRules::ToString(const EDSTRBossSkill Skill)
{
	switch (Skill)
	{
	case EDSTRBossSkill::None: return TEXT("None");
	case EDSTRBossSkill::Swing: return TEXT("Swing");
	case EDSTRBossSkill::ColossalBlow: return TEXT("ColossalBlow");
	case EDSTRBossSkill::PhantomRush: return TEXT("PhantomRush");
	case EDSTRBossSkill::Siphon: return TEXT("Siphon");
	case EDSTRBossSkill::Subjugate: return TEXT("Subjugate");
	}
	return TEXT("Unknown");
}
