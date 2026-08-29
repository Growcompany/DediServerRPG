#pragma once

#include "CoreMinimal.h"

struct FDSTRBossRoomCandidate
{
	FVector Location = FVector::ZeroVector;
	float PathLength = 0.0f;
	int32 TileCount = 0;
};

struct FDSTRBossGatePlacement
{
	FVector Location = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	bool bValid = false;
};

struct DEDISERVERRPG_API FDSTRDungeonRules
{
	static constexpr float BossRoomRadius = 700.0f;
	static constexpr int32 BossRoomMinTiles = 4;
	static constexpr float BossRoomMinPathLength = 1500.0f;
	static constexpr float BossRoomMaxPathLength = 4000.0f;
	static constexpr float FloorTileGrid = 600.0f;
	static constexpr int32 MaxAdvancePathPoints = 16;
	static constexpr float BossAggroRange = 1000.0f;
	static constexpr float AmbushAggroRange = 800.0f;
	static constexpr float AmbushSideOffset = 200.0f;
	static constexpr int32 AmbushGroupCount = 2;
	static constexpr float AmbushGroupSpread = 120.0f;
	static constexpr float AdvanceWaypointRadius = 120.0f;
	static constexpr float AdvanceCombatRange = 600.0f;

	static constexpr float GateWidth = 600.0f;
	static constexpr float GateHeight = 500.0f;
	static constexpr float GateThickness = 40.0f;
	static constexpr float GateFitTraceDistance = 1500.0f;
	static constexpr float GateFitOverlap = 100.0f;
	static constexpr float GateFitMinHalfWidth = GateWidth * 0.5f;
	static constexpr float GateFitMaxHalfWidth = 3000.0f;
	static constexpr float GateEntryDepth = 200.0f;
	static constexpr float GateEntryTriggerThickness = 150.0f;
	static constexpr float GateEntryTriggerHeight = 400.0f;
	static constexpr float GateEntryInwardSpeed = 10.0f;
	static constexpr float ArenaPullGraceSeconds = 5.0f;
	static constexpr float BossWakeShakeRadius = 1500.0f;

	static constexpr float BossCameraRange = 1500.0f;
	static constexpr float DefaultCameraArmLength = 400.0f;
	static constexpr float BossCameraArmLength = 480.0f;
	static constexpr float BossCameraBlendSeconds = 2.0f;

	static int32 PickBossRoom(const TArray<FDSTRBossRoomCandidate>& Candidates);
	static TArray<FVector> SimplifyPath(const TArray<FVector>& PathPoints, int32 MaxPoints = MaxAdvancePathPoints);
	static FVector PointOnPath(const TArray<FVector>& Path, float Alpha);
	static TArray<FVector> PlaceAmbushes(const TArray<FVector>& Path, int32 GroupCount = AmbushGroupCount);
	static int32 GetAmbushCountPerGroup(int32 PlayerCount) { return FMath::Max(1, PlayerCount - 1); }
	static int32 NextWaypointIndex(const TArray<FVector>& Path, const FVector& From);
	static FVector NextWaypoint(const TArray<FVector>& Path, const FVector& From, const FVector& BossRoom);
	static bool ShouldWake(bool bDormant, float DistanceToPlayer, float AggroRange);

	static FDSTRBossGatePlacement PlaceGate(const TArray<FVector>& Path, const FVector& RoomCenter, float RoomRadius);
	static float FitGateWidth(float LeftHit, float RightHit, float MinWidth, float MaxWidth);
	static float GateSignedDistance(const FVector& Point, const FVector& GateLocation, const FVector& GateForward);
	static bool EnteredGateInward(const FVector& Velocity, const FVector& GateForward);
	static bool ShouldOpenGate(bool bSealed, int32 AmbushRemaining);
	static TArray<FVector> TrimPathAtGate(
		const TArray<FVector>& Path, const FVector& GateLocation, const FVector& GateForward);
	static FVector GetGateEntryPoint(const FVector& GateLocation, const FVector& GateForward);
	static bool ShouldPullToArena(bool bLocked, bool bInsideRoom, float SecondsOutside);

	static float StepCameraArmLength(float Current, bool bNearAwakeBoss, float DeltaSeconds);
};
