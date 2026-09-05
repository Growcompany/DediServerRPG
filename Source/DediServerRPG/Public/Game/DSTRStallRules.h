#pragma once

#include "CoreMinimal.h"
#include "Game/DSTRGameState.h"

struct FDSTRMatchProgress
{
	uint8 Phase = 0;
	uint8 GateState = 0;
	int32 RemainingEnemies = 0;
	int32 AmbushRemaining = 0;
	int32 PendingSpawnCount = 0;
	int32 DownCount = 0;
	int32 ReviveCount = 0;
	int32 BossHealth = -1;
	int32 PlayersStanding = 0;
	// 넓은 던전에서는 보스방으로 걸어가는 것 자체가 진척이다.
	int32 BossApproachStep = 0;
};

struct DEDISERVERRPG_API FDSTRStallRules
{
	static constexpr float StallTimeoutSeconds = 60.0f;
	static constexpr float BossApproachStepSize = 200.0f;

	static bool IsMatchRunning(EDSTRMatchPhase Phase);
	static bool HasProgressed(const FDSTRMatchProgress& Previous, const FDSTRMatchProgress& Current);
	static bool ShouldFailForStall(bool bMatchRunning, float SecondsSinceProgress);
};
