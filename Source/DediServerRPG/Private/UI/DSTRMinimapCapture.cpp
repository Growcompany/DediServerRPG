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

	if (!RenderTarget)
	{
		RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
			World, RenderTargetSize, RenderTargetSize, RTF_RGBA8, FLinearColor(0.01f, 0.02f, 0.04f, 1.0f));
	}
	if (!RenderTarget)
	{
		return false;
	}

	int32 StartCount = 0;
	int32 DoorCount = 0;
	const FBox Anchors = CollectPlayAnchorBounds(StartCount, DoorCount);
	const bool bAnchored = Anchors.IsValid != 0;
	Frame = bAnchored
		? FDSTRMinimapViewModel::MakeFrame(
			Anchors, FDSTRMinimapViewModel::PanelSize, FDSTRMinimapViewModel::AnchorFrameMargin)
		: FDSTRMinimapViewModel::MakeFrame(Bounds, FDSTRMinimapViewModel::PanelSize);
	Capture->ClearShowOnlyComponents();
	for (UPrimitiveComponent* Floor : Floors)
	{
		Capture->ShowOnlyComponent(Floor);
	}
	Capture->TextureTarget = RenderTarget;
	Capture->OrthoWidth = Frame.WorldHalfExtent * 2.0f;
	SetActorLocationAndRotation(
		FVector(Frame.WorldCenter.X, Frame.WorldCenter.Y, Bounds.Max.Z + CaptureHeight),
		FRotator(-90.0f, 0.0f, 0.0f));
	Capture->ShowFlags.SetLighting(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetPostProcessing(false);
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->CaptureScene();

	UE_LOG(LogDSTR, Log, TEXT("DSTR_MINIMAP_FLOORS Rule=Tag Count=%d"), Floors.Num());
	UE_LOG(LogDSTR, Log, TEXT("DSTR_MINIMAP_FRAME Rule=%s Starts=%d Doors=%d Ortho=%.0f"),
		bAnchored ? TEXT("PlayAnchors") : TEXT("FloorBounds"), StartCount, DoorCount, Capture->OrthoWidth);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_MINIMAP_READY Floors=%d Center=%s HalfExtent=%.0f"),
		Floors.Num(), *Frame.WorldCenter.ToString(), Frame.WorldHalfExtent);
	return true;
}
