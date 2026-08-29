#pragma once

#include "CoreMinimal.h"

struct DEDISERVERRPG_API FDSTRDownedRules
{
	static constexpr float BleedOutSeconds = 45.0f;

	static float GetBleedOutRemaining(float DeadlineServerTime, float NowServerTime);
	static bool HasBledOut(float DeadlineServerTime, float NowServerTime);
	static bool IsPartyLost(int32 PlayerCount, int32 StandingCount);
};
