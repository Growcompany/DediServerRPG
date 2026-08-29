#include "Game/DSTRFallRecovery.h"

bool FDSTRFallRecoveryRing::ShouldRecover(const float FallSeconds, const float Depth)
{
	if (Depth >= RecoveryDepth)
	{
		return true;
	}
	return FallSeconds >= RecoverySeconds && Depth > 0.0f;
}

void FDSTRFallRecoveryRing::Reset()
{
	Points.Reset();
	LastSampleTime = -1.0;
}

bool FDSTRFallRecoveryRing::Sample(const FVector& Location, const double TimeSeconds)
{
	if (LastSampleTime >= 0.0 && TimeSeconds - LastSampleTime < SampleInterval)
	{
		return false;
	}
	LastSampleTime = TimeSeconds;
	Points.Add(Location);
	if (Points.Num() > Capacity)
	{
		Points.RemoveAt(0, Points.Num() - Capacity, false);
	}
	return true;
}

bool FDSTRFallRecoveryRing::FindRecoveryPoint(const FVector& FallLocation, FVector& OutPoint) const
{
	for (const FVector& Point : Points)
	{
		if (FVector::DistSquared2D(Point, FallLocation) >= MinHorizontalClearance * MinHorizontalClearance)
		{
			OutPoint = Point;
			return true;
		}
	}
	return false;
}
