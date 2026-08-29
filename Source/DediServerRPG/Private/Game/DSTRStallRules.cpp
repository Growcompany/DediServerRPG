#include "Game/DSTRStallRules.h"

bool FDSTRStallRules::IsMatchRunning(const EDSTRMatchPhase Phase)
{
	return Phase == EDSTRMatchPhase::Wave
		|| Phase == EDSTRMatchPhase::Advance
		|| Phase == EDSTRMatchPhase::Boss;
}

bool FDSTRStallRules::HasProgressed(const FDSTRMatchProgress& Previous, const FDSTRMatchProgress& Current)
{
	return Previous.Phase != Current.Phase
		|| Previous.GateState != Current.GateState
		|| Previous.RemainingEnemies != Current.RemainingEnemies
		|| Previous.AmbushRemaining != Current.AmbushRemaining
		|| Previous.PendingSpawnCount != Current.PendingSpawnCount
		|| Previous.DownCount != Current.DownCount
		|| Previous.ReviveCount != Current.ReviveCount
		|| Previous.BossHealth != Current.BossHealth
		|| Previous.PlayersStanding != Current.PlayersStanding;
}

bool FDSTRStallRules::ShouldFailForStall(const bool bMatchRunning, const float SecondsSinceProgress)
{
	return bMatchRunning && SecondsSinceProgress >= StallTimeoutSeconds;
}
