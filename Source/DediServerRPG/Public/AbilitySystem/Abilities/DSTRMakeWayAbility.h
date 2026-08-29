#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DSTRGameplayAbility.h"
#include "DSTRMakeWayAbility.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRMakeWayAbility : public UDSTRGameplayAbility
{
	GENERATED_BODY()

public:
	UDSTRMakeWayAbility();
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
	void ExecuteLanding();
	void FinishRecovery();
	FTimerHandle LandingTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	FVector LeapDirection = FVector::ForwardVector;
};
