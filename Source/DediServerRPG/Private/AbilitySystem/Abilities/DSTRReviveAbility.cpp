#include "AbilitySystem/Abilities/DSTRReviveAbility.h"

#include "AbilitySystem/DSTRGameplayTags.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "TimerManager.h"

UDSTRReviveAbility::UDSTRReviveAbility()
{
	AbilityTags.AddTag(DSTRGameplayTags::Ability_Interaction_Revive.GetTag());
	ActivationOwnedTags.AddTag(DSTRGameplayTags::State_Attacking.GetTag());
}

void UDSTRReviveAbility::ActivateAbility(
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
	if (!Character)
	{
		FinishAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}
	Character->StartCombatAction(EDSTRCombatAction::Revive);

	const FDSTRCombatActionProfile& Profile =
		FDSTRCombatPresentation::GetProfile(EDSTRCombatAction::Revive);
	if (Character->HasAuthority())
	{
		Character->GetWorldTimerManager().SetTimer(
			InteractionTimerHandle,
			this,
			&UDSTRReviveAbility::ExecuteInteraction,
			Profile.ImpactDelay,
			false);
	}
	Character->GetWorldTimerManager().SetTimer(
		RecoveryTimerHandle,
		this,
		&UDSTRReviveAbility::FinishRecovery,
		Profile.RecoveryDuration,
		false);
}

void UDSTRReviveAbility::ExecuteInteraction()
{
	if (ADediServerRPGCharacter* Character = GetCurrentActorInfo()
		? Cast<ADediServerRPGCharacter>(GetCurrentActorInfo()->AvatarActor.Get()) : nullptr)
	{
		Character->TryInteractFromServer();
	}
}

void UDSTRReviveAbility::FinishRecovery()
{
	FinishAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
}

void UDSTRReviveAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(InteractionTimerHandle);
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(RecoveryTimerHandle);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
