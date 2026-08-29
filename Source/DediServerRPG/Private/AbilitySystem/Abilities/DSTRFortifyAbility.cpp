#include "AbilitySystem/Abilities/DSTRFortifyAbility.h"
#include "DSTRLog.h"

#include "AbilitySystem/DSTRGameplayEffects.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Combat/DSTRDamageRules.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "TimerManager.h"

UDSTRFortifyAbility::UDSTRFortifyAbility()
{
	AbilityTags.AddTag(DSTRGameplayTags::Ability_Skill_Fortify.GetTag());
	ActivationOwnedTags.AddTag(DSTRGameplayTags::State_Attacking.GetTag());
	CostGameplayEffectClass = UDSTRFortifyCostEffect::StaticClass();
	CooldownGameplayEffectClass = UDSTRFortifyCooldownEffect::StaticClass();
}

void UDSTRFortifyAbility::ActivateAbility(
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

	ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, GetDefault<UDSTRFortifyEffect>(), 1.0f);
	Character->StartCombatAction(EDSTRCombatAction::Fortify);

	if (Character->HasAuthority())
	{
		int32 Taunted = 0;
		for (TActorIterator<ADSTREnemyCharacter> It(World); It; ++It)
		{
			ADSTREnemyCharacter* Enemy = *It;
			if (!Enemy || Enemy->IsCombatantDead() || Enemy->IsDormant())
			{
				continue;
			}
			if (FVector::Dist2D(Enemy->GetActorLocation(), Character->GetActorLocation())
				<= FDSTRDamageRules::FortifyTauntRadius)
			{
				Enemy->AddThreat(Character, FDSTRDamageRules::FortifyTauntThreat);
				++Taunted;
			}
		}
		UE_LOG(LogDSTR, Log, TEXT("DSTR_FORTIFY Player=%s Taunted=%d Seconds=%.1f"),
			*Character->GetName(), Taunted, FDSTRDamageRules::FortifyDurationSeconds);
		Character->PlayAbilityFeedback(
			EDSTRCombatFeedback::Fortify,
			Character->GetActorLocation(),
			Character->GetActorForwardVector(),
			FDSTRDamageRules::FortifyTauntRadius);
	}

	World->GetTimerManager().SetTimer(
		RecoveryTimerHandle,
		this,
		&UDSTRFortifyAbility::FinishRecovery,
		FDSTRCombatPresentation::GetCancelDelay(EDSTRCombatAction::Fortify),
		false);
}

void UDSTRFortifyAbility::FinishRecovery()
{
	FinishAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
}

void UDSTRFortifyAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(RecoveryTimerHandle);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
