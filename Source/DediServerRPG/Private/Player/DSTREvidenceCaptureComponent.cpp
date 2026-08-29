#include "Player/DSTREvidenceCaptureComponent.h"
#include "DSTRLog.h"

#include "Camera/CameraComponent.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Game/DSTRGameState.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Presentation/DSTRVisualAssetRegistry.h"
#include "TimerManager.h"
#include "UnrealClient.h"

UDSTREvidenceCaptureComponent::UDSTREvidenceCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool UDSTREvidenceCaptureComponent::IsFrameDumpEnabled(const FString& CommandLine)
{
	return FParse::Param(*CommandLine, TEXT("DSTRDumpFrames"));
}

bool UDSTREvidenceCaptureComponent::IsVisualEvidenceEnabled(const FString& CommandLine)
{
	return FParse::Param(*CommandLine, TEXT("DSTRVisualEvidence"));
}

bool UDSTREvidenceCaptureComponent::ShouldAttach(const FString& CommandLine, const bool bRenderingClient)
{
	return bRenderingClient
		&& (IsFrameDumpEnabled(CommandLine) || IsVisualEvidenceEnabled(CommandLine));
}

void UDSTREvidenceCaptureComponent::BeginPlay()
{
	Super::BeginPlay();
	const FString& CommandLine = FCommandLine::Get();
	bClipFrameDumpEnabled = IsFrameDumpEnabled(CommandLine);
	if (IsVisualEvidenceEnabled(CommandLine))
	{
		GetWorld()->GetTimerManager().SetTimer(
			VisualEvidenceTimerHandle,
			this,
			&UDSTREvidenceCaptureComponent::CaptureVisualEvidence,
			EvidenceIntervalSeconds,
			true,
			EvidenceFirstShotSeconds);
	}
}

void UDSTREvidenceCaptureComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bClipFrameDumpEnabled)
	{
		DumpClipFrame();
	}
}

void UDSTREvidenceCaptureComponent::DumpClipFrame()
{
	const ADSTRGameState* State = GetWorld() ? GetWorld()->GetGameState<ADSTRGameState>() : nullptr;
	if (!FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(this)
		|| !State || State->GetMatchPhase() == EDSTRMatchPhase::WaitingForPlayers)
	{
		return;
	}

	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Clips/Frames"));
	if (ClipFrameIndex == 0)
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
	}
	const ADediServerRPGCharacter* Character = Cast<ADediServerRPGCharacter>(GetOwner());
	UE_LOG(LogDSTR, Log, TEXT("DSTR_CLIP_FRAME Index=%d Speed=%.0f Action=%d"),
		ClipFrameIndex,
		Character ? Character->GetVelocity().Size2D() : 0.0f,
		Character ? static_cast<int32>(Character->GetCurrentCombatAction()) : 0);
	FScreenshotRequest::RequestScreenshot(
		FPaths::Combine(Directory, FString::Printf(TEXT("ClipFrame%05d.png"), ClipFrameIndex++)),
		true,
		false);
}

void UDSTREvidenceCaptureComponent::CaptureVisualEvidence()
{
	const FString Directory = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("Screenshots/Windows"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (!FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(this))
	{
		UE_LOG(LogDSTR, Log, TEXT("DSTR_VISUAL_EVIDENCE_DEFERRED Reason=PreloadPending"));
		return;
	}

	const ADediServerRPGCharacter* Character = Cast<ADediServerRPGCharacter>(GetOwner());
	USpringArmComponent* CameraBoom = Character ? Character->GetCameraBoom() : nullptr;
	const UCameraComponent* FollowCamera = Character ? Character->GetFollowCamera() : nullptr;
	if (CameraBoom && FollowCamera)
	{
		const FVector Origin = CameraBoom->GetComponentLocation();
		const FVector Desired = Origin - CameraBoom->GetTargetRotation().Vector() * CameraBoom->TargetArmLength;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DSTRCameraProbe), false, Character);
		const bool bBlocked = GetWorld()->SweepSingleByChannel(
			Hit, Origin, Desired, FQuat::Identity, CameraBoom->ProbeChannel,
			FCollisionShape::MakeSphere(CameraBoom->ProbeSize), Params);
		UE_LOG(LogDSTR, Log,
			TEXT("DSTR_VISUAL_EVIDENCE_CAMERA ArmFixed=%d CameraDist=%.0f Blocked=%d HitDist=%.0f HitActor=%s HitComp=%s"),
			CameraBoom->IsCollisionFixApplied() ? 1 : 0,
			FVector::Dist(FollowCamera->GetComponentLocation(), Character->GetActorLocation()),
			bBlocked ? 1 : 0,
			bBlocked ? Hit.Distance : CameraBoom->TargetArmLength,
			*GetNameSafe(Hit.GetActor()),
			*GetNameSafe(Hit.GetComponent()));
	}

	++VisualEvidenceShotIndex;
	const FString Filename = FPaths::Combine(
		Directory,
		VisualEvidenceShotIndex == 1
			? FString(TEXT("DSTR_HardeningEvidence.png"))
			: FString::Printf(TEXT("DSTR_HardeningEvidence_%d.png"), VisualEvidenceShotIndex));
	FScreenshotRequest::RequestScreenshot(Filename, true, false);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_VISUAL_EVIDENCE_REQUESTED File=%s"), *Filename);
	if (VisualEvidenceShotIndex >= EvidenceShotCount)
	{
		GetWorld()->GetTimerManager().ClearTimer(VisualEvidenceTimerHandle);
	}
}
