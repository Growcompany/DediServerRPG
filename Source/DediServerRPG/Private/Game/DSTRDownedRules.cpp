#include "Game/DSTRDownedRules.h"

float FDSTRDownedRules::GetBleedOutRemaining(const float DeadlineServerTime, const float NowServerTime)
{
	return DeadlineServerTime > 0.0f
		? FMath::Max(0.0f, DeadlineServerTime - NowServerTime)
		: 0.0f;
}

bool FDSTRDownedRules::HasBledOut(const float DeadlineServerTime, const float NowServerTime)
{
	return DeadlineServerTime > 0.0f && NowServerTime >= DeadlineServerTime;
}

bool FDSTRDownedRules::IsPartyLost(const int32 PlayerCount, const int32 StandingCount)
{
	return PlayerCount > 0 && StandingCount <= 0;
}
