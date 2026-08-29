#include "AbilitySystem/Abilities/DSTRGameplayAbility.h"
#include "DSTRLog.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Combat/DSTRCombatLibrary.h"
#include "Combat/DSTRCombatMath.h"
#include "Combat/DSTRDamageRules.h"
#include "Combat/DSTRHitQuery.h"
#include "Components/CapsuleComponent.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Engine/World.h"

UDSTRGameplayAbility::UDSTRGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationBlockedTags.AddTag(DSTRGameplayTags::State_Dead.GetTag());
	ActivationBlockedTags.AddTag(DSTRGameplayTags::State_Stunned.GetTag());
	ActivationBlockedTags.AddTag(DSTRGameplayTags::State_Attacking.GetTag());
}

void UDSTRGameplayAbility::PreActivate(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
	const FGameplayEventData* TriggerEventData)
{
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
	if (!IsActive() || ActivationOwnedTags.IsEmpty())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		RouteActivationOwnedTagsForReplication(*ASC, true);
		OnGameplayAbilityEnded.AddUObject(this, &UDSTRGameplayAbility::HandleActivationEnded);
	}
}

void UDSTRGameplayAbility::RouteActivationOwnedTagsForReplication(
	UAbilitySystemComponent& ASC,
	const bool bAdd) const
{
	const EGameplayAbilityNetExecutionPolicy::Type Policy = GetNetExecutionPolicy();
	const bool bExecutesOnOwningClient =
		Policy == EGameplayAbilityNetExecutionPolicy::LocalPredicted
		|| Policy == EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	if (bExecutesOnOwningClient)
	{
		if (bAdd)
		{
			ASC.AddMinimalReplicationGameplayTags(ActivationOwnedTags);
		}
		else
		{
			ASC.RemoveMinimalReplicationGameplayTags(ActivationOwnedTags);
		}
	}
	else if (bAdd)
	{
		ASC.AddReplicatedLooseGameplayTags(ActivationOwnedTags);
	}
	else
	{
		ASC.RemoveReplicatedLooseGameplayTags(ActivationOwnedTags);
	}
}

void UDSTRGameplayAbility::HandleActivationEnded(UGameplayAbility*)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		RouteActivationOwnedTagsForReplication(*ASC, false);
	}
}

void UDSTRGameplayAbility::FinishAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bWasCancelled)
{
	EndAbility(Handle, ActorInfo, ActivationInfo, true, bWasCancelled);
}

void UDSTRGameplayAbility::DamagePlayersInRange(
	const FVector& Center,
	const float Reach,
	const float ConeHalfAngleDegrees,
	const float Multiplier,
	const bool bReachToSurface,
	const TCHAR* SkillName,
	const float KnockbackSpeed,
	const float StaggerSeconds,
	TArray<TWeakObjectPtr<ADediServerRPGCharacter>>* AlreadyHit)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	AActor* SourceAvatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!SourceAvatar || !SourceAvatar->GetWorld() || !SourceAvatar->HasAuthority())
	{
		return;
	}
	const FVector Forward = SourceAvatar->GetActorForwardVector();

	TArray<ADediServerRPGCharacter*> Candidates;
	FDSTRHitQuery::GatherPlayers(SourceAvatar, Center, Reach, Candidates);
	for (ADediServerRPGCharacter* Target : Candidates)
	{
		if (AlreadyHit && AlreadyHit->Contains(Target))
		{
			continue;
		}
		const FVector ToTarget = Target->GetActorLocation() - Center;
		const float HitDistance = bReachToSurface
			? Reach + Target->GetCapsuleComponent()->GetScaledCapsuleRadius()
			: Reach;
		if (!FDSTRCombatMath::IsHit(Forward, ToTarget, HitDistance, ConeHalfAngleDegrees))
		{
			continue;
		}
		if (AlreadyHit)
		{
			AlreadyHit->Add(Target);
		}
		UE_LOG(LogDSTR, Log, TEXT("DSTR_HIT Source=%s Target=%s Dist=%.0f Reach=%.0f Angle=%.0f Skill=%s"),
			*SourceAvatar->GetName(), *Target->GetName(), ToTarget.Size(), Reach,
			FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
				FVector::DotProduct(Forward.GetSafeNormal2D(), ToTarget.GetSafeNormal2D()), -1.0f, 1.0f))),
			SkillName);

		const bool bDamaged = UDSTRCombatLibrary::ApplyDamage(
			ActorInfo->AbilitySystemComponent.Get(),
			Target->GetAbilitySystemComponent(),
			Multiplier);
		if (bDamaged && !Target->IsDowned())
		{
			if (KnockbackSpeed > 0.0f)
			{
				Target->LaunchCharacter(
					FDSTRDamageRules::KnockbackVelocity(
						Center, Target->GetActorLocation(), Forward, KnockbackSpeed),
					true,
					false);
			}
			if (StaggerSeconds > 0.0f)
			{
				UDSTRCombatLibrary::ApplyStagger(
					ActorInfo->AbilitySystemComponent.Get(),
					Target->GetAbilitySystemComponent(),
					StaggerSeconds);
			}
			Target->PlayAbilityFeedback(
				EDSTRCombatFeedback::HitTaken,
				Target->GetActorLocation(),
				(Target->GetActorLocation() - Center).GetSafeNormal(),
				80.0f);
		}
	}
}
