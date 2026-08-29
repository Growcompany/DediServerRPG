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
