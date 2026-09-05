#include "AbilitySystem/DSTRAbilitySystemComponent.h"

#include "AbilitySystem/DSTRGameplayTags.h"
#include "AbilitySystem/Abilities/DSTRBasicAttackAbility.h"
#include "AbilitySystem/Abilities/DSTRFortifyAbility.h"
#include "AbilitySystem/Abilities/DSTRChargeAbility.h"
#include "AbilitySystem/Abilities/DSTRSmashAbility.h"
#include "AbilitySystem/Abilities/DSTRPulledAbility.h"
#include "AbilitySystem/Abilities/DSTRReckoningAbility.h"
#include "AbilitySystem/Abilities/DSTRReviveAbility.h"
#include "AbilitySystem/Abilities/DSTRSprintAbility.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	bool IsBufferedCombatInput(const FGameplayTag& InputTag)
	{
		return InputTag == DSTRGameplayTags::InputTag_Ability_BasicAttack.GetTag()
			|| InputTag == DSTRGameplayTags::InputTag_Ability_Fortify.GetTag()
			|| InputTag == DSTRGameplayTags::InputTag_Ability_Charge.GetTag()
			|| InputTag == DSTRGameplayTags::InputTag_Ability_Smash.GetTag()
			|| InputTag == DSTRGameplayTags::InputTag_Ability_Reckoning.GetTag();
	}

	struct FDSTRStartupAbilityDefinition
	{
		TSubclassOf<UGameplayAbility> AbilityClass;
		FGameplayTag InputTag;
	};

	const TArray<FDSTRStartupAbilityDefinition>& GetStartupAbilityDefinitions()
	{
		static const TArray<FDSTRStartupAbilityDefinition> Definitions =
		{
			{ UDSTRBasicAttackAbility::StaticClass(), DSTRGameplayTags::InputTag_Ability_BasicAttack.GetTag() },
			{ UDSTRFortifyAbility::StaticClass(), DSTRGameplayTags::InputTag_Ability_Fortify.GetTag() },
			{ UDSTRChargeAbility::StaticClass(), DSTRGameplayTags::InputTag_Ability_Charge.GetTag() },
			{ UDSTRSmashAbility::StaticClass(), DSTRGameplayTags::InputTag_Ability_Smash.GetTag() },
			{ UDSTRReckoningAbility::StaticClass(), DSTRGameplayTags::InputTag_Ability_Reckoning.GetTag() },
			{ UDSTRReviveAbility::StaticClass(), DSTRGameplayTags::InputTag_Ability_Revive.GetTag() },
			{ UDSTRSprintAbility::StaticClass(), DSTRGameplayTags::InputTag_Ability_Sprint.GetTag() },
			{ UDSTRPulledAbility::StaticClass(), FGameplayTag() }
		};
		return Definitions;
	}
}

UDSTRAbilitySystemComponent::UDSTRAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
}

// 초기화 중복 호출돼도 플래그로 1회만 부여
void UDSTRAbilitySystemComponent::GiveStartupAbilities()
{
	if (bStartupAbilitiesGiven || !IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FDSTRStartupAbilityDefinition& Definition : GetStartupAbilityDefinitions())
	{
		FGameplayAbilitySpec AbilitySpec(Definition.AbilityClass, 1);
		AbilitySpec.DynamicAbilityTags.AddTag(Definition.InputTag);
		GiveAbility(AbilitySpec);
	}

	bStartupAbilitiesGiven = true;
}

void UDSTRAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag))
		{
			ClearBufferedInput();
			AbilitySpecInputPressed(AbilitySpec);
			if (!AbilitySpec.IsActive() && TryActivateAbility(AbilitySpec.Handle))
			{
				return;
			}
			// 후딜 직전의 마지막 입력 하나만 보관한다. 달리기/상호작용은 예약하지 않는다.
			UWorld* World = GetWorld();
			const bool bRecovering = HasMatchingGameplayTag(DSTRGameplayTags::State_Attacking.GetTag())
				|| (InputTag == DSTRGameplayTags::InputTag_Ability_BasicAttack.GetTag()
					&& HasMatchingGameplayTag(DSTRGameplayTags::Effect_Cooldown_Attack.GetTag()));
			if (World && bRecovering && IsBufferedCombatInput(InputTag)
				&& !HasMatchingGameplayTag(DSTRGameplayTags::State_Dead.GetTag())
				&& !HasMatchingGameplayTag(DSTRGameplayTags::State_Stunned.GetTag()))
			{
				BufferedAbilityHandle = AbilitySpec.Handle;
				BufferedAvatar = GetAvatarActor();
				BufferedInputExpiresAt = World->GetTimeSeconds() + InputBufferSeconds;
				World->GetTimerManager().SetTimer(
					BufferedInputTimer, this, &UDSTRAbilitySystemComponent::RetryBufferedInput, 0.01f, true);
			}
			return;
		}
	}
}

void UDSTRAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag))
		{
			AbilitySpecInputReleased(AbilitySpec);
		}
	}
}

void UDSTRAbilitySystemComponent::ClearBufferedInput()
{
	BufferedAbilityHandle = FGameplayAbilitySpecHandle();
	BufferedAvatar.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BufferedInputTimer);
	}
}

void UDSTRAbilitySystemComponent::ClearAbilityInput()
{
	ClearBufferedInput();
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		AbilitySpecInputReleased(AbilitySpec);
	}
}

void UDSTRAbilitySystemComponent::RetryBufferedInput()
{
	const UWorld* World = GetWorld();
	if (!World || World->GetTimeSeconds() >= BufferedInputExpiresAt
		|| !BufferedAvatar.IsValid() || BufferedAvatar.Get() != GetAvatarActor()
		|| HasMatchingGameplayTag(DSTRGameplayTags::State_Dead.GetTag())
		|| HasMatchingGameplayTag(DSTRGameplayTags::State_Stunned.GetTag()))
	{
		ClearBufferedInput();
		return;
	}

	ABILITYLIST_SCOPE_LOCK();
	const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(BufferedAbilityHandle);
	if (!Spec)
	{
		ClearBufferedInput();
	}
	else if (!Spec->IsActive()
		&& !HasMatchingGameplayTag(DSTRGameplayTags::State_Attacking.GetTag())
		&& TryActivateAbility(BufferedAbilityHandle))
	{
		ClearBufferedInput();
	}
}

bool UDSTRAbilitySystemComponent::TryMarkStartupEffectsApplied()
{
	if (bStartupEffectsApplied)
	{
		return false;
	}
	bStartupEffectsApplied = true;
	return true;
}
