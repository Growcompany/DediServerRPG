#include "AbilitySystem/DSTRAbilitySystemComponent.h"

#include "AbilitySystem/DSTRGameplayTags.h"
#include "AbilitySystem/Abilities/DSTRBasicAttackAbility.h"
#include "AbilitySystem/Abilities/DSTRFortifyAbility.h"
#include "AbilitySystem/Abilities/DSTRMakeWayAbility.h"
#include "AbilitySystem/Abilities/DSTRChargeAbility.h"
#include "AbilitySystem/Abilities/DSTRPulledAbility.h"
#include "AbilitySystem/Abilities/DSTRReckoningAbility.h"
#include "AbilitySystem/Abilities/DSTRReviveAbility.h"

namespace
{
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
			{ UDSTRMakeWayAbility::StaticClass(), DSTRGameplayTags::InputTag_Ability_MakeWay.GetTag() },
			{ UDSTRFortifyAbility::StaticClass(), DSTRGameplayTags::InputTag_Ability_Fortify.GetTag() },
			{ UDSTRChargeAbility::StaticClass(), DSTRGameplayTags::InputTag_Ability_Charge.GetTag() },
			{ UDSTRReckoningAbility::StaticClass(), DSTRGameplayTags::InputTag_Ability_Reckoning.GetTag() },
			{ UDSTRReviveAbility::StaticClass(), DSTRGameplayTags::InputTag_Ability_Revive.GetTag() },
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
			AbilitySpecInputPressed(AbilitySpec);
			if (!AbilitySpec.IsActive())
			{
				TryActivateAbility(AbilitySpec.Handle);
			}
		}
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
