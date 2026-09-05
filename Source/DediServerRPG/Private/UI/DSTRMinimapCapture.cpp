#include "UI/DSTRMinimapCapture.h"
#include "DSTRLog.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Game/DSTRSpawnRules.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/KismetRenderingLibrary.h"

ADSTRMinimapCapture::ADSTRMinimapCapture()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	RootComponent = Capture;
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
}

void ADSTRMinimapCapture::CollectFloorComponents(
	TArray<UPrimitiveComponent*>& OutFloors,
	FBox& OutBounds) const
{
	OutFloors.Reset();
	OutBounds = FBox(ForceInit);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	auto Accept = [&OutFloors, &OutBounds](UStaticMeshComponent* Mesh)
	{
		OutFloors.Add(Mesh);
		OutBounds += Mesh->Bounds.GetBox();
	};

	for (UStaticMeshComponent* Mesh : FDSTRSpawnRules::CollectMinimapFloors(World))
	{
		Accept(Mesh);
	}
}

FBox ADSTRMinimapCapture::CollectPlayAnchorBounds(int32& OutStarts, int32& OutDoors) const
{
	FBox Bounds(ForceInit);
	OutStarts = 0;
	OutDoors = 0;
	UWorld* World = const_cast<UWorld*>(GetWorld());
	if (!World)
	{
		return Bounds;
	}
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		++OutStarts;
		Bounds += It->GetActorLocation();
	}
	for (const AActor* Door : FDSTRSpawnRules::CollectSpawnDoors(World))
	{
		++OutDoors;
		Bounds += Door->GetActorLocation();
	}
	return Bounds;
}

UTextureRenderTarget2D* ADSTRMinimapCapture::GetLayerTarget(const int32 LayerIndex) const
{
	return MinimapLayers.IsValidIndex(LayerIndex) ? MinimapLayers[LayerIndex].Target : nullptr;
}

int32 ADSTRMinimapCapture::FindLayerForZ(const float WorldZ) const
{
	return FDSTRMinimapViewModel::FindLayer(LayerBands, WorldZ);
}

bool ADSTRMinimapCapture::IsWithinLayer(const int32 LayerIndex, const float WorldZ) const
{
	return FindLayerForZ(WorldZ) == LayerIndex;
}

const FDSTRMinimapFrame& ADSTRMinimapCapture::GetLayerFrame(const int32 LayerIndex) const
{
	return MinimapLayers.IsValidIndex(LayerIndex) ? MinimapLayers[LayerIndex].Frame : Frame;
}

