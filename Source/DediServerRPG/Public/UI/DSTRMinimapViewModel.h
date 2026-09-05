#pragma once

#include "CoreMinimal.h"

struct FDSTRMinimapFrame
{
	FVector2D WorldCenter = FVector2D::ZeroVector;
	float WorldHalfExtent = 1000.0f;
	float PanelSize = 200.0f;
};

// 층이 여러 개면 위에서 내려찍는 캡처 한 장으로는 아래층이 위층에 가려 안 보인다.
struct FDSTRMinimapLayerBand
{
	float MinZ = 0.0f;
	float MaxZ = 0.0f;
};

struct DEDISERVERRPG_API FDSTRMinimapViewModel
{
	static constexpr float PanelSize = 200.0f;
	static constexpr float MarkerInset = 6.0f;
	static constexpr float FrameMargin = 100.0f;
	static constexpr float AnchorFrameMargin = 400.0f;
	static constexpr float MinHalfExtent = 100.0f;
	static constexpr float PulseMinOpacity = 0.5f;
	static constexpr float LayerGapThreshold = 300.0f;
	static constexpr float LayerStandTolerance = 150.0f;
	static constexpr float OtherLayerOpacity = 0.25f;

	static FVector2D WorldToPanel(const FDSTRMinimapFrame& Frame, const FVector& World, bool& bClamped);
	static float HeadingToArrowAngle(float CameraYaw);
	static FDSTRMinimapFrame MakeFrame(const FBox& FloorBounds, float InPanelSize, float Margin = FrameMargin);
	static float PulseOpacity(bool bPulseOn);
	static TArray<FDSTRMinimapLayerBand> BuildLayerBands(
		TArray<float> FloorHeights, float GapThreshold = LayerGapThreshold);
	static int32 FindLayer(const TArray<FDSTRMinimapLayerBand>& Bands, float WorldZ);
};
