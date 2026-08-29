#include "AbilitySystem/Abilities/DSTRBossSubjugateAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DSTRGameplayEffects.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Enemy/DSTRBossSkillRules.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "Engine/World.h"
#include "Game/DSTRGameState.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "TimerManager.h"

UDSTRBossSubjugateAbility::UDSTRBossSubjugateAbility()
{
	AbilityTags.AddTag(DSTRGameplayTags::Ability_Boss_Subjugate.GetTag());
	CooldownGameplayEffectClass = UDSTRBossSubjugateCooldownEffect::StaticClass();
}

void UDSTRBossSubjugateAbility::ActivateAbility(
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

	Boss->NotifySubjugate();
	SlamCenter = Boss->GetActorLocation();
	Boss->StartCombatAction(EDSTRCombatAction::BossSubjugateTargeting);
	Boss->SetPreparingAreaAttack(true, SlamCenter);
	const float SwingDelay = FDSTRBossSkillRules::SubjugateWarningSeconds;
	Boss->GetWorldTimerManager().SetTimer(
		SwingTimerHandle, this, &UDSTRBossSubjugateAbility::BeginSwing, SwingDelay, false);
	Boss->GetWorldTimerManager().SetTimer(
		RecoveryTimerHandle, this, &UDSTRBossSubjugateAbility::FinishRecovery,
		SwingDelay + FDSTRCombatPresentation::GetProfile(EDSTRCombatAction::BossSubjugate).RecoveryDuration,
		false);
}

void UDSTRBossSubjugateAbility::BeginSwing()
{
	ADSTREnemyCharacter* Boss = GetBoss();
	if (!Boss || Boss->IsCombatantDead())
	{
		return;
	}
	Boss->StartCombatAction(EDSTRCombatAction::BossSubjugate);
	Boss->GetWorldTimerManager().SetTimer(
		SlamTimerHandle, this, &UDSTRBossSubjugateAbility::ExecuteSlam,
		FDSTRCombatPresentation::GetImpactDelay(EDSTRCombatAction::BossSubjugate, 0), false);
}

void UDSTRBossSubjugateAbility::ExecuteSlam()
{
	ADSTREnemyCharacter* Boss = GetBoss();
	const ADSTRGameState* State = Boss && Boss->GetWorld()
		? Boss->GetWorld()->GetGameState<ADSTRGameState>() : nullptr;
	const bool bBossPhase = State && State->GetMatchPhase() == EDSTRMatchPhase::Boss;
	if (!Boss || !ADSTREnemyCharacter::IsDelayedAttackAllowed(Boss->IsCombatantDead(), bBossPhase))
	{
		if (Boss)
		{
			Boss->SetPreparingAreaAttack(false);
		}
		FinishAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
		return;
	}

	Boss->SetPreparingAreaAttack(false);
	Boss->PlayAbilityFeedback(SlamCenter, 900.0f, EDSTRCombatFeedback::BossImpact, nullptr);
	Boss->PlayAbilityFeedback(
		SlamCenter, FDSTRBossSkillRules::SubjugateRadius, EDSTRCombatFeedback::BossSubjugate, nullptr);
	DamagePlayersInRange(
		SlamCenter,
		FDSTRBossSkillRules::SubjugateRadius,
		0.0f,
		FDSTRBossSkillRules::SubjugateDamageMultiplier,
		false,
		TEXT("Subjugate"),
		FDSTRBossSkillRules::SubjugateKnockbackSpeed,
		FDSTRBossSkillRules::SubjugateStaggerSeconds);
}

void UDSTRBossSubjugateAbility::FinishRecovery()
{
	FinishAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
}

void UDSTRBossSubjugateAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (ADSTREnemyCharacter* Boss = ActorInfo ? Cast<ADSTREnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
	{
		Boss->SetPreparingAreaAttack(false);
		Boss->GetWorldTimerManager().ClearTimer(SwingTimerHandle);
		Boss->GetWorldTimerManager().ClearTimer(SlamTimerHandle);
		Boss->GetWorldTimerManager().ClearTimer(RecoveryTimerHandle);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
