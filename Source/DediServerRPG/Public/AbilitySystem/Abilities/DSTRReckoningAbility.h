#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DSTRGameplayAbility.h"
#include "DSTRReckoningAbility.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRReckoningAbility : public UDSTRGameplayAbility
{
	GENERATED_BODY()

public:
	UDSTRReckoningAbility();
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
	void ExecuteBlast();
	void FinishRecovery();
	FTimerHandle BlastTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	FVector BlastCenter = FVector::ZeroVector;
};
