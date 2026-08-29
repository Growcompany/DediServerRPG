#include "AbilitySystem/DSTRGameplayEffects.h"

#include "AbilitySystem/DSTRAttributeSet.h"
#include "AbilitySystem/DSTRDamageExecution.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Combat/DSTRDamageRules.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

namespace
{
	void ConfigureGrantedTag(
		UTargetTagsGameplayEffectComponent& TargetTagsComponent,
		const FGameplayTag Tag)
	{
		FInheritedTagContainer Tags;
		Tags.AddTag(Tag);
		TargetTagsComponent.SetAndApplyTargetTagChanges(Tags);
	}

	void AddScalableModifier(
		UGameplayEffect& Effect,
		const FGameplayAttribute& Attribute,
		const EGameplayModOp::Type Operation,
		const float Magnitude)
	{
		FGameplayModifierInfo& Modifier = Effect.Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = Operation;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Magnitude));
	}

	void AddSetByCallerModifier(
		UGameplayEffect& Effect,
		const FGameplayAttribute& Attribute,
		const FSetByCallerFloat& SetByCaller)
	{
		FGameplayModifierInfo& Modifier = Effect.Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	}

	void ConfigureCooldown(
		UGameplayEffect& Effect,
		UTargetTagsGameplayEffectComponent& TargetTagsComponent,
		const float Duration,
		const FGameplayTag Tag)
	{
		Effect.DurationPolicy = EGameplayEffectDurationType::HasDuration;
		Effect.DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Duration));
		ConfigureGrantedTag(TargetTagsComponent, Tag);
	}
}

UDSTRDamageEffect::UDSTRDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition& Execution = Executions.AddDefaulted_GetRef();
	Execution.CalculationClass = UDSTRDamageExecution::StaticClass();
}

UDSTREnemyStatsEffect::UDSTREnemyStatsEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat MaxHealthByCaller;
	MaxHealthByCaller.DataTag = DSTRGameplayTags::Effect_Init_MaxHealth.GetTag();
	FSetByCallerFloat AttackPowerByCaller;
	AttackPowerByCaller.DataTag = DSTRGameplayTags::Effect_Init_AttackPower.GetTag();

	AddSetByCallerModifier(*this, UDSTRAttributeSet::GetMaxHealthAttribute(), MaxHealthByCaller);
	AddSetByCallerModifier(*this, UDSTRAttributeSet::GetHealthAttribute(), MaxHealthByCaller);
	AddSetByCallerModifier(*this, UDSTRAttributeSet::GetAttackPowerAttribute(), AttackPowerByCaller);
}

UDSTRDeadEffect::UDSTRDeadEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureGrantedTag(*TargetTags, DSTRGameplayTags::State_Dead.GetTag());
}

UDSTRInvulnerableEffect::UDSTRInvulnerableEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.45f));
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureGrantedTag(*TargetTags, DSTRGameplayTags::State_Invulnerable.GetTag());
}

UDSTRReviveEffect::UDSTRReviveEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddScalableModifier(*this, UDSTRAttributeSet::GetHealthAttribute(), EGameplayModOp::Override, 50.0f);
}

UDSTRReviveProtectionEffect::UDSTRReviveProtectionEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(GetDurationSeconds()));
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureGrantedTag(*TargetTags, DSTRGameplayTags::State_Invulnerable.GetTag());
}

UDSTREncounterRecoveryEffect::UDSTREncounterRecoveryEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddScalableModifier(*this, UDSTRAttributeSet::GetHealthAttribute(), EGameplayModOp::Override, 100.0f);
	AddScalableModifier(*this, UDSTRAttributeSet::GetStaminaAttribute(), EGameplayModOp::Override, 100.0f);
}

UDSTRAttackBuffEffect::UDSTRAttackBuffEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(30.0f));
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	AddScalableModifier(*this, UDSTRAttributeSet::GetAttackPowerAttribute(), EGameplayModOp::Additive, 10.0f);
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureGrantedTag(*TargetTags, DSTRGameplayTags::Effect_Buff_Attack.GetTag());
}

UDSTRFortifyCostEffect::UDSTRFortifyCostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddScalableModifier(*this, UDSTRAttributeSet::GetStaminaAttribute(), EGameplayModOp::Additive, -20.0f);
}

UDSTRMakeWayCostEffect::UDSTRMakeWayCostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddScalableModifier(*this, UDSTRAttributeSet::GetStaminaAttribute(), EGameplayModOp::Additive, -25.0f);
}

