#include "AbilitySystem/Abilities/DSTRBasicAttackAbility.h"
#include "DSTRLog.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DSTRGameplayEffects.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Combat/DSTRCombatantInterface.h"
#include "Combat/DSTRCombatLibrary.h"
#include "Combat/DSTRCombatMath.h"
#include "Combat/DSTRHitQuery.h"
#include "Components/CapsuleComponent.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "Engine/World.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "TimerManager.h"

UDSTRBasicAttackAbility::UDSTRBasicAttackAbility()
{
	AbilityTags.AddTag(DSTRGameplayTags::Ability_Attack_Basic.GetTag());
	ActivationOwnedTags.AddTag(DSTRGameplayTags::State_Attacking.GetTag());
	CooldownGameplayEffectClass = UDSTRAttackCooldownEffect::StaticClass();
}

void UDSTRBasicAttackAbility::ActivateAbility(
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

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!Avatar || !World)
	{
		FinishAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	ActiveVariant = 0;
	if (ADediServerRPGCharacter* Character = Cast<ADediServerRPGCharacter>(Avatar))
	{
		ActiveVariant = Character->StartCombatAction(EDSTRCombatAction::BasicAttack);
	}

	const float ImpactDelay = FDSTRCombatPresentation::GetImpactDelay(
		EDSTRCombatAction::BasicAttack, ActiveVariant);
	World->GetTimerManager().SetTimer(
		RecoveryTimerHandle,
		this,
		&UDSTRBasicAttackAbility::FinishRecovery,
		FDSTRCombatPresentation::GetCancelDelay(EDSTRCombatAction::BasicAttack, ActiveVariant),
		false);
	// 클라는 이 관문 미통과 → 몽타주만. 판정은 서버
	if (FDSTRCombatPresentation::ShouldScheduleAuthorityImpact(Avatar->HasAuthority()))
	{
		World->GetTimerManager().SetTimer(
			ImpactTimerHandle,
			this,
			&UDSTRBasicAttackAbility::ExecuteImpact,
			ImpactDelay,
			false);
	}
}

void UDSTRBasicAttackAbility::ExecuteImpact()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!ActorInfo || !Avatar || !World)
	{
		FinishAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true);
		return;
	}

	const FVector Center = Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * 120.0f;
	FGameplayCueParameters CueParameters;
	CueParameters.Location = Center;
	CueParameters.Normal = Avatar->GetActorForwardVector();
	CueParameters.RawMagnitude = 160.0f;
	ActorInfo->AbilitySystemComponent->ExecuteGameplayCue(
		DSTRGameplayTags::GameplayCue_DSTR_BasicAttack.GetTag(),
		CueParameters);

	if (Avatar->HasAuthority())
	{
		const FVector Origin = Avatar->GetActorLocation();
		const FVector Forward = Avatar->GetActorForwardVector();
		const float ConeHalfAngle = FDSTRCombatPresentation::GetHitConeHalfAngle(EDSTRCombatAction::BasicAttack);
		ADSTREnemyCharacter* Best = nullptr;
		float BestDistance = TNumericLimits<float>::Max();
		TArray<ADSTREnemyCharacter*> Candidates;
		FDSTRHitQuery::GatherEnemies(
			Avatar, Origin,
			FDSTRCombatPresentation::GetProfile(EDSTRCombatAction::BasicAttack).HitReach,
			Candidates);
		for (ADSTREnemyCharacter* Enemy : Candidates)
		{
			const FVector ToTarget = Enemy->GetActorLocation() - Origin;
			const float HitDistance = FDSTRCombatPresentation::GetHitDistance(
				EDSTRCombatAction::BasicAttack, Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius());
			if (FDSTRCombatMath::IsHit(Forward, ToTarget, HitDistance, ConeHalfAngle)
				&& ToTarget.Size() < BestDistance)
			{
				Best = Enemy;
				BestDistance = ToTarget.Size();
			}
		}
		if (Best)
		{
			const FVector ToTarget = Best->GetActorLocation() - Origin;
			UE_LOG(LogDSTR, Log, TEXT("DSTR_HIT Source=%s Target=%s Dist=%.0f Reach=%.0f Angle=%.0f Skill=Basic"),
				*Avatar->GetName(), *Best->GetName(), ToTarget.Size(),
				FDSTRCombatPresentation::GetProfile(EDSTRCombatAction::BasicAttack).HitReach,
				FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
					FVector::DotProduct(Forward.GetSafeNormal2D(), ToTarget.GetSafeNormal2D()), -1.0f, 1.0f))));
			const bool bDamaged = UDSTRCombatLibrary::ApplyDamage(
				ActorInfo->AbilitySystemComponent.Get(),
				Best->GetAbilitySystemComponent(),
				1.0f);
			if (bDamaged)
			{
				Best->AddThreat(
					Cast<ADediServerRPGCharacter>(Avatar),
					UDSTRCombatLibrary::GetOutgoingDamage(ActorInfo->AbilitySystemComponent.Get(), 1.0f));
			}
			if (bDamaged && !Best->IsCombatantDead())
			{
				Best->PlayAbilityFeedback(
					Best->GetActorLocation(), 160.0f, EDSTRCombatFeedback::HitDealt, Avatar);
			}
		}
	}

}

void UDSTRBasicAttackAbility::FinishRecovery()
{
	FinishAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
}

void UDSTRBasicAttackAbility::EndAbility(
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
