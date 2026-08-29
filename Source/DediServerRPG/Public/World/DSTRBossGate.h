#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/DSTRDungeonRules.h"
#include "Game/DSTRGameState.h"
#include "DSTRBossGate.generated.h"

class UBoxComponent;
class UPointLightComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FDSTRGateEnteredSignature, APawn*);

UCLASS()
class DEDISERVERRPG_API ADSTRBossGate : public AActor
{
	GENERATED_BODY()

public:
	ADSTRBossGate();

	void SetGateState(EDSTRGateState NewState);
	EDSTRGateState GetGateState() const { return GateState; }
	bool IsSealed() const { return GateState == EDSTRGateState::Sealed; }
	bool IsLocked() const { return GateState == EDSTRGateState::Locked; }
	FVector GetGateForward() const { return GetActorForwardVector(); }
	FVector GetEntryPoint() const;

	FDSTRGateEnteredSignature OnGateEntered;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> GateRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> Barrier;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> EntryTrigger;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> GateLight;

	UPROPERTY(ReplicatedUsing = OnRep_GateState)
	EDSTRGateState GateState = EDSTRGateState::Sealed;

	UFUNCTION()
	void OnRep_GateState();

	UPROPERTY(ReplicatedUsing = OnRep_BarrierHalfWidth)
	float BarrierHalfWidth = FDSTRDungeonRules::GateFitMinHalfWidth;

	UFUNCTION()
	void OnRep_BarrierHalfWidth();

	void FitToCorridor();
	void ApplyBarrierSize();

	void ApplyCollision();

	bool bEntryConsumed = false;

	UFUNCTION()
	void HandleEntryOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	void ApplyCurtain();
	void StopCurtain();
	void SetCurtainCue(bool bActive);

	bool bCurtainCueActive = false;
};
