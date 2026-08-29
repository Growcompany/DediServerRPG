#include "Presentation/DSTRCombatGameplayCues.h"
#include "DSTRLog.h"

#include "Enemy/DSTREnemyCharacter.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Presentation/DSTRCombatFeedback.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "Presentation/DSTRVisualAssetRegistry.h"

namespace
{
	const FVector GateCurtainScale(3.0f, 3.0f, 2.0f);

	bool SpawnEffect(
		AActor* Target,
		const FGameplayCueParameters& Parameters,
		const FSoftObjectPath& EffectPath,
		const float Scale)
	{
		if (!Target || !Target->GetWorld()
			|| !FDSTRVisualAssetRegistry::ShouldLoadVisualAssets(Target->GetNetMode()))
		{
			return false;
		}

		UParticleSystem* Effect = Cast<UParticleSystem>(EffectPath.ResolveObject());
		if (!Effect)
		{
			return false;
		}

		const FVector Location = Parameters.Location.IsNearlyZero()
			? Target->GetActorLocation()
			: FVector(Parameters.Location);
		const FRotator Rotation = Parameters.Normal.IsNearlyZero()
			? Target->GetActorRotation()
			: Parameters.Normal.Rotation();
		UGameplayStatics::SpawnEmitterAtLocation(
			Target->GetWorld(),
			Effect,
			Location,
			Rotation,
			FVector(Scale),
			true);
		return true;
	}

	void PlayCueFeedback(
		AActor* Target,
		const FGameplayCueParameters& Parameters,
		const EDSTRCombatFeedback Feedback)
	{
		if (!Target)
		{
			return;
		}
		FDSTRCombatFeedbackRequest Request;
		Request.Feedback = Feedback;
		Request.Location = Parameters.Location.IsNearlyZero()
			? Target->GetActorLocation()
			: FVector(Parameters.Location);
		Request.Direction = FVector(Parameters.Normal);
		Request.Radius = Parameters.RawMagnitude;
		Request.InstigatorActor = Target;
		FDSTRCombatFeedbackPlayer::Play(Target->GetWorld(), Request);
	}
}

bool UDSTRBasicAttackGameplayCue::OnExecute_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	const bool bSpawned = SpawnEffect(
		MyTarget,
		Parameters,
		FDSTRVisualAssetRegistry::GetBasicAttackEffect(),
		GetPresentationScale());
	PlayCueFeedback(MyTarget, Parameters, EDSTRCombatFeedback::BasicAttack);
	return bSpawned;
}

ADSTRLoopingGameplayCue::ADSTRLoopingGameplayCue()
{
	NumPreallocatedInstances = 0;
	bAutoDestroyOnRemove = true;
}

bool ADSTRLoopingGameplayCue::WhileActive_Implementation(
	AActor* Target,
	const FGameplayCueParameters& Parameters)
{
	Super::WhileActive_Implementation(Target, Parameters);

	UWorld* World = Target ? Target->GetWorld() : GetWorld();
	if (!World || !FDSTRCombatFeedbackPolicy::ShouldPresent(World->GetNetMode()))
	{
		return false;
	}

	UE_LOG(LogDSTR, Log, TEXT("DSTR_CUE_ACTIVE Cue=%s Target=%s Radius=%.0f"),
		*GameplayCueTag.ToString(), *GetNameSafe(Target), Parameters.RawMagnitude);

	StopEffect();
	const FSoftObjectPath* EffectPath = GetEffectPath();
	UParticleSystem* Effect = EffectPath ? Cast<UParticleSystem>(EffectPath->ResolveObject()) : nullptr;
	if (Effect)
	{
		EffectComponent = UGameplayStatics::SpawnEmitterAtLocation(
			World,
			Effect,
			FVector(Parameters.Location),
			bUseParametersRotation ? FVector(Parameters.Normal).Rotation() : FRotator::ZeroRotator,
			GetEffectScale(Parameters.RawMagnitude),
			true);
	}

	if (bPlayFeedback)
	{
		FDSTRCombatFeedbackRequest Request;
		Request.Feedback = Feedback;
		Request.Location = FVector(Parameters.Location);
		Request.Radius = Parameters.RawMagnitude;
		Request.bSoundOnly = bSoundOnly;
		const ADSTREnemyCharacter* Enemy = Cast<ADSTREnemyCharacter>(Target);
		Request.bBossVariant = Enemy && Enemy->IsBoss();
		Request.VictimActor = Target;
		FDSTRCombatFeedbackPlayer::Play(World, Request);
	}
	return false;
}

bool ADSTRLoopingGameplayCue::OnRemove_Implementation(
	AActor* Target,
	const FGameplayCueParameters& Parameters)
{
	StopEffect();
	return Super::OnRemove_Implementation(Target, Parameters);
}

bool ADSTRLoopingGameplayCue::Recycle()
{
	StopEffect();
	return Super::Recycle();
}

void ADSTRLoopingGameplayCue::StopEffect()
{
	if (UParticleSystemComponent* Component = EffectComponent.Get())
	{
		Component->DeactivateSystem();
	}
	EffectComponent.Reset();
}

ADSTREnemyTelegraphGameplayCue::ADSTREnemyTelegraphGameplayCue()
{
	Feedback = EDSTRCombatFeedback::EnemyTelegraph;
}

const FSoftObjectPath* ADSTREnemyTelegraphGameplayCue::GetEffectPath() const
{
	return &FDSTRVisualAssetRegistry::GetEnemyTelegraphEffect();
}

FVector ADSTREnemyTelegraphGameplayCue::GetEffectScale(const float Radius) const
{
	return FDSTRCombatPresentation::GetTelegraphScale3D(Radius);
}

ADSTRBossSiphonGameplayCue::ADSTRBossSiphonGameplayCue()
{
	Feedback = EDSTRCombatFeedback::BossSiphon;
}

const FSoftObjectPath* ADSTRBossSiphonGameplayCue::GetEffectPath() const
{
	return &FDSTRVisualAssetRegistry::GetEnemyTelegraphEffect();
}

FVector ADSTRBossSiphonGameplayCue::GetEffectScale(const float Radius) const
{
	return FDSTRCombatPresentation::GetTelegraphScale3D(Radius);
}

ADSTRBossRushGameplayCue::ADSTRBossRushGameplayCue()
{
	Feedback = EDSTRCombatFeedback::BossRush;
}

ADSTRBossWindupGameplayCue::ADSTRBossWindupGameplayCue()
{
	Feedback = EDSTRCombatFeedback::EnemyTelegraph;
	bSoundOnly = true;
}

ADSTRGateSealedGameplayCue::ADSTRGateSealedGameplayCue()
{
	bPlayFeedback = false;
	bUseParametersRotation = true;
}

const FSoftObjectPath* ADSTRGateSealedGameplayCue::GetEffectPath() const
{
	return &FDSTRVisualAssetRegistry::GetGateSealedEffect();
}

FVector ADSTRGateSealedGameplayCue::GetEffectScale(const float Radius) const
{
	return GateCurtainScale;
}