bool ADSTRMinimapCapture::CaptureLayer(
	const TArray<UPrimitiveComponent*>& Floors,
	const FDSTRMinimapLayerBand& Band,
	const FBox& AnchorBounds,
	const bool bAnchorsInBand)
{
	UWorld* World = GetWorld();

	TArray<UPrimitiveComponent*> BandFloors;
	FBox BandBounds(ForceInit);
	for (UPrimitiveComponent* Floor : Floors)
	{
		const float FloorZ = static_cast<float>(Floor->Bounds.GetBox().Max.Z);
		if (FloorZ < Band.MinZ - 1.0f || FloorZ > Band.MaxZ + 1.0f)
		{
			continue;
		}
		BandFloors.Add(Floor);
		BandBounds += Floor->Bounds.GetBox();
	}
	if (BandFloors.IsEmpty() || BandBounds.IsValid == 0)
	{
		return false;
	}

	FDSTRMinimapLayer Layer;
	Layer.MinZ = Band.MinZ;
	Layer.MaxZ = Band.MaxZ;
	// 지상층은 기존처럼 플레이 앵커로 타이트하게, 그 외 층은 자기 바닥 경계로 프레임을 잡는다.
	Layer.Frame = bAnchorsInBand
		? FDSTRMinimapViewModel::MakeFrame(
			AnchorBounds, FDSTRMinimapViewModel::PanelSize, FDSTRMinimapViewModel::AnchorFrameMargin)
		: FDSTRMinimapViewModel::MakeFrame(BandBounds, FDSTRMinimapViewModel::PanelSize);
	Layer.Target = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, RenderTargetSize, RenderTargetSize, RTF_RGBA8, FLinearColor(0.01f, 0.02f, 0.04f, 1.0f));
	if (!Layer.Target)
	{
		return false;
	}

	// 이 층의 바닥만 그리므로 위층이 아래층을 가리지 않는다.
	Capture->ClearShowOnlyComponents();
	for (UPrimitiveComponent* Floor : BandFloors)
	{
		Capture->ShowOnlyComponent(Floor);
	}

	Capture->TextureTarget = Layer.Target;
	Capture->OrthoWidth = Layer.Frame.WorldHalfExtent * 2.0f;
	SetActorLocationAndRotation(
		FVector(Layer.Frame.WorldCenter.X, Layer.Frame.WorldCenter.Y, Band.MaxZ + CaptureHeight),
		FRotator(-90.0f, 0.0f, 0.0f));
	Capture->CaptureScene();
	MinimapLayers.Add(Layer);

	UE_LOG(LogDSTR, Log, TEXT("DSTR_MINIMAP_LAYER Index=%d Floors=%d MinZ=%.0f MaxZ=%.0f Centre=%s Half=%.0f Anchored=%d"),
		MinimapLayers.Num() - 1, BandFloors.Num(), Band.MinZ, Band.MaxZ,
		*Layer.Frame.WorldCenter.ToString(), Layer.Frame.WorldHalfExtent, bAnchorsInBand ? 1 : 0);
	return true;
}

bool ADSTRMinimapCapture::CaptureFloorLayout()
{
	UWorld* World = GetWorld();
	if (!World || !Capture || IsRunningDedicatedServer())
	{
		return false;
	}

	TArray<UPrimitiveComponent*> Floors;
	FBox Bounds(ForceInit);
	CollectFloorComponents(Floors, Bounds);
	if (Floors.IsEmpty() || Bounds.IsValid == 0)
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_MINIMAP_FLOORS_MISSING RequiredTag=%s"),
			*FDSTRSpawnRules::MinimapFloorTag.ToString());
		return false;
	}

	int32 StartCount = 0;
	int32 DoorCount = 0;
	const FBox Anchors = CollectPlayAnchorBounds(StartCount, DoorCount);
	const bool bAnchored = Anchors.IsValid != 0;

	TArray<float> Heights;
	Heights.Reserve(Floors.Num());
	for (const UPrimitiveComponent* Floor : Floors)
	{
		Heights.Add(static_cast<float>(Floor->Bounds.GetBox().Max.Z));
	}
	LayerBands = FDSTRMinimapViewModel::BuildLayerBands(Heights);

	// 앵커(문·시작점)가 속한 층만 앵커 프레임을 쓴다.
	const int32 AnchorLayer = bAnchored
		? FDSTRMinimapViewModel::FindLayer(LayerBands, static_cast<float>(Anchors.GetCenter().Z))
		: INDEX_NONE;

	Capture->ShowFlags.SetLighting(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetPostProcessing(false);
	Capture->ShowFlags.SetAtmosphere(false);
	MinimapLayers.Reset();
	for (int32 BandIndex = 0; BandIndex < LayerBands.Num(); ++BandIndex)
	{
		CaptureLayer(Floors, LayerBands[BandIndex], Anchors, bAnchored && BandIndex == AnchorLayer);
	}
	if (MinimapLayers.IsEmpty())
	{
		return false;
	}
	Frame = MinimapLayers[0].Frame;

	UE_LOG(LogDSTR, Log, TEXT("DSTR_MINIMAP_FLOORS Rule=Tag Count=%d Layers=%d"), Floors.Num(), MinimapLayers.Num());
	UE_LOG(LogDSTR, Log, TEXT("DSTR_MINIMAP_FRAME Rule=%s Starts=%d Doors=%d"),
		bAnchored ? TEXT("PlayAnchors") : TEXT("FloorBounds"), StartCount, DoorCount);
	return true;
}
