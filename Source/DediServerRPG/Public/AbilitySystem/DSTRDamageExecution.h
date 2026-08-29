#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "DSTRDamageExecution.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_UCLASS_BODY()

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
