#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystem/Abilities/DSTRGameplayAbility.h"
#include "DSTRSprintAbility.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRSprintAbility : public UDSTRGameplayAbility
{
	GENERATED_BODY()

public:
	UDSTRSprintAbility();
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	void CheckStamina();
	FTimerHandle StaminaWatchHandle;
	FActiveGameplayEffectHandle SpeedEffectHandle;
	FActiveGameplayEffectHandle CostEffectHandle;
};
