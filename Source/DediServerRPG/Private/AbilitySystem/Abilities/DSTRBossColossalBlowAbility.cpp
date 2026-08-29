#include "AbilitySystem/Abilities/DSTRBossColossalBlowAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DSTRGameplayEffects.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Enemy/DSTRBossSkillRules.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "TimerManager.h"

UDSTRBossColossalBlowAbility::UDSTRBossColossalBlowAbility()
{
	AbilityTags.AddTag(DSTRGameplayTags::Ability_Boss_ColossalBlow.GetTag());
	CooldownGameplayEffectClass = UDSTRBossColossalCooldownEffect::StaticClass();
}

void UDSTRBossColossalBlowAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ADSTREnemyCharacter* Boss = ActorInfo ? Cast<ADSTREnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !Boss
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	Boss->NotifyColossalBlow();
	Boss->StartCombatAction(EDSTRCombatAction::BossColossal);
	Boss->SetPreparingMelee(true);
	Boss->GetWorldTimerManager().SetTimer(
		BlowTimerHandle, this, &UDSTRBossColossalBlowAbility::ExecuteBlow,
		FDSTRCombatPresentation::GetImpactDelay(EDSTRCombatAction::BossColossal, 0), false);
	Boss->GetWorldTimerManager().SetTimer(
		RecoveryTimerHandle, this, &UDSTRBossColossalBlowAbility::FinishRecovery,
		FDSTRCombatPresentation::GetProfile(EDSTRCombatAction::BossColossal).RecoveryDuration, false);
}

void UDSTRBossColossalBlowAbility::ExecuteBlow()
{
	ADSTREnemyCharacter* Boss = GetBoss();
	if (!Boss)
	{
		return;
	}
	Boss->SetPreparingMelee(false);
	if (Boss->IsCombatantDead())
	{
		return;
	}
	DamagePlayersInRange(
		Boss->GetActorLocation(),
		FDSTRBossSkillRules::ColossalReach,
		FDSTRBossSkillRules::ColossalConeHalfAngleDegrees,
		FDSTRBossSkillRules::ColossalDamageMultiplier,
		true,
		TEXT("Colossal"),
		FDSTRBossSkillRules::ColossalKnockbackSpeed);
}

void UDSTRBossColossalBlowAbility::FinishRecovery()
{
	FinishAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
}

void UDSTRBossColossalBlowAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (ADSTREnemyCharacter* Boss = ActorInfo ? Cast<ADSTREnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
	{
		Boss->SetPreparingMelee(false);
		Boss->GetWorldTimerManager().ClearTimer(BlowTimerHandle);
		Boss->GetWorldTimerManager().ClearTimer(RecoveryTimerHandle);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
