#include "AbilitySystem/DSTRAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Combat/DSTRCombatantInterface.h"
#include "Net/UnrealNetwork.h"
#include "Player/DSTRHeroMovementTuning.h"

UDSTRAttributeSet::UDSTRAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitStamina(100.0f);
	InitMaxStamina(100.0f);
	InitAttackPower(10.0f);
	InitMoveSpeed(FDSTRHeroMovementTuning::MaxWalkSpeed);
	InitDamage(0.0f);
}

void UDSTRAttributeSet::PreAttributeChange(
	const FGameplayAttribute& Attribute,
	float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
	else if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
}

void UDSTRAttributeSet::PostGameplayEffectExecute(
	const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		// 피해 메타 속성은 체력에 반영한 뒤 즉시 비운다.
		const float IncomingDamage = GetDamage();
		SetDamage(0.0f);
		if (IncomingDamage > 0.0f)
		{
			SetHealth(FMath::Clamp(GetHealth() - IncomingDamage, 0.0f, GetMaxHealth()));
			HandleHealthDepleted(Data);
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
		HandleHealthDepleted(Data);
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
	}
}

void UDSTRAttributeSet::HandleHealthDepleted(const FGameplayEffectModCallbackData& Data) const
{
	AActor* TargetAvatar = Data.Target.AbilityActorInfo.IsValid()
		? Data.Target.AbilityActorInfo->AvatarActor.Get()
		: nullptr;
	if (GetHealth() <= 0.0f && TargetAvatar && TargetAvatar->HasAuthority())
	{
		if (IDSTRCombatantInterface* Combatant = Cast<IDSTRCombatantInterface>(TargetAvatar))
		{
			if (!Combatant->IsCombatantDead())
			{
				Combatant->HandleOutOfHealth();
			}
		}
	}
}

void UDSTRAttributeSet::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UDSTRAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDSTRAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDSTRAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDSTRAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDSTRAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDSTRAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
}

void UDSTRAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDSTRAttributeSet, Health, OldValue);
}

void UDSTRAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDSTRAttributeSet, MaxHealth, OldValue);
}

void UDSTRAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDSTRAttributeSet, Stamina, OldValue);
}

void UDSTRAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDSTRAttributeSet, MaxStamina, OldValue);
}

void UDSTRAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDSTRAttributeSet, AttackPower, OldValue);
}

void UDSTRAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDSTRAttributeSet, MoveSpeed, OldValue);
}
