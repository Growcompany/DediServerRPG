#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DSTRGameplayAbility.h"
#include "DSTRBossAbility.generated.h"

class ADediServerRPGCharacter;
class ADSTREnemyCharacter;

UCLASS(Abstract)
class DEDISERVERRPG_API UDSTRBossAbility : public UDSTRGameplayAbility
{
	GENERATED_BODY()

public:
	UDSTRBossAbility();

protected:
	ADSTREnemyCharacter* GetBoss() const;
};
