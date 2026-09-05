#include "UI/DSTRMinimapViewModel.h"

FVector2D FDSTRMinimapViewModel::WorldToPanel(
	const FDSTRMinimapFrame& Frame,
	const FVector& World,
	bool& bClamped)
{
	const float HalfExtent = FMath::Max(Frame.WorldHalfExtent, MinHalfExtent);
	const float HalfPanel = Frame.PanelSize * 0.5f;
	const float North = static_cast<float>(World.X - Frame.WorldCenter.X);
	const float East = static_cast<float>(World.Y - Frame.WorldCenter.Y);

	bClamped = FMath::Abs(North) > HalfExtent || FMath::Abs(East) > HalfExtent;
	return FVector2D(
		FMath::Clamp(HalfPanel + East / HalfExtent * HalfPanel, MarkerInset, Frame.PanelSize - MarkerInset),
		FMath::Clamp(HalfPanel - North / HalfExtent * HalfPanel, MarkerInset, Frame.PanelSize - MarkerInset));
}

float FDSTRMinimapViewModel::HeadingToArrowAngle(const float CameraYaw)
{
	return FMath::Fmod(FMath::Fmod(CameraYaw, 360.0f) + 360.0f, 360.0f);
}

FDSTRMinimapFrame FDSTRMinimapViewModel::MakeFrame(const FBox& FloorBounds, const float InPanelSize, const float Margin)
{
	const bool bUsable = FloorBounds.IsValid != 0;
	const FVector Center = bUsable ? FloorBounds.GetCenter() : FVector::ZeroVector;
	const FVector Extent = bUsable ? FloorBounds.GetExtent() : FVector::ZeroVector;

	FDSTRMinimapFrame Frame;
	Frame.PanelSize = InPanelSize;
	Frame.WorldCenter = FVector2D(Center.X, Center.Y);
	Frame.WorldHalfExtent = FMath::Max(
		static_cast<float>(FMath::Max(Extent.X, Extent.Y)) + Margin, MinHalfExtent);
	return Frame;
}

float FDSTRMinimapViewModel::PulseOpacity(const bool bPulseOn)
{
	return bPulseOn ? 1.0f : PulseMinOpacity;
}

TArray<FDSTRMinimapLayerBand> FDSTRMinimapViewModel::BuildLayerBands(
	TArray<float> FloorHeights,
	const float GapThreshold)
{
	TArray<FDSTRMinimapLayerBand> Bands;
	if (FloorHeights.IsEmpty())
	{
		return Bands;
	}
	FloorHeights.Sort();

	FDSTRMinimapLayerBand Current{FloorHeights[0], FloorHeights[0]};
	for (int32 Index = 1; Index < FloorHeights.Num(); ++Index)
	{
		// 계단처럼 촘촘히 이어진 높이는 한 층으로 묶고, 빈 높이가 크면 층을 끊는다.
		if (FloorHeights[Index] - Current.MaxZ > GapThreshold)
		{
			Bands.Add(Current);
			Current = FDSTRMinimapLayerBand{FloorHeights[Index], FloorHeights[Index]};
			continue;
		}
		Current.MaxZ = FloorHeights[Index];
	}
	Bands.Add(Current);
	return Bands;
}

int32 FDSTRMinimapViewModel::FindLayer(const TArray<FDSTRMinimapLayerBand>& Bands, const float WorldZ)
{
	int32 Best = INDEX_NONE;
	float BestScore = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < Bands.Num(); ++Index)
	{
		// 캐릭터는 바닥 위에 서므로 발밑에서 가장 가까운 층이 자기 층이다.
		const float Above = WorldZ - Bands[Index].MaxZ;
		const float Score = Above >= -LayerStandTolerance
			? Above
			: FMath::Abs(Above) + LayerStandTolerance;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Index;
		}
	}
	return Best;
}
