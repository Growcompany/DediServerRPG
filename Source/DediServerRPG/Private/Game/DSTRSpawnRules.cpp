#include "Game/DSTRSpawnRules.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

const FName FDSTRSpawnRules::SpawnDoorTag(TEXT("DSTR_SpawnDoor"));
const FName FDSTRSpawnRules::MinimapFloorTag(TEXT("DSTR_MinimapFloor"));

TArray<AActor*> FDSTRSpawnRules::CollectSpawnDoors(const UWorld* World)
{
	TArray<AActor*> Tagged;
	if (!World)
	{
		return Tagged;
	}
	for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		if (Actor->ActorHasTag(SpawnDoorTag))
		{
			Tagged.Add(Actor);
		}
	}
	return Tagged;
}

TArray<UStaticMeshComponent*> FDSTRSpawnRules::CollectMinimapFloors(const UWorld* World)
{
	TArray<UStaticMeshComponent*> Tagged;
	if (!World)
	{
		return Tagged;
	}
	for (TActorIterator<AStaticMeshActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
		if (!Mesh || !Mesh->GetStaticMesh())
		{
			continue;
		}
		if (It->ActorHasTag(MinimapFloorTag))
		{
			Tagged.Add(Mesh);
		}
	}
	return Tagged;
}

float FDSTRSpawnRules::DistanceToNearestPlayer(const FVector& Candidate, const TArray<FVector>& Players)
{
	float Nearest = TNumericLimits<float>::Max();
	for (const FVector& Player : Players)
	{
		Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist(Candidate, Player)));
	}
	return Nearest;
}

TArray<int32> FDSTRSpawnRules::FilterUsable(const TArray<FVector>& Candidates, const TArray<FVector>& Players)
{
	TArray<int32> Usable;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		if (DistanceToNearestPlayer(Candidates[Index], Players) >= MinDistanceToPlayers)
		{
			Usable.Add(Index);
		}
	}
	return Usable;
}

int32 FDSTRSpawnRules::PickFarthest(const TArray<FVector>& Candidates, const TArray<FVector>& Players)
{
	int32 Best = INDEX_NONE;
	float BestDistance = -1.0f;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const float Distance = DistanceToNearestPlayer(Candidates[Index], Players);
		if (Distance > BestDistance)
		{
			BestDistance = Distance;
			Best = Index;
		}
	}
	return Best;
}

FVector FDSTRSpawnRules::PartyCentre(const TArray<FVector>& Players)
{
	if (Players.IsEmpty())
	{
		return FVector::ZeroVector;
	}
	FVector Sum = FVector::ZeroVector;
	for (const FVector& Player : Players)
	{
		Sum += Player;
	}
	return Sum / static_cast<double>(Players.Num());
}

int32 FDSTRSpawnRules::PickNearestTo(
	const TArray<FVector>& Candidates,
	const TArray<int32>& Usable,
	const FVector& Point)
{
	int32 Best = INDEX_NONE;
	double BestDistanceSquared = TNumericLimits<double>::Max();
	for (const int32 Index : Usable)
	{
		if (!Candidates.IsValidIndex(Index))
		{
			continue;
		}
		const double DistanceSquared = FVector::DistSquared(Candidates[Index], Point);
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Best = Index;
		}
	}
	return Best;
}

FVector FDSTRSpawnRules::FallbackRingOffset(const int32 Index, const int32 Count)
{
	const int32 Divisor = FMath::Max(Count, 1);
	const float Angle = FMath::DegreesToRadians(360.0f * static_cast<float>(FMath::Max(Index, 0) % Divisor) / Divisor);
	return FVector(
		FallbackRingRadius * FMath::Cos(Angle),
		FallbackRingRadius * FMath::Sin(Angle),
		FallbackRingHeight);
}

float FDSTRSpawnRules::FacingYaw(const FVector& From, const FVector& To)
{
	const FVector Delta = To - From;
	if (Delta.SizeSquared2D() <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}
	return static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
}
