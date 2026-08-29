#include "AbilitySystem/Abilities/DSTRChargeAbility.h"
#include "DSTRLog.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
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

namespace
{
	constexpr float ChargeSweepInterval = 0.05f;
}

UDSTRChargeAbility::UDSTRChargeAbility()
{
	AbilityTags.AddTag(DSTRGameplayTags::Ability_Skill_Charge.GetTag());
	ActivationOwnedTags.AddTag(DSTRGameplayTags::State_Attacking.GetTag());
	CostGameplayEffectClass = UDSTRChargeCostEffect::StaticClass();
	CooldownGameplayEffectClass = UDSTRChargeCooldownEffect::StaticClass();
}

void UDSTRChargeAbility::ActivateAbility(
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

	AlreadyHit.Reset();
	ChargeStartTime = World->GetTimeSeconds();
	ChargeDirection = Character->GetController()
		? FRotator(0.0f, Character->GetControlRotation().Yaw, 0.0f).Vector()
		: Character->GetActorForwardVector().GetSafeNormal2D();
	Character->StartCombatAction(EDSTRCombatAction::Charge);
	Character->PlayAbilityFeedback(
		EDSTRCombatFeedback::Charge,
		Character->GetActorLocation(),
		ChargeDirection,
		FDSTRDamageRules::ChargeSweepRadius);

	if (UAbilityTask_ApplyRootMotionConstantForce* ChargeTask =
		UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			NAME_None,
			ChargeDirection,
			FDSTRDamageRules::ChargeDistance / FDSTRDamageRules::ChargeDuration,
			FDSTRDamageRules::ChargeDuration,
			false,
			nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity,
			FVector::ZeroVector,
			0.0f,
			false))
	{
		ChargeTask->ReadyForActivation();
	}

	if (FDSTRCombatPresentation::ShouldScheduleAuthorityImpact(Character->HasAuthority()))
	{
		World->GetTimerManager().SetTimer(
			SweepTimerHandle, this, &UDSTRChargeAbility::TickSweep, ChargeSweepInterval, true);
	}
	World->GetTimerManager().SetTimer(
		RecoveryTimerHandle,
		this,
		&UDSTRChargeAbility::FinishRecovery,
		FDSTRCombatPresentation::GetCancelDelay(EDSTRCombatAction::Charge, 0),
		false);
}

void UDSTRChargeAbility::TickSweep()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ADediServerRPGCharacter* Character = ActorInfo
		? Cast<ADediServerRPGCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World || !Character->HasAuthority())
	{
		return;
	}

	if (World->GetTimeSeconds() - ChargeStartTime > FDSTRDamageRules::ChargeDuration)
	{
		World->GetTimerManager().ClearTimer(SweepTimerHandle);
		return;
	}

	const FVector Center = Character->GetActorLocation();
	TArray<ADSTREnemyCharacter*> Candidates;
	FDSTRHitQuery::GatherEnemies(Character, Center, FDSTRDamageRules::ChargeSweepRadius, Candidates);
	for (ADSTREnemyCharacter* Enemy : Candidates)
	{
		if (AlreadyHit.Contains(Enemy))
		{
			continue;
		}
		const float Distance = static_cast<float>(FVector::Dist2D(Enemy->GetActorLocation(), Center));
		if (!FDSTRDamageRules::IsInChargeSweep(
			Distance, Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius()))
		{
			continue;
		}
		AlreadyHit.Add(Enemy);
		UE_LOG(LogDSTR, Log, TEXT("DSTR_HIT Source=%s Target=%s Dist=%.0f Reach=%.0f Angle=%.0f Skill=Charge"),
			*Character->GetName(), *Enemy->GetName(), Distance, FDSTRDamageRules::ChargeSweepRadius, 0.0f);
		if (UDSTRCombatLibrary::ApplyDamage(
			ActorInfo->AbilitySystemComponent.Get(),
			Enemy->GetAbilitySystemComponent(),
			FDSTRDamageRules::ChargeDamageMultiplier))
		{
			Enemy->AddThreat(Character, UDSTRCombatLibrary::GetOutgoingDamage(
				ActorInfo->AbilitySystemComponent.Get(), FDSTRDamageRules::ChargeDamageMultiplier));
			if (!Enemy->IsCombatantDead())
			{
				UDSTRCombatLibrary::ApplyStagger(
					ActorInfo->AbilitySystemComponent.Get(),
					Enemy->GetAbilitySystemComponent(),
					FDSTRDamageRules::ChargeStaggerSeconds);
				Enemy->PlayAbilityFeedback(
					Enemy->GetActorLocation(), 180.0f, EDSTRCombatFeedback::HitDealt, Character);
			}
		}
	}
}

void UDSTRChargeAbility::FinishRecovery()
{
	FinishAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
}

void UDSTRChargeAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(SweepTimerHandle);
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(RecoveryTimerHandle);
	}
	AlreadyHit.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
