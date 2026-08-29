#pragma once

#include "CoreMinimal.h"

struct FDSTRMinimapFrame
{
	FVector2D WorldCenter = FVector2D::ZeroVector;
	float WorldHalfExtent = 1000.0f;
	float PanelSize = 200.0f;
};

struct DEDISERVERRPG_API FDSTRMinimapViewModel
{
	static constexpr float PanelSize = 200.0f;
	static constexpr float MarkerInset = 6.0f;
	static constexpr float FrameMargin = 100.0f;
	static constexpr float AnchorFrameMargin = 400.0f;
	static constexpr float MinHalfExtent = 100.0f;
	static constexpr float PulseMinOpacity = 0.5f;

	static FVector2D WorldToPanel(const FDSTRMinimapFrame& Frame, const FVector& World, bool& bClamped);
	static float HeadingToArrowAngle(float CameraYaw);
	static FDSTRMinimapFrame MakeFrame(const FBox& FloorBounds, float InPanelSize, float Margin = FrameMargin);
	static float PulseOpacity(bool bPulseOn);
};
