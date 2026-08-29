#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DSTRBossAbility.h"
#include "DSTRBossPhantomRushAbility.generated.h"

class ADediServerRPGCharacter;

UCLASS()
class DEDISERVERRPG_API UDSTRBossPhantomRushAbility : public UDSTRBossAbility
{
	GENERATED_BODY()

public:
	UDSTRBossPhantomRushAbility();
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
	void TickSweep();
	void EndCharge();
	void FinishRecovery();
	FTimerHandle SweepTimerHandle;
	FTimerHandle ChargeTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	TArray<TWeakObjectPtr<ADediServerRPGCharacter>> AlreadyHit;
};
