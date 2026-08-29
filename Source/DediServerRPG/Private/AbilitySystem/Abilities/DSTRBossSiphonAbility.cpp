#include "AbilitySystem/Abilities/DSTRBossSiphonAbility.h"
#include "DSTRLog.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DSTRGameplayEffects.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Combat/DSTRCombatLibrary.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Enemy/DSTRBossSkillRules.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "TimerManager.h"

UDSTRBossSiphonAbility::UDSTRBossSiphonAbility()
{
	AbilityTags.AddTag(DSTRGameplayTags::Ability_Boss_Siphon.GetTag());
	CooldownGameplayEffectClass = UDSTRBossSiphonCooldownEffect::StaticClass();
}

void UDSTRBossSiphonAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ADSTREnemyCharacter* Boss = ActorInfo ? Cast<ADSTREnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	ADediServerRPGCharacter* Target = Boss
		? Boss->FindNearestLivingPlayer(FDSTRBossSkillRules::SiphonMaxDistance) : nullptr;
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !Boss || !Target
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	Victim = Target;
	Boss->SetSiphoning(true, Target->GetActorLocation());
	Boss->StartCombatAction(EDSTRCombatAction::BossSiphonTargeting);
	Boss->GetWorldTimerManager().SetTimer(
		PullTimerHandle, this, &UDSTRBossSiphonAbility::ExecutePull,
		FDSTRBossSkillRules::SiphonWarningSeconds, false);
	Boss->GetWorldTimerManager().SetTimer(
		RecoveryTimerHandle, this, &UDSTRBossSiphonAbility::FinishRecovery,
		FDSTRBossSkillRules::SiphonWarningSeconds
			+ FDSTRCombatPresentation::GetProfile(EDSTRCombatAction::BossSiphon).RecoveryDuration,
		false);
}

void UDSTRBossSiphonAbility::ExecutePull()
{
	ADSTREnemyCharacter* Boss = GetBoss();
	ADediServerRPGCharacter* Target = Victim.Get();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!Boss || Boss->IsCombatantDead() || !Target || Target->IsDowned() || !ActorInfo)
	{
		EndPull();
		return;
	}

	Boss->StartCombatAction(EDSTRCombatAction::BossSiphon);
	const float Distance = static_cast<float>(
		FVector::Dist2D(Target->GetActorLocation(), Boss->GetActorLocation()));
	UE_LOG(LogDSTR, Log, TEXT("DSTR_HIT Source=%s Target=%s Dist=%.0f Reach=%.0f Angle=%.0f Skill=Siphon"),
		*Boss->GetName(), *Target->GetName(), Distance, FDSTRBossSkillRules::SiphonMaxDistance, 0.0f);
	if (UDSTRCombatLibrary::ApplyDamage(
		ActorInfo->AbilitySystemComponent.Get(),
		Target->GetAbilitySystemComponent(),
		FDSTRBossSkillRules::SiphonDamageMultiplier))
	{
		UDSTRCombatLibrary::ApplySlow(
			ActorInfo->AbilitySystemComponent.Get(), Target->GetAbilitySystemComponent());
		if (!Target->IsDowned())
		{
			Target->PlayAbilityFeedback(
				EDSTRCombatFeedback::HitTaken,
				Target->GetActorLocation(),
				(Target->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal(),
				80.0f);
		}
	}
	Boss->BeginSiphonPull(Target, FDSTRBossSkillRules::SiphonPullSeconds);
	Boss->GetWorldTimerManager().SetTimer(
		PullEndTimerHandle, this, &UDSTRBossSiphonAbility::EndPull,
		FDSTRBossSkillRules::SiphonPullSeconds, false);
}

void UDSTRBossSiphonAbility::EndPull()
{
	if (ADSTREnemyCharacter* Boss = GetBoss())
	{
		Boss->EndSiphonPull();
		Boss->SetSiphoning(false);
	}
}

void UDSTRBossSiphonAbility::FinishRecovery()
{
	FinishAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
}

void UDSTRBossSiphonAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (ADSTREnemyCharacter* Boss = ActorInfo ? Cast<ADSTREnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
	{
		Boss->EndSiphonPull();
		Boss->SetSiphoning(false);
		Boss->GetWorldTimerManager().ClearTimer(PullTimerHandle);
		Boss->GetWorldTimerManager().ClearTimer(PullEndTimerHandle);
		Boss->GetWorldTimerManager().ClearTimer(RecoveryTimerHandle);
	}
	Victim.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
