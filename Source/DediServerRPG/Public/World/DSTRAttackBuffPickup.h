#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DSTRAttackBuffPickup.generated.h"

class ADediServerRPGCharacter;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class DEDISERVERRPG_API ADSTRAttackBuffPickup : public AActor
{
	GENERATED_BODY()

public:
	ADSTRAttackBuffPickup();

	bool TryConsume(ADediServerRPGCharacter* Player);
	bool IsConsumed() const { return bConsumed; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyPickupMesh();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(ReplicatedUsing = OnRep_Consumed)
	bool bConsumed = false;

	UFUNCTION()
	void OnRep_Consumed();
};
