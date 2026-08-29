#include "AbilitySystem/Abilities/DSTRMakeWayAbility.h"
#include "DSTRLog.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionJumpForce.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DSTRGameplayEffects.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Combat/DSTRCombatLibrary.h"
#include "Combat/DSTRDamageRules.h"
#include "Combat/DSTRHitQuery.h"
#include "Components/CapsuleComponent.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "Engine/World.h"
#include "GameFramework/RootMotionSource.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "TimerManager.h"

UDSTRMakeWayAbility::UDSTRMakeWayAbility()
{
	AbilityTags.AddTag(DSTRGameplayTags::Ability_Skill_MakeWay.GetTag());
	ActivationOwnedTags.AddTag(DSTRGameplayTags::State_Attacking.GetTag());
	CostGameplayEffectClass = UDSTRMakeWayCostEffect::StaticClass();
	CooldownGameplayEffectClass = UDSTRMakeWayCooldownEffect::StaticClass();
}

void UDSTRMakeWayAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	ADediServerRPGCharacter* Character = Cast<ADediServerRPGCharacter>(ActorInfo->AvatarActor.Get());
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World)
	{
		FinishAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	LeapDirection = Character->GetController()
		? FRotator(0.0f, Character->GetControlRotation().Yaw, 0.0f).Vector()
		: Character->GetActorForwardVector().GetSafeNormal2D();
	const FRotator LeapRotation = LeapDirection.Rotation();
	Character->SetActorRotation(FRotator(0.0f, LeapRotation.Yaw, 0.0f));
	Character->StartCombatAction(EDSTRCombatAction::MakeWay);

	const float Distance = FDSTRDamageRules::MakeWayLeapDistance;
	const float LandingTime = FDSTRCombatPresentation::GetImpactDelay(EDSTRCombatAction::MakeWay, 0);
	if (UAbilityTask_ApplyRootMotionJumpForce* JumpTask =
		UAbilityTask_ApplyRootMotionJumpForce::ApplyRootMotionJumpForce(
			this,
			NAME_None,
			FRotator(0.0f, LeapRotation.Yaw, 0.0f),
			Distance,
			FDSTRDamageRules::MakeWayLeapHeight,
			LandingTime,
			LandingTime,
			true,
			ERootMotionFinishVelocityMode::ClampVelocity,
			FVector::ZeroVector,
			0.0f,
			nullptr,
			nullptr))
	{
		JumpTask->ReadyForActivation();
	}

	World->GetTimerManager().SetTimer(
		RecoveryTimerHandle,
		this,
		&UDSTRMakeWayAbility::FinishRecovery,
		FDSTRCombatPresentation::GetCancelDelay(EDSTRCombatAction::MakeWay, 0),
		false);
	if (FDSTRCombatPresentation::ShouldScheduleAuthorityImpact(Character->HasAuthority()))
	{
		World->GetTimerManager().SetTimer(
			LandingTimerHandle, this, &UDSTRMakeWayAbility::ExecuteLanding, LandingTime, false);
	}
}

void UDSTRMakeWayAbility::ExecuteLanding()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ADediServerRPGCharacter* Character = ActorInfo
		? Cast<ADediServerRPGCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World || !Character->HasAuthority())
	{
		return;
	}

	const FVector Center = Character->GetActorLocation();
	Character->PlayAbilityFeedback(
		EDSTRCombatFeedback::MakeWay, Center, LeapDirection, FDSTRDamageRules::MakeWayLandingRadius);

	TArray<ADSTREnemyCharacter*> Candidates;
	FDSTRHitQuery::GatherEnemies(Character, Center, FDSTRDamageRules::MakeWayLandingRadius, Candidates);
	for (ADSTREnemyCharacter* Enemy : Candidates)
	{
		const float Distance = static_cast<float>(FVector::Dist2D(Enemy->GetActorLocation(), Center));
		const float CapsuleRadius = Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius();
		if (!FDSTRDamageRules::IsInLandingRadius(Distance, CapsuleRadius))
		{
			continue;
		}
		UE_LOG(LogDSTR, Log, TEXT("DSTR_HIT Source=%s Target=%s Dist=%.0f Reach=%.0f Angle=%.0f Skill=MakeWay"),
			*Character->GetName(), *Enemy->GetName(), Distance,
			FDSTRDamageRules::MakeWayLandingRadius, 0.0f);
		if (UDSTRCombatLibrary::ApplyDamage(
			ActorInfo->AbilitySystemComponent.Get(),
			Enemy->GetAbilitySystemComponent(),
			FDSTRDamageRules::MakeWayDamageMultiplier))
		{
			Enemy->AddThreat(Character, UDSTRCombatLibrary::GetOutgoingDamage(
				ActorInfo->AbilitySystemComponent.Get(), FDSTRDamageRules::MakeWayDamageMultiplier));
			Enemy->LaunchCharacter(
				FDSTRDamageRules::KnockbackVelocity(
					Center, Enemy->GetActorLocation(), LeapDirection,
					FDSTRDamageRules::MakeWayKnockbackSpeed),
				true,
				false);
			if (!Enemy->IsCombatantDead())
			{
				Enemy->ApplyThrown(Character, FDSTRDamageRules::MakeWayStaggerSeconds);
				Enemy->PlayAbilityFeedback(
					Enemy->GetActorLocation(), 200.0f, EDSTRCombatFeedback::HitDealt, Character);
			}
		}
	}
}

void UDSTRMakeWayAbility::FinishRecovery()
{
	FinishAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
}

void UDSTRMakeWayAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(LandingTimerHandle);
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(RecoveryTimerHandle);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
