#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "DSTRPlayerState.generated.h"

class UDSTRAbilitySystemComponent;
class UDSTRAttributeSet;

UCLASS()
class DEDISERVERRPG_API ADSTRPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADSTRPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UDSTRAbilitySystemComponent* GetDSTRAbilitySystemComponent() const;
	const UDSTRAttributeSet* GetAttributeSet() const;

	bool IsPresentationReady() const { return bPresentationReady; }
	void SetPresentationReady(bool bNewPresentationReady);

	void StartBleedOut();
	void ClearBleedOut();
	void MarkEliminated();
	bool IsEliminated() const { return bEliminated; }
	float GetBleedOutEndServerTime() const { return BleedOutEndServerTime; }
	float GetBleedOutRemaining() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Abilities")
	TObjectPtr<UDSTRAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UDSTRAttributeSet> AttributeSet;

	UPROPERTY(Replicated)
	bool bPresentationReady = false;

	UPROPERTY(Replicated)
	float BleedOutEndServerTime = 0.0f;

	UPROPERTY(Replicated)
	bool bEliminated = false;
};
