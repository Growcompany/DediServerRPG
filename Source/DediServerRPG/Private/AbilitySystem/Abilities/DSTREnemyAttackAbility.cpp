#include "AbilitySystem/Abilities/DSTREnemyAttackAbility.h"

#include "AbilitySystem/DSTRGameplayEffects.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "TimerManager.h"
#include "Presentation/DSTRCombatPresentation.h"

UDSTREnemyAttackAbility::UDSTREnemyAttackAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	ActivationOwnedTags.AddTag(DSTRGameplayTags::State_Attacking.GetTag());
	CooldownGameplayEffectClass = UDSTREnemyAttackCooldownEffect::StaticClass();
}

void UDSTREnemyAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->IsNetAuthority()
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	ADSTREnemyCharacter* Enemy = Cast<ADSTREnemyCharacter>(ActorInfo->AvatarActor.Get());
	if (!Enemy)
	{
		FinishAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	const EDSTRCombatAction Action = Enemy->IsBoss()
		? EDSTRCombatAction::BossMelee
		: EDSTRCombatAction::EnemyMelee;
	ActiveVariant = Enemy->StartCombatAction(Action);
	if (Enemy->IsBoss())
	{
		Enemy->SetPreparingMelee(true);
		Enemy->NotifyBossSwing();
	}
	Enemy->GetWorldTimerManager().SetTimer(
		MeleeTimerHandle,
		this,
		&UDSTREnemyAttackAbility::ExecuteMelee,
		FDSTRCombatPresentation::GetImpactDelay(Action, ActiveVariant),
		false);
	Enemy->GetWorldTimerManager().SetTimer(
		RecoveryTimerHandle,
		this,
		&UDSTREnemyAttackAbility::FinishRecovery,
		FDSTRCombatPresentation::GetProfile(Action).RecoveryDuration,
		false);
}

void UDSTREnemyAttackAbility::ExecuteMelee()
{
	ADSTREnemyCharacter* Source = GetCurrentActorInfo()
		? Cast<ADSTREnemyCharacter>(GetCurrentActorInfo()->AvatarActor.Get())
		: nullptr;
	if (Source)
	{
		Source->SetPreparingMelee(false);
	}
	if (Source && !Source->IsCombatantDead())
	{
		const EDSTRCombatAction Action = Source->IsBoss()
			? EDSTRCombatAction::BossMelee
			: EDSTRCombatAction::EnemyMelee;
		DamagePlayersInRange(
			Source->GetActorLocation(),
			FDSTRCombatPresentation::GetProfile(Action).HitReach,
			FDSTRCombatPresentation::GetHitConeHalfAngle(Action),
			1.0f,
			true,
			TEXT("Swing"));
	}
}

void UDSTREnemyAttackAbility::FinishRecovery()
{
	FinishAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
}

void UDSTREnemyAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (ActorInfo)
	{
		if (ADSTREnemyCharacter* Enemy = Cast<ADSTREnemyCharacter>(ActorInfo->AvatarActor.Get()))
		{
			Enemy->SetPreparingMelee(false);
			Enemy->GetWorldTimerManager().ClearTimer(MeleeTimerHandle);
			Enemy->GetWorldTimerManager().ClearTimer(RecoveryTimerHandle);
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
