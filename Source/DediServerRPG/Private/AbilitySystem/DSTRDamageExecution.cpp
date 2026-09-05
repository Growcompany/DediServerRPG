#include "AbilitySystem/DSTRDamageExecution.h"
#include "DSTRLog.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DSTRAttributeSet.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Combat/DSTRCombatLibrary.h"
#include "Combat/DSTRDamageRules.h"

namespace
{
	struct FDSTRDamageCapture
	{
		FGameplayEffectAttributeCaptureDefinition AttackPowerDef;

		FDSTRDamageCapture()
		{
			AttackPowerDef = FGameplayEffectAttributeCaptureDefinition(
				UDSTRAttributeSet::GetAttackPowerAttribute(),
				EGameplayEffectAttributeCaptureSource::Source,
				true);
		}
	};

	const FDSTRDamageCapture& DamageCapture()
	{
		static FDSTRDamageCapture Capture;
		return Capture;
	}
}

UDSTRDamageExecution::UDSTRDamageExecution(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RelevantAttributesToCapture.Add(DamageCapture().AttackPowerDef);
}

// 피해 계산은 여기서 종료. 출력은 메타 Damage 하나뿐
void UDSTRDamageExecution::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluationParameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float AttackPower = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		DamageCapture().AttackPowerDef, EvaluationParameters, AttackPower);

	const float AbilityMultiplier = Spec.GetSetByCallerMagnitude(
		DSTRGameplayTags::Effect_Damage.GetTag(), false, 0.0f);
	const float Outgoing = UDSTRCombatLibrary::CalculateDamage(AttackPower, AbilityMultiplier);
	if (Outgoing <= 0.0f)
	{
		return;
	}

	const bool bFortified = EvaluationParameters.TargetTags
		&& EvaluationParameters.TargetTags->HasTag(DSTRGameplayTags::State_Fortified.GetTag());
	const float Mitigation = FDSTRDamageRules::FortifiedMultiplier(bFortified);
	const float Damage = Outgoing * Mitigation;

	const UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	const UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
	const AActor* TargetAvatar = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
	const AActor* SourceAvatar = SourceASC ? SourceASC->GetAvatarActor() : nullptr;

	if (Mitigation < 1.0f)
	{
		UE_LOG(LogDSTR, Log, TEXT("DSTR_FORTIFY_REDUCED Target=%s From=%.0f To=%.0f"),
			*GetNameSafe(TargetAvatar), Outgoing, Damage);
	}

	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		UDSTRAttributeSet::GetDamageAttribute(), EGameplayModOp::Additive, Damage));

	UE_LOG(LogDSTR, Log,
		TEXT("DSTR_DAMAGE Source=%s Target=%s Amount=%.2f"),
		*GetNameSafe(SourceAvatar),
		*GetNameSafe(TargetAvatar),
		Damage);
}
