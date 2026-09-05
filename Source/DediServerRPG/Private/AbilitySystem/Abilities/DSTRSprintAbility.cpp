#include "AbilitySystem/Abilities/DSTRSprintAbility.h"
#include "DSTRLog.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DSTRAttributeSet.h"
#include "AbilitySystem/DSTRGameplayEffects.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Engine/World.h"
#include "Player/DSTRHeroMovementTuning.h"
#include "TimerManager.h"

namespace
{
	float GetStamina(const UAbilitySystemComponent& ASC)
	{
		return ASC.GetNumericAttribute(UDSTRAttributeSet::GetStaminaAttribute());
	}
}

UDSTRSprintAbility::UDSTRSprintAbility()
{
	AbilityTags.AddTag(DSTRGameplayTags::Ability_Skill_Sprint.GetTag());
}

void UDSTRSprintAbility::ActivateAbility(
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

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	UWorld* World = ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor->GetWorld() : nullptr;
	if (!ASC || !World || GetStamina(*ASC) <= 0.0f)
	{
		FinishAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	SpeedEffectHandle = ApplyGameplayEffectToOwner(
		Handle, ActorInfo, ActivationInfo, GetDefault<UDSTRSprintEffect>(), 1.0f);
	CostEffectHandle = ApplyGameplayEffectToOwner(
		Handle, ActorInfo, ActivationInfo, GetDefault<UDSTRSprintCostEffect>(), 1.0f);

	World->GetTimerManager().SetTimer(
		StaminaWatchHandle,
		this,
		&UDSTRSprintAbility::CheckStamina,
		FDSTRHeroMovementTuning::SprintCostPeriod,
		true);

	UE_LOG(LogDSTR, Log, TEXT("DSTR_SPRINT Start Avatar=%s Stamina=%.0f"),
		*GetNameSafe(ActorInfo->AvatarActor.Get()), GetStamina(*ASC));
}

void UDSTRSprintAbility::CheckStamina()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC && GetStamina(*ASC) > 0.0f)
	{
		return;
	}

	UE_LOG(LogDSTR, Log, TEXT("DSTR_SPRINT End Reason=Exhausted"));
	FinishAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo());
}

void UDSTRSprintAbility::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	UE_LOG(LogDSTR, Log, TEXT("DSTR_SPRINT End Reason=Released"));
	FinishAbility(Handle, ActorInfo, ActivationInfo);
}

void UDSTRSprintAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(StaminaWatchHandle);
	}
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->RemoveActiveGameplayEffect(SpeedEffectHandle);
		ASC->RemoveActiveGameplayEffect(CostEffectHandle);
	}
	SpeedEffectHandle.Invalidate();
	CostEffectHandle.Invalidate();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
