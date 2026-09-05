#include "AbilitySystem/Abilities/DSTRSmashAbility.h"
#include "DSTRLog.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DSTRGameplayEffects.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Combat/DSTRCombatLibrary.h"
#include "Combat/DSTRDamageRules.h"
#include "Combat/DSTRHitDebug.h"
#include "Combat/DSTRHitQuery.h"
#include "Components/CapsuleComponent.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "Engine/World.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "TimerManager.h"

UDSTRSmashAbility::UDSTRSmashAbility()
{
	AbilityTags.AddTag(DSTRGameplayTags::Ability_Skill_Smash.GetTag());
	ActivationOwnedTags.AddTag(DSTRGameplayTags::State_Attacking.GetTag());
	CostGameplayEffectClass = UDSTRSmashCostEffect::StaticClass();
	CooldownGameplayEffectClass = UDSTRSmashCooldownEffect::StaticClass();
}

void UDSTRSmashAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	ADediServerRPGCharacter* Character = Cast<ADediServerRPGCharacter>(ActorInfo->AvatarActor.Get());
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World)
	{
		FinishAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	Character->StartCombatAction(EDSTRCombatAction::Smash);

	World->GetTimerManager().SetTimer(
		RecoveryTimerHandle,
		this,
		&UDSTRSmashAbility::FinishRecovery,
		FDSTRCombatPresentation::GetCancelDelay(EDSTRCombatAction::Smash, 0),
		false);
	if (FDSTRCombatPresentation::ShouldScheduleAuthorityImpact(Character->HasAuthority()))
	{
		World->GetTimerManager().SetTimer(
			ImpactTimerHandle,
			this,
			&UDSTRSmashAbility::ExecuteImpact,
			FDSTRCombatPresentation::GetImpactDelay(EDSTRCombatAction::Smash, 0),
			false);
	}
}

void UDSTRSmashAbility::ExecuteImpact()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ADediServerRPGCharacter* Character = ActorInfo
		? Cast<ADediServerRPGCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World || !Character->HasAuthority())
	{
		return;
	}

	const FVector Center = Character->GetActorLocation();
	const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
	Character->PlayAbilityFeedback(
		EDSTRCombatFeedback::Smash, Center, Forward, FDSTRDamageRules::SmashRadius);

	UDSTRHitDebugSubsystem* HitDebug = UDSTRHitDebugSubsystem::IsEnabled()
		? UDSTRHitDebugSubsystem::Get(World) : nullptr;
	if (HitDebug)
	{
		HitDebug->Begin(Character, Center, FDSTRDamageRules::SmashRadius, 0.0f, TEXT("Smash"));
	}
	TArray<ADSTREnemyCharacter*> Candidates;
	FDSTRHitQuery::GatherEnemies(Character, Center, FDSTRDamageRules::SmashRadius, Candidates);
	for (ADSTREnemyCharacter* Enemy : Candidates)
	{
		if (HitDebug)
		{
			HitDebug->SetVerdict(Character, Enemy, EDSTRHitVerdict::Hit);
		}
		const float Distance = static_cast<float>(FVector::Dist2D(Enemy->GetActorLocation(), Center));
		UE_LOG(LogDSTR, Log, TEXT("DSTR_HIT Source=%s Target=%s Dist=%.0f Reach=%.0f Angle=%.0f Skill=Smash"),
			*Character->GetName(), *Enemy->GetName(), Distance,
			FDSTRDamageRules::SmashRadius, 0.0f);
		if (UDSTRCombatLibrary::ApplyDamage(
			ActorInfo->AbilitySystemComponent.Get(),
			Enemy->GetAbilitySystemComponent(),
			FDSTRDamageRules::SmashDamageMultiplier))
		{
			Enemy->AddThreat(Character, UDSTRCombatLibrary::GetOutgoingDamage(
				ActorInfo->AbilitySystemComponent.Get(), FDSTRDamageRules::SmashDamageMultiplier));
			Enemy->LaunchCharacter(
				FDSTRDamageRules::KnockbackVelocity(
					Center, Enemy->GetActorLocation(), Forward,
					FDSTRDamageRules::SmashKnockbackSpeed),
				true,
				false);
			if (!Enemy->IsCombatantDead())
			{
				Enemy->ApplyThrown(Character, FDSTRDamageRules::SmashStaggerSeconds);
				Enemy->PlayAbilityFeedback(
					Enemy->GetActorLocation(), 200.0f, EDSTRCombatFeedback::HitDealt, Character);
			}
		}
	}
	if (HitDebug)
	{
		HitDebug->Commit(Character);
	}
}

void UDSTRSmashAbility::FinishRecovery()
{
	FinishAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
}

void UDSTRSmashAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(ImpactTimerHandle);
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(RecoveryTimerHandle);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
