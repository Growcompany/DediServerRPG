#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DSTRCombatLibrary.generated.h"

class UAbilitySystemComponent;

UCLASS()
class DEDISERVERRPG_API UDSTRCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "DSTR|Combat")
	static float CalculateDamage(float AttackPower, float AbilityMultiplier);

	UFUNCTION(BlueprintPure, Category = "DSTR|Combat")
	static bool IsReviveRequestValid(
		bool bSameActor,
		bool bReviverDead,
		bool bTargetDead,
		bool bTargetEliminated,
		float Distance);

	UFUNCTION(BlueprintPure, Category = "DSTR|Combat")
	static bool CanConsumePickup(bool bAlreadyConsumed, bool bPlayerDead, float Distance);

	UFUNCTION(BlueprintPure, Category = "DSTR|Combat")
	static bool CanApplyDamage(const UAbilitySystemComponent* TargetASC);

	UFUNCTION(BlueprintPure, Category = "DSTR|Combat")
	static float GetOutgoingDamage(const UAbilitySystemComponent* SourceASC, float AbilityMultiplier);

	UFUNCTION(BlueprintCallable, Category = "DSTR|Combat")
	static bool ApplyStagger(
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC,
		float Seconds);

	UFUNCTION(BlueprintCallable, Category = "DSTR|Combat")
	static bool ApplySlow(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC);

	UFUNCTION(BlueprintCallable, Category = "DSTR|Combat")
	static bool ApplyDamage(
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC,
		float AbilityMultiplier);
};
