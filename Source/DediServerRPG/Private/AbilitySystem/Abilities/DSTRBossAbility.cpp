#include "AbilitySystem/Abilities/DSTRBossAbility.h"

#include "AbilitySystem/DSTRGameplayTags.h"
#include "Enemy/DSTREnemyCharacter.h"

UDSTRBossAbility::UDSTRBossAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	ActivationOwnedTags.AddTag(DSTRGameplayTags::State_Attacking.GetTag());
}

ADSTREnemyCharacter* UDSTRBossAbility::GetBoss() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	return ActorInfo ? Cast<ADSTREnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
}
