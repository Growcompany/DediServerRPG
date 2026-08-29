#include "AbilitySystem/Abilities/DSTRBossPhantomRushAbility.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DSTRGameplayEffects.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Enemy/DSTRBossSkillRules.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "GameFramework/RootMotionSource.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "TimerManager.h"

namespace
{
	constexpr float RushSweepInterval = 0.05f;
}

UDSTRBossPhantomRushAbility::UDSTRBossPhantomRushAbility()
{
	AbilityTags.AddTag(DSTRGameplayTags::Ability_Boss_PhantomRush.GetTag());
	CooldownGameplayEffectClass = UDSTRBossRushCooldownEffect::StaticClass();
}

void UDSTRBossPhantomRushAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ADSTREnemyCharacter* Boss = ActorInfo ? Cast<ADSTREnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const ADediServerRPGCharacter* Target = Boss ? Boss->FindNearestLivingPlayer() : nullptr;
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !Boss || !Target
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	AlreadyHit.Reset();
	const FVector Heading = (Target->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
	Boss->SetActorRotation(FRotator(0.0f, Heading.Rotation().Yaw, 0.0f));
	Boss->SetRushing(true);
	Boss->StartCombatAction(EDSTRCombatAction::BossRush);

	if (UAbilityTask_ApplyRootMotionConstantForce* ChargeTask =
		UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			NAME_None,
			Heading,
			FDSTRBossSkillRules::RushDistance / FDSTRBossSkillRules::RushDuration,
			FDSTRBossSkillRules::RushDuration,
			false,
			nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity,
			FVector::ZeroVector,
			0.0f,
			false))
	{
		ChargeTask->ReadyForActivation();
	}

	Boss->GetWorldTimerManager().SetTimer(
		SweepTimerHandle, this, &UDSTRBossPhantomRushAbility::TickSweep, RushSweepInterval, true);
	Boss->GetWorldTimerManager().SetTimer(
		ChargeTimerHandle, this, &UDSTRBossPhantomRushAbility::EndCharge,
		FDSTRBossSkillRules::RushDuration, false);
	Boss->GetWorldTimerManager().SetTimer(
		RecoveryTimerHandle, this, &UDSTRBossPhantomRushAbility::FinishRecovery,
		FDSTRCombatPresentation::GetProfile(EDSTRCombatAction::BossRush).RecoveryDuration, false);
}

void UDSTRBossPhantomRushAbility::TickSweep()
{
	const ADSTREnemyCharacter* Boss = GetBoss();
	if (!Boss || Boss->IsCombatantDead())
	{
		return;
	}
	DamagePlayersInRange(
		Boss->GetActorLocation(),
		FDSTRBossSkillRules::RushSweepRadius,
		0.0f,
		FDSTRBossSkillRules::RushDamageMultiplier,
		true,
		TEXT("Rush"),
		0.0f,
		0.0f,
		&AlreadyHit);
}

void UDSTRBossPhantomRushAbility::EndCharge()
{
	if (ADSTREnemyCharacter* Boss = GetBoss())
	{
		Boss->SetRushing(false);
		Boss->GetWorldTimerManager().ClearTimer(SweepTimerHandle);
	}
}

void UDSTRBossPhantomRushAbility::FinishRecovery()
{
	FinishAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
}

void UDSTRBossPhantomRushAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (ADSTREnemyCharacter* Boss = ActorInfo ? Cast<ADSTREnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
	{
		Boss->SetRushing(false);
		Boss->GetWorldTimerManager().ClearTimer(SweepTimerHandle);
		Boss->GetWorldTimerManager().ClearTimer(ChargeTimerHandle);
		Boss->GetWorldTimerManager().ClearTimer(RecoveryTimerHandle);
	}
	AlreadyHit.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
