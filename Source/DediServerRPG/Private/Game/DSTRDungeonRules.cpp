#include "Game/DSTRDungeonRules.h"

namespace
{
	double PathArcLength(const TArray<FVector>& Path)
	{
		double Total = 0.0;
		for (int32 Index = 1; Index < Path.Num(); ++Index)
		{
			Total += FVector::Dist(Path[Index - 1], Path[Index]);
		}
		return Total;
	}
}

int32 FDSTRDungeonRules::PickBossRoom(const TArray<FDSTRBossRoomCandidate>& Candidates)
{
	int32 DeepestInBudget = INDEX_NONE;
	float DeepestInBudgetLength = -1.0f;
	int32 NearestOverBudget = INDEX_NONE;
	float NearestOverBudgetLength = TNumericLimits<float>::Max();

	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FDSTRBossRoomCandidate& Candidate = Candidates[Index];
		if (Candidate.TileCount < BossRoomMinTiles || Candidate.PathLength <= 0.0f)
		{
			continue;
		}
		if (Candidate.PathLength <= BossRoomMaxPathLength)
		{
			if (Candidate.PathLength > DeepestInBudgetLength)
			{
				DeepestInBudgetLength = Candidate.PathLength;
				DeepestInBudget = Index;
			}
		}
		else if (Candidate.PathLength < NearestOverBudgetLength)
		{
			NearestOverBudgetLength = Candidate.PathLength;
			NearestOverBudget = Index;
		}
	}
	return DeepestInBudget != INDEX_NONE ? DeepestInBudget : NearestOverBudget;
}

TArray<FVector> FDSTRDungeonRules::SimplifyPath(const TArray<FVector>& PathPoints, const int32 MaxPoints)
{
	const int32 Budget = FMath::Max(1, MaxPoints);
	if (PathPoints.Num() <= Budget)
	{
		return PathPoints;
	}
	const int32 Last = PathPoints.Num() - 1;
	if (Budget == 1)
	{
		return TArray<FVector>({PathPoints[Last]});
	}

	TArray<FVector> Thinned;
	Thinned.Reserve(Budget);
	for (int32 Index = 0; Index < Budget; ++Index)
	{
		const int32 Source = FMath::RoundToInt(static_cast<float>(Index) * Last / static_cast<float>(Budget - 1));
		Thinned.Add(PathPoints[FMath::Clamp(Source, 0, Last)]);
	}
	return Thinned;
}

FVector FDSTRDungeonRules::PointOnPath(const TArray<FVector>& Path, const float Alpha)
{
	if (Path.IsEmpty())
	{
		return FVector::ZeroVector;
	}
	const double Total = PathArcLength(Path);
	if (Total <= UE_KINDA_SMALL_NUMBER)
	{
		return Path[0];
	}

	const double Target = FMath::Clamp(static_cast<double>(Alpha), 0.0, 1.0) * Total;
	double Walked = 0.0;
	for (int32 Index = 1; Index < Path.Num(); ++Index)
	{
		const double Segment = FVector::Dist(Path[Index - 1], Path[Index]);
		if (Walked + Segment >= Target)
		{
			const double Fraction = Segment > UE_KINDA_SMALL_NUMBER ? (Target - Walked) / Segment : 0.0;
			return FMath::Lerp(Path[Index - 1], Path[Index], Fraction);
		}
		Walked += Segment;
	}
	return Path.Last();
}

TArray<FVector> FDSTRDungeonRules::PlaceAmbushes(const TArray<FVector>& Path, const int32 GroupCount)
{
	TArray<FVector> Anchors;
	if (Path.IsEmpty() || GroupCount <= 0)
	{
		return Anchors;
	}

	Anchors.Reserve(GroupCount);
	for (int32 Group = 0; Group < GroupCount; ++Group)
	{
		const float Alpha = static_cast<float>(Group + 1) / static_cast<float>(GroupCount + 1);
		const FVector OnPath = PointOnPath(Path, Alpha);
		const FVector Ahead = PointOnPath(Path, FMath::Min(1.0f, Alpha + 0.05f));
		const FVector Side = FVector::CrossProduct(FVector::UpVector, (Ahead - OnPath).GetSafeNormal2D());
		const float Sign = Group % 2 == 0 ? 1.0f : -1.0f;
		Anchors.Add(OnPath + Side * AmbushSideOffset * Sign);
	}
	return Anchors;
}

int32 FDSTRDungeonRules::NextWaypointIndex(const TArray<FVector>& Path, const FVector& From)
{
	if (Path.IsEmpty())
	{
		return INDEX_NONE;
	}

	int32 Nearest = 0;
	double NearestSquared = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < Path.Num(); ++Index)
	{
		const double DistanceSquared = FVector::DistSquared2D(Path[Index], From);
		if (DistanceSquared < NearestSquared)
		{
			NearestSquared = DistanceSquared;
			Nearest = Index;
		}
	}
	const int32 Next = Nearest + 1;
	return Path.IsValidIndex(Next) ? Next : INDEX_NONE;
}

