#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DSTRBossAbility.h"
#include "DSTRBossColossalBlowAbility.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRBossColossalBlowAbility : public UDSTRBossAbility
{
	GENERATED_BODY()

public:
	UDSTRBossColossalBlowAbility();
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
	void ExecuteBlow();
	void FinishRecovery();
	FTimerHandle BlowTimerHandle;
	FTimerHandle RecoveryTimerHandle;
};
