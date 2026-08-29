#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DSTRHeroMovementComponent.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRHeroMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual float GetMaxSpeed() const override;
};