FVector FDSTRDungeonRules::NextWaypoint(const TArray<FVector>& Path, const FVector& From, const FVector& BossRoom)
{
	const int32 Next = NextWaypointIndex(Path, From);
	return Path.IsValidIndex(Next) ? Path[Next] : BossRoom;
}

bool FDSTRDungeonRules::ShouldWake(const bool bDormant, const float DistanceToPlayer, const float AggroRange)
{
	return bDormant && DistanceToPlayer <= AggroRange;
}

FDSTRBossGatePlacement FDSTRDungeonRules::PlaceGate(
	const TArray<FVector>& Path,
	const FVector& RoomCenter,
	const float RoomRadius)
{
	FDSTRBossGatePlacement Placement;
	if (Path.IsEmpty())
	{
		return Placement;
	}

	for (int32 Index = Path.Num() - 1; Index >= 1; --Index)
	{
		const double Inner = FVector::Dist2D(Path[Index], RoomCenter);
		const double Outer = FVector::Dist2D(Path[Index - 1], RoomCenter);
		const FVector Along = (Path[Index] - Path[Index - 1]).GetSafeNormal2D();
		if (Inner > RoomRadius || Outer <= RoomRadius || Along.IsNearlyZero())
		{
			continue;
		}
		Placement.Location = FMath::Lerp(
			Path[Index - 1], Path[Index], FMath::Clamp((Outer - RoomRadius) / (Outer - Inner), 0.0, 1.0));
		Placement.Forward = Along;
		Placement.bValid = true;
		return Placement;
	}

	int32 Approach = INDEX_NONE;
	for (int32 Index = Path.Num() - 1; Index >= 0; --Index)
	{
		if (FVector::Dist2D(Path[Index], RoomCenter) > RoomRadius)
		{
			Approach = Index;
			break;
		}
	}

	const FVector From = Path.IsValidIndex(Approach) ? Path[Approach] : Path[0];
	const FVector ToRoom = RoomCenter - From;
	const FVector Heading = ToRoom.GetSafeNormal2D();
	if (Heading.IsNearlyZero())
	{
		return Placement;
	}

	const double Distance = ToRoom.Size2D();
	const double Alpha = Distance > UE_KINDA_SMALL_NUMBER
		? FMath::Clamp((Distance - RoomRadius) / Distance, 0.0, 1.0)
		: 0.0;
	Placement.Location = FMath::Lerp(From, RoomCenter, Alpha);
	Placement.Forward = Heading;
	Placement.bValid = true;
	return Placement;
}

float FDSTRDungeonRules::FitGateWidth(
	const float LeftHit,
	const float RightHit,
	const float MinWidth,
	const float MaxWidth)
{
	const float Span = FMath::Max(0.0f, LeftHit) + FMath::Max(0.0f, RightHit);
	return FMath::Clamp(Span + GateFitOverlap, MinWidth, FMath::Max(MinWidth, MaxWidth));
}

float FDSTRDungeonRules::GateSignedDistance(
	const FVector& Point,
	const FVector& GateLocation,
	const FVector& GateForward)
{
	const FVector Offset = Point - GateLocation;
	return static_cast<float>(FVector::DotProduct(
		FVector(Offset.X, Offset.Y, 0.0), GateForward.GetSafeNormal2D()));
}

bool FDSTRDungeonRules::EnteredGateInward(const FVector& Velocity, const FVector& GateForward)
{
	return FVector::DotProduct(
		FVector(Velocity.X, Velocity.Y, 0.0), GateForward.GetSafeNormal2D()) > GateEntryInwardSpeed;
}

bool FDSTRDungeonRules::ShouldOpenGate(const bool bSealed, const int32 AmbushRemaining)
{
	return bSealed && AmbushRemaining <= 0;
}

TArray<FVector> FDSTRDungeonRules::TrimPathAtGate(
	const TArray<FVector>& Path,
	const FVector& GateLocation,
	const FVector& GateForward)
{
	TArray<FVector> Corridor;
	for (const FVector& Point : Path)
	{
		if (GateSignedDistance(Point, GateLocation, GateForward) > 0.0f)
		{
			break;
		}
		Corridor.Add(Point);
	}
	Corridor.Add(GateLocation);
	return Corridor;
}

FVector FDSTRDungeonRules::GetGateEntryPoint(const FVector& GateLocation, const FVector& GateForward)
{
	return GateLocation + GateForward.GetSafeNormal2D() * GateEntryDepth;
}

bool FDSTRDungeonRules::ShouldPullToArena(
	const bool bLocked,
	const bool bInsideRoom,
	const float SecondsOutside)
{
	return bLocked && !bInsideRoom && SecondsOutside >= ArenaPullGraceSeconds;
}

float FDSTRDungeonRules::StepCameraArmLength(
	const float Current,
	const bool bNearAwakeBoss,
	const float DeltaSeconds)
{
	const float Target = bNearAwakeBoss ? BossCameraArmLength : DefaultCameraArmLength;
	const float Rate = (BossCameraArmLength - DefaultCameraArmLength) / BossCameraBlendSeconds;
	return FMath::FInterpConstantTo(Current, Target, FMath::Max(0.0f, DeltaSeconds), Rate);
}
