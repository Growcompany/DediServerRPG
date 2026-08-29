#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DSTRGameplayAbility.h"
#include "DSTRChargeAbility.generated.h"

class ADSTREnemyCharacter;

UCLASS()
class DEDISERVERRPG_API UDSTRChargeAbility : public UDSTRGameplayAbility
{
	GENERATED_BODY()

public:
	UDSTRChargeAbility();
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
	void TickSweep();
	void FinishRecovery();
	FTimerHandle SweepTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	TArray<TWeakObjectPtr<ADSTREnemyCharacter>> AlreadyHit;
	FVector ChargeDirection = FVector::ForwardVector;
	double ChargeStartTime = -1.0;
};
