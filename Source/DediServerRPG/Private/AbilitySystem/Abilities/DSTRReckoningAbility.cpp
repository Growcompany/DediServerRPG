#include "AbilitySystem/Abilities/DSTRReckoningAbility.h"
#include "DSTRLog.h"

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
#include "Presentation/DSTRCombatPresentation.h"
#include "TimerManager.h"

UDSTRReckoningAbility::UDSTRReckoningAbility()
{
	AbilityTags.AddTag(DSTRGameplayTags::Ability_Skill_Reckoning.GetTag());
	ActivationOwnedTags.AddTag(DSTRGameplayTags::State_Attacking.GetTag());
	CostGameplayEffectClass = UDSTRReckoningCostEffect::StaticClass();
	CooldownGameplayEffectClass = UDSTRReckoningCooldownEffect::StaticClass();
}

void UDSTRReckoningAbility::ActivateAbility(
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

	BlastCenter = Character->GetActorLocation();
	Character->StartCombatAction(EDSTRCombatAction::Reckoning);

	const float BlastDelay = FDSTRCombatPresentation::GetImpactDelay(EDSTRCombatAction::Reckoning, 0);
	if (Character->HasAuthority())
	{
		Character->PlayAbilityFeedback(
			EDSTRCombatFeedback::ReckoningWarning,
			BlastCenter,
			Character->GetActorForwardVector(),
			FDSTRDamageRules::ReckoningRadius);
		World->GetTimerManager().SetTimer(
			BlastTimerHandle, this, &UDSTRReckoningAbility::ExecuteBlast, BlastDelay, false);
	}
	World->GetTimerManager().SetTimer(
		RecoveryTimerHandle,
		this,
		&UDSTRReckoningAbility::FinishRecovery,
		FDSTRCombatPresentation::GetCancelDelay(EDSTRCombatAction::Reckoning, 0),
		false);
}

void UDSTRReckoningAbility::ExecuteBlast()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ADediServerRPGCharacter* Character = ActorInfo
		? Cast<ADediServerRPGCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World || !Character->HasAuthority())
	{
		return;
	}

	Character->PlayAbilityFeedback(
		EDSTRCombatFeedback::Reckoning,
		BlastCenter,
		Character->GetActorForwardVector(),
		FDSTRDamageRules::ReckoningRadius);

	TArray<ADSTREnemyCharacter*> Candidates;
	FDSTRHitQuery::GatherEnemies(Character, BlastCenter, FDSTRDamageRules::ReckoningRadius, Candidates);
	for (ADSTREnemyCharacter* Enemy : Candidates)
	{
		const float Distance = static_cast<float>(FVector::Dist2D(Enemy->GetActorLocation(), BlastCenter));
		if (Distance > FDSTRDamageRules::ReckoningRadius
			+ Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius())
		{
			continue;
		}
		UE_LOG(LogDSTR, Log, TEXT("DSTR_HIT Source=%s Target=%s Dist=%.0f Reach=%.0f Angle=%.0f Skill=Reckoning"),
			*Character->GetName(), *Enemy->GetName(), Distance, FDSTRDamageRules::ReckoningRadius, 0.0f);
		if (UDSTRCombatLibrary::ApplyDamage(
			ActorInfo->AbilitySystemComponent.Get(),
			Enemy->GetAbilitySystemComponent(),
			FDSTRDamageRules::ReckoningDamageMultiplier))
		{
			Enemy->AddThreat(Character, UDSTRCombatLibrary::GetOutgoingDamage(
				ActorInfo->AbilitySystemComponent.Get(), FDSTRDamageRules::ReckoningDamageMultiplier));
		}
		if (!Enemy->IsCombatantDead())
		{
			Enemy->ApplyStun(Character);
			Enemy->PlayAbilityFeedback(
				Enemy->GetActorLocation(), 220.0f, EDSTRCombatFeedback::HitDealt, Character);
		}
	}
}

void UDSTRReckoningAbility::FinishRecovery()
{
	FinishAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
}

void UDSTRReckoningAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(BlastTimerHandle);
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(RecoveryTimerHandle);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
