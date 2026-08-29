#pragma once

#include "CoreMinimal.h"

class AActor;
class UStaticMeshComponent;
class UWorld;

struct DEDISERVERRPG_API FDSTRSpawnRules
{
	static constexpr float MinDistanceToPlayers = 600.0f;
	static constexpr float SpawnInterval = 0.8f;
	static constexpr float FallbackRingRadius = 700.0f;
	static constexpr float FallbackRingHeight = 100.0f;

	static const FName SpawnDoorTag;
	static const FName MinimapFloorTag;

	static TArray<AActor*> CollectSpawnDoors(const UWorld* World);
	static TArray<UStaticMeshComponent*> CollectMinimapFloors(const UWorld* World);
	static float DistanceToNearestPlayer(const FVector& Candidate, const TArray<FVector>& Players);
	static TArray<int32> FilterUsable(const TArray<FVector>& Candidates, const TArray<FVector>& Players);
	static int32 PickFarthest(const TArray<FVector>& Candidates, const TArray<FVector>& Players);
	static FVector PartyCentre(const TArray<FVector>& Players);
	static int32 PickNearestTo(const TArray<FVector>& Candidates, const TArray<int32>& Usable, const FVector& Point);
	static FVector FallbackRingOffset(int32 Index, int32 Count);
	static float FacingYaw(const FVector& From, const FVector& To);
};
