#include "Presentation/DSTRCombatFeedback.h"
#include "DSTRLog.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Presentation/DSTRCombatCameraShake.h"
#include "Presentation/DSTRVisualAssetRegistry.h"
#include "Sound/SoundBase.h"

#if !UE_BUILD_SHIPPING
namespace
{
	TSet<FSoftObjectPath> WarnedMissingSounds;
	TSet<FSoftObjectPath> ReportedReadySounds;

	void ReportSoundResolution(const FSoftObjectPath& Path, const bool bResolved, const bool bPreloadDone)
	{
		if (!bResolved)
		{
			if (bPreloadDone && !WarnedMissingSounds.Contains(Path))
			{
				WarnedMissingSounds.Add(Path);
				UE_LOG(LogDSTR, Warning, TEXT("DSTR_FEEDBACK_SOUND_MISSING Path=%s"), *Path.ToString());
			}
			return;
		}
		if (!ReportedReadySounds.Contains(Path))
		{
			ReportedReadySounds.Add(Path);
			UE_LOG(LogDSTR, Log, TEXT("DSTR_FEEDBACK_SOUND_READY Path=%s"), *Path.ToString());
		}
	}
}
#endif

bool FDSTRCombatFeedbackPolicy::ShouldPresent(const ENetMode NetMode)
{
	return FDSTRVisualAssetRegistry::ShouldLoadVisualAssets(NetMode);
}

EDSTRFeedbackViewer FDSTRCombatFeedbackPolicy::ResolveViewer(
	const bool bLocalInstigator,
	const bool bLocalVictim)
{
	if (bLocalVictim)
	{
		return EDSTRFeedbackViewer::Victim;
	}
	return bLocalInstigator ? EDSTRFeedbackViewer::Instigator : EDSTRFeedbackViewer::Remote;
}

bool FDSTRCombatFeedbackPolicy::IsVictimFeedback(const EDSTRCombatFeedback Feedback)
{
	return Feedback == EDSTRCombatFeedback::HitTaken
		|| Feedback == EDSTRCombatFeedback::Downed
		|| Feedback == EDSTRCombatFeedback::Revived;
}

float FDSTRCombatFeedbackPolicy::ClampRadius(const float Radius)
{
	return FMath::Clamp(Radius, 0.0f, MaxShakeRadius);
}

float FDSTRCombatFeedbackPolicy::ClampShakeScale(const float Scale)
{
	return FMath::Clamp(Scale, 0.0f, 1.0f);
}

float FDSTRCombatFeedbackPolicy::GetShakeScale(
	const EDSTRCombatFeedback Feedback,
	const EDSTRFeedbackViewer Viewer,
	const float Distance,
	const float Radius)
{
	switch (Feedback)
	{
	case EDSTRCombatFeedback::BasicAttack:
		return Viewer == EDSTRFeedbackViewer::Instigator ? 0.15f : 0.0f;
	case EDSTRCombatFeedback::Fortify:
		return Viewer == EDSTRFeedbackViewer::Instigator ? 0.25f : 0.0f;
	case EDSTRCombatFeedback::MakeWay:
		return Viewer == EDSTRFeedbackViewer::Instigator ? 0.5f : 0.0f;
	case EDSTRCombatFeedback::Charge:
		return Viewer == EDSTRFeedbackViewer::Instigator ? 0.3f : 0.0f;
	case EDSTRCombatFeedback::Reckoning:
		return GetShakeScale(EDSTRCombatFeedback::BossImpact, Viewer, Distance, Radius);
	case EDSTRCombatFeedback::HitDealt:
		return Viewer == EDSTRFeedbackViewer::Instigator ? 0.3f : 0.0f;
	case EDSTRCombatFeedback::HitTaken:
		return Viewer == EDSTRFeedbackViewer::Victim ? 0.45f : 0.0f;
	case EDSTRCombatFeedback::Downed:
		return Viewer == EDSTRFeedbackViewer::Victim ? 0.8f : 0.0f;
	case EDSTRCombatFeedback::BossImpact:
	{
		const float ClampedRadius = ClampRadius(Radius);
		const float ClampedDistance = FMath::Max(0.0f, Distance);
		if (ClampedRadius <= 0.0f || ClampedDistance >= ClampedRadius)
		{
			return 0.0f;
		}
		return ClampShakeScale(0.9f * (1.0f - ClampedDistance / ClampedRadius));
	}
	default:
		return 0.0f;
	}
}

FDSTRShakeSample FDSTRCombatFeedbackPolicy::EvaluateShake(
	const float Elapsed,
	const float Duration,
	const float Amplitude,
	const float Frequency)
{
	FDSTRShakeSample Sample;
	if (Duration <= 0.0f || Elapsed < 0.0f || Elapsed >= Duration)
	{
		return Sample;
	}

	const float Envelope = 1.0f - Elapsed / Duration;
	const float Wave = FMath::Sin(Elapsed * Frequency * 2.0f * PI) * Envelope * Amplitude;
	Sample.LocationOffset = FVector(0.0f, Wave * 0.5f, Wave);
	Sample.RotationOffset = FRotator(Wave * 0.05f, 0.0f, Wave * 0.03f);
	return Sample;
}

void FDSTRCombatFeedbackPlayer::Play(UWorld* World, const FDSTRCombatFeedbackRequest& Request)
{
	if (!World || !FDSTRCombatFeedbackPolicy::ShouldPresent(World->GetNetMode()))
	{
		return;
	}
	if (!Request.bSkipSound)
	{
		PlaySound(*World, Request);
	}
	if (!Request.bSoundOnly)
	{
		PlayCameraShake(*World, Request);
	}
}

void FDSTRCombatFeedbackPlayer::PlaySound(UWorld& World, const FDSTRCombatFeedbackRequest& Request)
{
	const FDSTRFeedbackSound& Spec =
		FDSTRVisualAssetRegistry::GetFeedbackSound(Request.Feedback, Request.bBossVariant);
	if (!Spec.Path.IsValid())
	{
		return;
	}

	USoundBase* Sound = Cast<USoundBase>(Spec.Path.ResolveObject());
#if !UE_BUILD_SHIPPING
	ReportSoundResolution(
		Spec.Path, Sound != nullptr, FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(&World));
#endif
	if (!Sound)
	{
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(&World, Sound, Request.Location, Spec.Volume, Spec.Pitch);
}

void FDSTRCombatFeedbackPlayer::PlayCameraShake(UWorld& World, const FDSTRCombatFeedbackRequest& Request)
{
	APlayerController* Controller = World.GetFirstPlayerController();
	if (!Controller || !Controller->IsLocalController() || !Controller->PlayerCameraManager)
	{
		return;
	}

	const APawn* LocalPawn = Controller->GetPawn();
	const EDSTRFeedbackViewer Viewer = FDSTRCombatFeedbackPolicy::ResolveViewer(
		LocalPawn && LocalPawn == Request.InstigatorActor,
		LocalPawn && LocalPawn == Request.VictimActor);
	const float Distance = LocalPawn
		? FVector::Dist(LocalPawn->GetActorLocation(), Request.Location)
		: TNumericLimits<float>::Max();
	const float Scale = FDSTRCombatFeedbackPolicy::GetShakeScale(
		Request.Feedback, Viewer, Distance, Request.Radius);
	if (Scale <= 0.0f)
	{
		return;
	}

	Controller->PlayerCameraManager->StartCameraShake(UDSTRCombatCameraShake::StaticClass(), Scale);
}
