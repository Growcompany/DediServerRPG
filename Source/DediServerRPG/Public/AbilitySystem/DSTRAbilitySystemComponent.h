#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystemComponent.h"
#include "DSTRAbilitySystemComponent.generated.h"

class UGameplayAbility;

UCLASS()
class DEDISERVERRPG_API UDSTRAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UDSTRAbilitySystemComponent();

	void GiveStartupAbilities();
	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	bool TryMarkStartupEffectsApplied();
	void SetDeadEffectHandle(FActiveGameplayEffectHandle Handle) { DeadEffectHandle = Handle; }
	FActiveGameplayEffectHandle GetDeadEffectHandle() const { return DeadEffectHandle; }
	void ClearDeadEffectHandle() { DeadEffectHandle.Invalidate(); }

private:
	bool bStartupAbilitiesGiven = false;
	bool bStartupEffectsApplied = false;
	FActiveGameplayEffectHandle DeadEffectHandle;
};
