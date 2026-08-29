#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DSTRGameplayAbility.generated.h"

class ADediServerRPGCharacter;
class UAbilitySystemComponent;

UCLASS(Abstract)
class DEDISERVERRPG_API UDSTRGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDSTRGameplayAbility();

protected:
	virtual void PreActivate(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;

	void FinishAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bWasCancelled = false);

	void DamagePlayersInRange(
		const FVector& Center,
		float Reach,
		float ConeHalfAngleDegrees,
		float Multiplier,
		bool bReachToSurface,
		const TCHAR* SkillName,
		float KnockbackSpeed = 0.0f,
		float StaggerSeconds = 0.0f,
		TArray<TWeakObjectPtr<ADediServerRPGCharacter>>* AlreadyHit = nullptr);

private:
	void RouteActivationOwnedTagsForReplication(UAbilitySystemComponent& ASC, bool bAdd) const;
	void HandleActivationEnded(UGameplayAbility*);
};
