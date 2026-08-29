#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DSTRGameplayAbility.h"
#include "DSTREnemyAttackAbility.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTREnemyAttackAbility : public UDSTRGameplayAbility
{
	GENERATED_BODY()

public:
	UDSTREnemyAttackAbility();
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
	void ExecuteMelee();
	void FinishRecovery();

	uint8 ActiveVariant = 0;
	FTimerHandle MeleeTimerHandle;
	FTimerHandle RecoveryTimerHandle;
};
