#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DSTRGameplayAbility.h"
#include "DSTRPulledAbility.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRPulledAbility : public UDSTRGameplayAbility
{
	GENERATED_BODY()

public:
	UDSTRPulledAbility();
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	static const FName PullTaskName;

private:
	UFUNCTION()
	void HandlePullFinished();
};
