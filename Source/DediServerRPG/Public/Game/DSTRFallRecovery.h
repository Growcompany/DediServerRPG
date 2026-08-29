#pragma once

#include "CoreMinimal.h"

struct DEDISERVERRPG_API FDSTRFallRecoveryRing
{
	static constexpr int32 Capacity = 8;
	static constexpr float SampleInterval = 0.5f;
	static constexpr float MinHorizontalClearance = 300.0f;

	static constexpr float RecoverySeconds = 2.0f;
	static constexpr float RecoveryDepth = 800.0f;
	static bool ShouldRecover(float FallSeconds, float Depth);

	void Reset();
	bool Sample(const FVector& Location, double TimeSeconds);
	bool FindRecoveryPoint(const FVector& FallLocation, FVector& OutPoint) const;
	int32 Num() const { return Points.Num(); }

private:
	TArray<FVector> Points;
	double LastSampleTime = -1.0;
};
