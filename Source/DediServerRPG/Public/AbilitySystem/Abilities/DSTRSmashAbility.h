#pragma once

#include "AbilitySystem/Abilities/DSTRGameplayAbility.h"
#include "DSTRSmashAbility.generated.h"

// RMB 대지 강타: 제자리에서 주변 광역 피해 + 넉백.
UCLASS()
class DEDISERVERRPG_API UDSTRSmashAbility : public UDSTRGameplayAbility
{
	GENERATED_BODY()

public:
	UDSTRSmashAbility();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	void ExecuteImpact();
	void FinishRecovery();

	FTimerHandle ImpactTimerHandle;
	FTimerHandle RecoveryTimerHandle;
};
