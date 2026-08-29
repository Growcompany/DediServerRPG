// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/DSTRCombatantInterface.h"
#include "Game/DSTRFallRecovery.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "Presentation/DSTRCombatFeedback.h"
#include "DediServerRPGCharacter.generated.h"

class UAnimSequenceBase;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UEnhancedInputLocalPlayerSubsystem;
class UGameplayAbility;
struct FGameplayTag;
struct FInputActionValue;

UCLASS(config=Game)
class ADediServerRPGCharacter : public ACharacter,
	public IAbilitySystemInterface,
	public IDSTRCombatantInterface
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* CombatMappingContext;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* BasicAttackAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MakeWayAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* FortifyAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* ChargeAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* ReckoningAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* InteractAction;

public:
	ADediServerRPGCharacter(const FObjectInitializer& ObjectInitializer);
	virtual void Tick(float DeltaSeconds) override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void HandleOutOfHealth() override;
	virtual bool IsCombatantDead() const override { return IsDowned(); }
	virtual EDSTRCombatTeam GetCombatTeam() const override { return EDSTRCombatTeam::Player; }
	virtual void HandleAnimationImpact(const UAnimSequenceBase* Animation) override;

	static bool ShouldHideOwnerMesh(float CameraDistance) { return CameraDistance < 150.0f; }
	static constexpr float DefaultCameraPitch = -12.0f;

	void PressAbilityInput(const FGameplayTag& InputTag);
	void SetDesiredMoveDirection(const FVector& Direction) { DesiredMoveDirection = Direction; }
	bool IsAbilityReady(const FGameplayTag& CooldownTag) const;

	static constexpr float InteractRange = 250.0f;

	bool CanBeRevivedBy(const ADediServerRPGCharacter* Reviver) const;
	bool TryReviveNearestFromServer();
	bool TryInteractFromServer();
	bool ReviveFromServer(ADediServerRPGCharacter* Reviver);
	ADediServerRPGCharacter* FindNearestReviveTarget() const;
	class ADSTRAttackBuffPickup* FindNearestPickupTarget() const;
	uint8 StartCombatAction(EDSTRCombatAction Action);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCombatFeedback(
		EDSTRCombatFeedback Feedback,
		FVector_NetQuantize Location,
		FVector_NetQuantizeNormal Direction,
		float Radius);

	void PlayAbilityFeedback(
		EDSTRCombatFeedback Feedback,
		FVector_NetQuantize Location,
		FVector_NetQuantizeNormal Direction,
		float Radius);

	UFUNCTION(BlueprintPure, Category = "DSTR|Combat")
	bool IsDowned() const;

	bool IsEliminated() const;
	float GetBleedOutRemaining() const;
	EDSTRCombatAction GetCurrentCombatAction() const { return CurrentCombatAction; }
	float GetCombatActionProgress() const;

protected:

	void Move(const FInputActionValue& Value);

	void Look(const FInputActionValue& Value);


protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UnPossessed() override;
	virtual void PawnClientRestart() override;

public:
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:
	void RemoveInputMappings();
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;

	void InitializeAbilitySystem();
	void HandleBasicAttackInput();
	void HandleMakeWayInput();
	void HandleFortifyInput();
	void HandleChargeInput();
	void HandleReckoningInput();
	void HandleInteractInput();
	void ClearWarningEffect();
	TWeakObjectPtr<class UParticleSystemComponent> WarningEffectComponent;
	FTimerHandle WarningEffectTimerHandle;
	void ClearBuffEffect();
	TWeakObjectPtr<class UParticleSystemComponent> BuffEffectComponent;
	FTimerHandle BuffEffectTimerHandle;
	void ApplyDownedPresentation();
	void PlayCombatActionLocal(EDSTRCombatAction Action, uint8 Variant);
	void FinishCombatAction();
	uint8 NextAttackVariant = 0;
	uint8 CurrentCombatVariant = 0;
	void ClearReplicatedCombatAction();

	UPROPERTY(ReplicatedUsing = OnRep_CombatAction)
	FDSTRReplicatedCombatAction ReplicatedCombatAction;

	UFUNCTION()
	void OnRep_CombatAction();

	FVector DesiredMoveDirection = FVector::ZeroVector;
	FDSTRFallRecoveryRing FallRecoveryRing;
	FVector LastGroundedLocation = FVector::ZeroVector;
	double FallStartTime = -1.0;
	void TickFallRecovery();
	EDSTRCombatAction CurrentCombatAction;
	double CombatActionStartTime = -1.0;
	FTimerHandle CombatActionTimerHandle;
	FTimerHandle ReplicatedCombatActionTimerHandle;
	void TickBossCamera(float DeltaSeconds);
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
	FDelegateHandle DeadTagChangedHandle;
	TWeakObjectPtr<class UAbilitySystemComponent> BoundAbilitySystemComponent;
};