UDSTRReckoningCostEffect::UDSTRReckoningCostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddScalableModifier(*this, UDSTRAttributeSet::GetStaminaAttribute(), EGameplayModOp::Additive, -40.0f);
}

UDSTRChargeCostEffect::UDSTRChargeCostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddScalableModifier(*this, UDSTRAttributeSet::GetStaminaAttribute(), EGameplayModOp::Additive, -20.0f);
}

UDSTRFortifyEffect::UDSTRFortifyEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(
		FScalableFloat(FDSTRDamageRules::FortifyDurationSeconds));
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureGrantedTag(*TargetTags, DSTRGameplayTags::State_Fortified.GetTag());
}

UDSTRStunEffect::UDSTRStunEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(
		FScalableFloat(FDSTRDamageRules::ReckoningStunSeconds));
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureGrantedTag(*TargetTags, DSTRGameplayTags::State_Stunned.GetTag());
}

UDSTRStaggerEffect::UDSTRStaggerEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = DSTRGameplayTags::Effect_Duration.GetTag();
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureGrantedTag(*TargetTags, DSTRGameplayTags::State_Stunned.GetTag());
}

UDSTRSlowEffect::UDSTRSlowEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(
		FScalableFloat(FDSTRDamageRules::SlowDurationSeconds));
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	AddScalableModifier(
		*this,
		UDSTRAttributeSet::GetMoveSpeedAttribute(),
		EGameplayModOp::Multiplicitive,
		FDSTRDamageRules::SlowedMoveScale);
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureGrantedTag(*TargetTags, DSTRGameplayTags::State_Slowed.GetTag());
}

UDSTRStaminaRegenEffect::UDSTRStaminaRegenEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(1.0f);
	bExecutePeriodicEffectOnApplication = false;
	AddScalableModifier(*this, UDSTRAttributeSet::GetStaminaAttribute(), EGameplayModOp::Additive, 15.0f);
}

UDSTRAttackCooldownEffect::UDSTRAttackCooldownEffect()
{
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureCooldown(*this, *TargetTags, 0.55f, DSTRGameplayTags::Effect_Cooldown_Attack.GetTag());
}

UDSTRFortifyCooldownEffect::UDSTRFortifyCooldownEffect()
{
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureCooldown(*this, *TargetTags, CooldownSeconds, DSTRGameplayTags::Effect_Cooldown_Fortify.GetTag());
}

UDSTRMakeWayCooldownEffect::UDSTRMakeWayCooldownEffect()
{
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureCooldown(*this, *TargetTags, CooldownSeconds, DSTRGameplayTags::Effect_Cooldown_MakeWay.GetTag());
}

UDSTRReckoningCooldownEffect::UDSTRReckoningCooldownEffect()
{
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureCooldown(*this, *TargetTags, CooldownSeconds, DSTRGameplayTags::Effect_Cooldown_Reckoning.GetTag());
}

UDSTRChargeCooldownEffect::UDSTRChargeCooldownEffect()
{
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureCooldown(*this, *TargetTags, CooldownSeconds, DSTRGameplayTags::Effect_Cooldown_Charge.GetTag());
}

UDSTRBossColossalCooldownEffect::UDSTRBossColossalCooldownEffect()
{
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureCooldown(*this, *TargetTags, CooldownSeconds, DSTRGameplayTags::Effect_Cooldown_BossColossal.GetTag());
}

UDSTRBossRushCooldownEffect::UDSTRBossRushCooldownEffect()
{
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureCooldown(*this, *TargetTags, CooldownSeconds, DSTRGameplayTags::Effect_Cooldown_BossRush.GetTag());
}

UDSTRBossSiphonCooldownEffect::UDSTRBossSiphonCooldownEffect()
{
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureCooldown(*this, *TargetTags, CooldownSeconds, DSTRGameplayTags::Effect_Cooldown_BossSiphon.GetTag());
}

UDSTRBossSubjugateCooldownEffect::UDSTRBossSubjugateCooldownEffect()
{
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureCooldown(*this, *TargetTags, CooldownSeconds, DSTRGameplayTags::Effect_Cooldown_BossSubjugate.GetTag());
}

UDSTREnemyAttackCooldownEffect::UDSTREnemyAttackCooldownEffect()
{
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	ConfigureCooldown(*this, *TargetTags, CooldownSeconds, DSTRGameplayTags::Effect_Cooldown_EnemyAttack.GetTag());
}
