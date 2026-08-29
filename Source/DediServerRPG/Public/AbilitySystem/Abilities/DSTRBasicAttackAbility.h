#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DSTRGameplayAbility.h"
#include "DSTRBasicAttackAbility.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRBasicAttackAbility : public UDSTRGameplayAbility
{
	GENERATED_BODY()

public:
	UDSTRBasicAttackAbility();
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
	void ExecuteImpact();
	void FinishRecovery();
	uint8 ActiveVariant = 0;
	FTimerHandle ImpactTimerHandle;
	FTimerHandle RecoveryTimerHandle;
};
