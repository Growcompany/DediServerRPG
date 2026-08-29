#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DSTRGameplayAbility.h"
#include "DSTRFortifyAbility.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRFortifyAbility : public UDSTRGameplayAbility
{
	GENERATED_BODY()

public:
	UDSTRFortifyAbility();
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
	void FinishRecovery();
	FTimerHandle RecoveryTimerHandle;
};
