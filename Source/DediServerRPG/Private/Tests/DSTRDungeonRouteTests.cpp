#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/DSTRDungeonRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRDungeonRouteCornersTest,
	"DSTR.Dungeon.RouteKeepsCorners", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDSTRDungeonRouteCornersTest::RunTest(const FString&)
{
	const TArray<FVector> Path = {FVector(0, 0, 0), FVector(1000, 0, 0), FVector(1000, 1000, 0)};
	TestEqual(TEXT("Do not cut the corner from halfway down the corridor"),
		FDSTRDungeonRules::NextWaypointIndex(Path, FVector(600, 0, 0)), 1);
	TestEqual(TEXT("Combat displacement still returns to the next corner"),
		FDSTRDungeonRules::NextWaypointIndex(Path, FVector(650, -80, 0)), 1);
	TestEqual(TEXT("At the corner, proceed along the next segment"),
		FDSTRDungeonRules::NextWaypointIndex(Path, FVector(1000, 0, 0)), 2);
	TestEqual(TEXT("Keep approaching the final waypoint until actually reached"),
		FDSTRDungeonRules::NextWaypointIndex(Path, FVector(1000, 600, 0)), 2);
	TestEqual(TEXT("Reached final waypoint uses the destination fallback"),
		FDSTRDungeonRules::NextWaypointIndex(Path, FVector(1000, 1000, 0)), INDEX_NONE);
	TestEqual(TEXT("An empty route uses the destination fallback"),
		FDSTRDungeonRules::NextWaypointIndex({}, FVector::ZeroVector), INDEX_NONE);
	return true;
}

#endif
