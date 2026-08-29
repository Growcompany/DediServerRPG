#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DSTRBossAbility.h"
#include "DSTRBossSiphonAbility.generated.h"

class ADediServerRPGCharacter;

UCLASS()
class DEDISERVERRPG_API UDSTRBossSiphonAbility : public UDSTRBossAbility
{
	GENERATED_BODY()

public:
	UDSTRBossSiphonAbility();
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
	void ExecutePull();
	void EndPull();
	void FinishRecovery();
	FTimerHandle PullTimerHandle;
	FTimerHandle PullEndTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	TWeakObjectPtr<ADediServerRPGCharacter> Victim;
};
