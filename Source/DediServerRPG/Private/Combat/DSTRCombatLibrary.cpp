#include "Combat/DSTRCombatLibrary.h"
#include "DSTRLog.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DSTRAttributeSet.h"
#include "AbilitySystem/DSTRGameplayEffects.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Combat/DSTRDamageRules.h"
#include "Enemy/DSTREnemyCharacter.h"

float UDSTRCombatLibrary::CalculateDamage(float AttackPower, float AbilityMultiplier)
{
	return FMath::Max(0.0f, AttackPower) * FMath::Max(0.0f, AbilityMultiplier);
}

bool UDSTRCombatLibrary::IsReviveRequestValid(
	const bool bSameActor,
	const bool bReviverDead,
	const bool bTargetDead,
	const bool bTargetEliminated,
	const float Distance)
{
	return !bSameActor
		&& !bReviverDead
		&& bTargetDead
		&& !bTargetEliminated
		&& Distance <= FDSTRDamageRules::InteractReach;
}

bool UDSTRCombatLibrary::CanConsumePickup(
	const bool bAlreadyConsumed,
	const bool bPlayerDead,
	const float Distance)
{
	return !bAlreadyConsumed && !bPlayerDead && Distance <= FDSTRDamageRules::InteractReach;
}

bool UDSTRCombatLibrary::CanApplyDamage(const UAbilitySystemComponent* TargetASC)
{
	return TargetASC
		&& !TargetASC->HasMatchingGameplayTag(DSTRGameplayTags::State_Invulnerable.GetTag())
		&& !TargetASC->HasMatchingGameplayTag(DSTRGameplayTags::State_Dead.GetTag());
}

float UDSTRCombatLibrary::GetOutgoingDamage(
	const UAbilitySystemComponent* SourceASC,
	const float AbilityMultiplier)
{
	if (!SourceASC)
	{
		return 0.0f;
	}
	return CalculateDamage(
		SourceASC->GetNumericAttribute(UDSTRAttributeSet::GetAttackPowerAttribute()), AbilityMultiplier);
}

bool UDSTRCombatLibrary::ApplyStagger(
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC,
	const float Seconds)
{
	const AActor* Avatar = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
	if (!Avatar || !Avatar->HasAuthority() || Seconds <= 0.0f)
	{
		return false;
	}
	UAbilitySystemComponent* Applier = SourceASC ? SourceASC : TargetASC;
	FGameplayEffectSpecHandle SpecHandle = Applier->MakeOutgoingSpec(
		UDSTRStaggerEffect::StaticClass(), 1.0f, Applier->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return false;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(DSTRGameplayTags::Effect_Duration.GetTag(), Seconds);
	Applier->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_STAGGER Target=%s Seconds=%.1f"), *GetNameSafe(Avatar), Seconds);
	return true;
}

bool UDSTRCombatLibrary::ApplySlow(
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC)
{
	const AActor* Avatar = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
	if (!Avatar || !Avatar->HasAuthority())
	{
		return false;
	}
	UAbilitySystemComponent* Applier = SourceASC ? SourceASC : TargetASC;
	FGameplayEffectSpecHandle SpecHandle = Applier->MakeOutgoingSpec(
		UDSTRSlowEffect::StaticClass(), 1.0f, Applier->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return false;
	}
	Applier->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_SLOW Target=%s Seconds=%.1f Speed=%.0f"),
		*GetNameSafe(Avatar),
		FDSTRDamageRules::SlowDurationSeconds,
		TargetASC->GetNumericAttribute(UDSTRAttributeSet::GetMoveSpeedAttribute()));
	return true;
}

bool UDSTRCombatLibrary::ApplyDamage(
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC,
	const float AbilityMultiplier)
{
	if (!SourceASC || !CanApplyDamage(TargetASC))
	{
		return false;
	}

	// 피해 확정은 소스 액터의 서버 권한에서만 수행한다.
	const AActor* SourceAvatar = SourceASC->GetAvatarActor();
	if (!SourceAvatar || !SourceAvatar->HasAuthority())
	{
		return false;
	}

	// 잠든 보스는 관문 진입 전 피해를 받지 않는다.
	const ADSTREnemyCharacter* TargetEnemy = Cast<ADSTREnemyCharacter>(TargetASC->GetAvatarActor());
	if (TargetEnemy
		&& FDSTRDamageRules::IsImmuneWhileDormant(TargetEnemy->IsDormant(), TargetEnemy->IsBoss()))
	{
		UE_LOG(LogDSTR, Log, TEXT("DSTR_DAMAGE_REJECTED Source=%s Target=%s Reason=Dormant"),
			*GetNameSafe(SourceAvatar), *GetNameSafe(TargetEnemy));
		return false;
	}

	if (GetOutgoingDamage(SourceASC, AbilityMultiplier) <= 0.0f)
	{
		return false;
	}

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		UDSTRDamageEffect::StaticClass(),
		1.0f,
		SourceASC->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(
		DSTRGameplayTags::Effect_Damage.GetTag(),
		AbilityMultiplier);
	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return true;
}
