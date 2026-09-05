#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/DSTRMinimapViewModel.h"
#include "DSTRMinimapCapture.generated.h"

class UPrimitiveComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

USTRUCT()
struct FDSTRMinimapLayer
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> Target = nullptr;

	float MinZ = 0.0f;
	float MaxZ = 0.0f;
	// 층마다 자기 구역을 프레임으로 잡아야 지하가 지도 밖으로 잘리지 않는다.
	FDSTRMinimapFrame Frame;
};

UCLASS()
class DEDISERVERRPG_API ADSTRMinimapCapture : public AActor
{
	GENERATED_BODY()

public:
	ADSTRMinimapCapture();

	static constexpr int32 RenderTargetSize = 256;
	static constexpr float CaptureHeight = 2000.0f;

	bool CaptureFloorLayout();
	const FDSTRMinimapFrame& GetFrame() const { return Frame; }
	const FDSTRMinimapFrame& GetLayerFrame(int32 LayerIndex) const;

	int32 GetLayerCount() const { return MinimapLayers.Num(); }
	UTextureRenderTarget2D* GetLayerTarget(int32 LayerIndex) const;
	int32 FindLayerForZ(float WorldZ) const;
	bool IsWithinLayer(int32 LayerIndex, float WorldZ) const;

private:
	void CollectFloorComponents(TArray<UPrimitiveComponent*>& OutFloors, FBox& OutBounds) const;
	FBox CollectPlayAnchorBounds(int32& OutStarts, int32& OutDoors) const;
	bool CaptureLayer(
		const TArray<UPrimitiveComponent*>& Floors, const FDSTRMinimapLayerBand& Band,
		const FBox& AnchorBounds, bool bAnchorsInBand);

	UPROPERTY(VisibleAnywhere, Category = "Minimap")
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY(Transient)
	TArray<FDSTRMinimapLayer> MinimapLayers;

	TArray<FDSTRMinimapLayerBand> LayerBands;
	FDSTRMinimapFrame Frame;
};
