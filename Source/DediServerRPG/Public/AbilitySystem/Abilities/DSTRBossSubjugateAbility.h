#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DSTRBossAbility.h"
#include "DSTRBossSubjugateAbility.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRBossSubjugateAbility : public UDSTRBossAbility
{
	GENERATED_BODY()

public:
	UDSTRBossSubjugateAbility();
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
	void BeginSwing();
	void ExecuteSlam();
	void FinishRecovery();
	FTimerHandle SwingTimerHandle;
	FTimerHandle SlamTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	FVector SlamCenter = FVector::ZeroVector;
};
