#include "Player/DSTRHeroMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/DSTRAttributeSet.h"

float UDSTRHeroMovementComponent::GetMaxSpeed() const
{
	const float Base = Super::GetMaxSpeed();
	if (MovementMode != MOVE_Walking && MovementMode != MOVE_NavWalking)
	{
		return Base;
	}

	const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner());
	const UAbilitySystemComponent* ASC = AbilityOwner ? AbilityOwner->GetAbilitySystemComponent() : nullptr;
	if (!ASC || !ASC->HasAttributeSetForAttribute(UDSTRAttributeSet::GetMoveSpeedAttribute()))
	{
		return Base;
	}
	return FMath::Max(0.0f, ASC->GetNumericAttribute(UDSTRAttributeSet::GetMoveSpeedAttribute()));
}
