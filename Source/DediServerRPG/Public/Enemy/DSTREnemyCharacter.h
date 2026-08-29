#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/DSTRCombatantInterface.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "Presentation/DSTRCombatFeedback.h"
#include "DSTREnemyCharacter.generated.h"

class ADediServerRPGCharacter;
class UAnimSequenceBase;
class UDSTRAbilitySystemComponent;
class UDSTRAttributeSet;

DECLARE_MULTICAST_DELEGATE_OneParam(FDSTREnemyDefeated, class ADSTREnemyCharacter*);
DECLARE_MULTICAST_DELEGATE_OneParam(FDSTREnemyAwakened, class ADSTREnemyCharacter*);

UCLASS()
class DEDISERVERRPG_API ADSTREnemyCharacter : public ACharacter,
	public IAbilitySystemInterface,
	public IDSTRCombatantInterface
{
	GENERATED_BODY()

public:
	ADSTREnemyCharacter();
	virtual void Tick(float DeltaSeconds) override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UDSTRAbilitySystemComponent* GetDSTRAbilitySystemComponent() const;
	const UDSTRAttributeSet* GetAttributeSet() const;

	virtual void HandleOutOfHealth() override;
	virtual bool IsCombatantDead() const override { return bIsDead; }
	virtual EDSTRCombatTeam GetCombatTeam() const override { return EDSTRCombatTeam::Enemy; }
	virtual void HandleAnimationImpact(const UAnimSequenceBase* Animation) override;

	void ConfigureAsBoss(bool bInBoss);
	bool IsBoss() const { return bIsBoss; }
	void SetDormant(bool bNewDormant);
	bool IsDormant() const { return bDormant; }
	void Wake(const TCHAR* Reason);
	bool IsPreparingAreaAttack() const;
	void SetPreparingAreaAttack(bool bPreparing, const FVector& Center = FVector::ZeroVector);
	void SetPreparingMelee(bool bPreparing);
	bool IsPreparingMelee() const;
	void ApplyStun(const ADediServerRPGCharacter* Source);
	bool IsStunned() const;
	void ApplyThrown(const ADediServerRPGCharacter* Source, float Seconds);
	void SetRushing(bool bNewRushing);
	bool IsRushing() const;
	void SetSiphoning(bool bNewSiphoning, const FVector& TelegraphLocation = FVector::ZeroVector);
	bool IsSiphoning() const;
	int32 GetSwingsSinceColossal() const { return SwingsSinceColossal; }
	int32 GetSwingsSinceSubjugate() const { return SwingsSinceSubjugate; }
	void NotifyBossSwing() { ++SwingsSinceColossal; ++SwingsSinceSubjugate; }
	void NotifyColossalBlow() { SwingsSinceColossal = 0; ++SwingsSinceSubjugate; }
	void NotifySubjugate() { SwingsSinceSubjugate = 0; }
	void BeginSiphonPull(ADediServerRPGCharacter* Target, float Seconds);
	void EndSiphonPull();
	void AddThreat(const ADediServerRPGCharacter* Source, float Amount);
	float GetThreat(const ADediServerRPGCharacter* Source) const;
	static bool IsDelayedAttackAllowed(bool bDead, bool bBossPhase)
	{
		return !bDead && bBossPhase;
	}
	static float GetConfiguredMaxHealth(bool bBoss, int32 PlayerCount);
	static float GetConfiguredAttackPower(bool bBoss);
	static float GetActorScaleForRole(bool bBoss) { return bBoss ? 1.2f : 1.0f; }
	static float GetSpawnGroundClearance(bool bBoss);
	float GetAttackRange() const
	{
		return FDSTRCombatPresentation::GetEngageRange(
			bIsBoss ? EDSTRCombatAction::BossMelee : EDSTRCombatAction::EnemyMelee);
	}
	ADediServerRPGCharacter* FindNearestLivingPlayer(float MaxRange = TNumericLimits<float>::Max()) const;
	uint8 StartCombatAction(EDSTRCombatAction Action, int32 ExplicitVariant = INDEX_NONE);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastEnemyFeedback(
		FVector_NetQuantize Location,
		float Radius,
		EDSTRCombatFeedback Feedback,
		AActor* InstigatorActor);

	void PlayAbilityFeedback(
		FVector_NetQuantize Location,
		float Radius,
		EDSTRCombatFeedback Feedback,
		AActor* InstigatorActor);

	FDSTREnemyDefeated OnEnemyDefeated;
	FDSTREnemyAwakened OnEnemyAwakened;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void InitializeAbilitySystem();
	void ApplyConfiguredStats();
	void ApplyVisualAssets();
	void PlaySpawnEffect();
	void PlayCombatActionLocal(EDSTRCombatAction Action, uint8 Variant);
	void FinishCombatAction();
	void ClearReplicatedCombatAction();
	uint8 NextAttackVariant = 0;
	uint8 CurrentCombatVariant = 0;

	UPROPERTY(VisibleAnywhere, Category = "Abilities")
	TObjectPtr<UDSTRAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UDSTRAttributeSet> AttributeSet;

	UPROPERTY(ReplicatedUsing = OnRep_Boss)
	bool bIsBoss = false;

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bIsDead = false;

	UPROPERTY(ReplicatedUsing = OnRep_Dormant)
	bool bDormant = false;

	int32 SwingsSinceColossal = 0;
	int32 SwingsSinceSubjugate = 0;
	TWeakObjectPtr<ADediServerRPGCharacter> SiphonTarget;
	FTimerHandle StunSegmentTimerHandle;
	void PlayStunLoop();
	void PlayStunEnd();

	UFUNCTION()
	void OnRep_Boss();

	UFUNCTION()
	void OnRep_Dead();

	void SetWarningCue(const FGameplayTag& CueTag, bool bActive, const FVector& Location, float Radius);
	bool HasWarningCue(const FGameplayTag& CueTag) const;

	UFUNCTION()
	void OnRep_Dormant();

	TWeakObjectPtr<class UParticleSystemComponent> SpawnEffectComponent;
	FTimerHandle SpawnEffectTimerHandle;
	bool bSpawnEffectPending = false;
	void StopSpawnEffect();
	void PlayWakeEffect();

	void ApplyDeadCollision();

	struct FDSTRThreatEntry
	{
		float Threat = 0.0f;
		double UpdatedTime = 0.0;
	};
	TMap<TWeakObjectPtr<const ADediServerRPGCharacter>, FDSTRThreatEntry> ThreatTable;

	void BeginCorpse();
	void TickCorpseSink();
	double CorpseDeathTime = -1.0;
	float CorpseMeshRestZ = 0.0f;

	void PlayEnemyFeedbackLocal(
		FVector Location,
		float Radius,
		EDSTRCombatFeedback Feedback,
		const AActor* InstigatorActor);

	UPROPERTY(ReplicatedUsing = OnRep_CombatAction)
	FDSTRReplicatedCombatAction ReplicatedCombatAction;

	UFUNCTION()
	void OnRep_CombatAction();

	EDSTRCombatAction CurrentCombatAction;
	double CombatActionStartTime = -1.0;
	FTimerHandle CombatActionTimerHandle;
	FTimerHandle ReplicatedCombatActionTimerHandle;
};
