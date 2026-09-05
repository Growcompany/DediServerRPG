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
	void AbilityInputTagReleased(const FGameplayTag& InputTag);
	void ClearAbilityInput();
	bool TryMarkStartupEffectsApplied();
	void SetDeadEffectHandle(FActiveGameplayEffectHandle Handle) { DeadEffectHandle = Handle; }
	FActiveGameplayEffectHandle GetDeadEffectHandle() const { return DeadEffectHandle; }
	void ClearDeadEffectHandle() { DeadEffectHandle.Invalidate(); }

private:
	void ClearBufferedInput();
	void RetryBufferedInput();
	FGameplayAbilitySpecHandle BufferedAbilityHandle;
	TWeakObjectPtr<AActor> BufferedAvatar;
	FTimerHandle BufferedInputTimer;
	double BufferedInputExpiresAt = 0.0;
	static constexpr float InputBufferSeconds = 0.18f;

	bool bStartupAbilitiesGiven = false;
	bool bStartupEffectsApplied = false;
	FActiveGameplayEffectHandle DeadEffectHandle;
};
