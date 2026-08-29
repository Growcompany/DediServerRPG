#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Looping.h"
#include "GameplayCueNotify_Static.h"
#include "Presentation/DSTRCombatFeedback.h"
#include "DSTRCombatGameplayCues.generated.h"

class UParticleSystemComponent;

UCLASS()
class DEDISERVERRPG_API UDSTRBasicAttackGameplayCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	static constexpr float GetPresentationScale() { return 0.01f; }
	virtual bool OnExecute_Implementation(
		AActor* MyTarget,
		const FGameplayCueParameters& Parameters) const override;
};

UCLASS(Abstract)
class DEDISERVERRPG_API ADSTRLoopingGameplayCue : public AGameplayCueNotify_Looping
{
	GENERATED_BODY()

public:
	ADSTRLoopingGameplayCue();

protected:
	virtual bool WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;

	virtual const FSoftObjectPath* GetEffectPath() const { return nullptr; }
	virtual FVector GetEffectScale(float Radius) const { return FVector(1.0f); }

	EDSTRCombatFeedback Feedback = EDSTRCombatFeedback::EnemyTelegraph;
	bool bPlayFeedback = true;
	bool bSoundOnly = false;
	bool bUseParametersRotation = false;

private:
	void StopEffect();
	TWeakObjectPtr<UParticleSystemComponent> EffectComponent;
};

UCLASS()
class DEDISERVERRPG_API ADSTREnemyTelegraphGameplayCue : public ADSTRLoopingGameplayCue
{
	GENERATED_BODY()

public:
	ADSTREnemyTelegraphGameplayCue();

protected:
	virtual const FSoftObjectPath* GetEffectPath() const override;
	virtual FVector GetEffectScale(float Radius) const override;
};

UCLASS()
class DEDISERVERRPG_API ADSTRBossSiphonGameplayCue : public ADSTRLoopingGameplayCue
{
	GENERATED_BODY()

public:
	ADSTRBossSiphonGameplayCue();

protected:
	virtual const FSoftObjectPath* GetEffectPath() const override;
	virtual FVector GetEffectScale(float Radius) const override;
};

UCLASS()
class DEDISERVERRPG_API ADSTRBossRushGameplayCue : public ADSTRLoopingGameplayCue
{
	GENERATED_BODY()

public:
	ADSTRBossRushGameplayCue();
};

UCLASS()
class DEDISERVERRPG_API ADSTRBossWindupGameplayCue : public ADSTRLoopingGameplayCue
{
	GENERATED_BODY()

public:
	ADSTRBossWindupGameplayCue();
};

UCLASS()
class DEDISERVERRPG_API ADSTRGateSealedGameplayCue : public ADSTRLoopingGameplayCue
{
	GENERATED_BODY()

public:
	ADSTRGateSealedGameplayCue();

protected:
	virtual const FSoftObjectPath* GetEffectPath() const override;
	virtual FVector GetEffectScale(float Radius) const override;
};
