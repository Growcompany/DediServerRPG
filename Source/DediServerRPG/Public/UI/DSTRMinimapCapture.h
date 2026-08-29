#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/DSTRMinimapViewModel.h"
#include "DSTRMinimapCapture.generated.h"

class UPrimitiveComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

UCLASS()
class DEDISERVERRPG_API ADSTRMinimapCapture : public AActor
{
	GENERATED_BODY()

public:
	ADSTRMinimapCapture();

	static constexpr int32 RenderTargetSize = 256;
	static constexpr float CaptureHeight = 2000.0f;

	bool CaptureFloorLayout();
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }
	const FDSTRMinimapFrame& GetFrame() const { return Frame; }

private:
	void CollectFloorComponents(TArray<UPrimitiveComponent*>& OutFloors, FBox& OutBounds) const;
	FBox CollectPlayAnchorBounds(int32& OutStarts, int32& OutDoors) const;

	UPROPERTY(VisibleAnywhere, Category = "Minimap")
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	FDSTRMinimapFrame Frame;
};
