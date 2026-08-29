#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DSTRGameplayAbility.h"
#include "DSTRReviveAbility.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRReviveAbility : public UDSTRGameplayAbility
{
	GENERATED_BODY()

public:
	UDSTRReviveAbility();
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	void ExecuteInteraction();
	void FinishRecovery();
	FTimerHandle InteractionTimerHandle;
	FTimerHandle RecoveryTimerHandle;
};
