#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DSTRGameplayEffects.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRDamageEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTREnemyStatsEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTREnemyStatsEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRDeadEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRDeadEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRInvulnerableEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRInvulnerableEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRReviveEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRReviveEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRReviveProtectionEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRReviveProtectionEffect();
	static float GetDurationSeconds() { return 2.5f; }
};

UCLASS()
class DEDISERVERRPG_API UDSTREncounterRecoveryEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTREncounterRecoveryEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRAttackBuffEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRAttackBuffEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRFortifyCostEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRFortifyCostEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRMakeWayCostEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRMakeWayCostEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRReckoningCostEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRReckoningCostEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRChargeCostEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRChargeCostEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRFortifyEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRFortifyEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRStunEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRStunEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRStaggerEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRStaggerEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRSlowEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRSlowEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRStaminaRegenEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRStaminaRegenEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRAttackCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UDSTRAttackCooldownEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRFortifyCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	static constexpr float CooldownSeconds = 12.0f;
	UDSTRFortifyCooldownEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRMakeWayCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	static constexpr float CooldownSeconds = 8.0f;
	UDSTRMakeWayCooldownEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRReckoningCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	static constexpr float CooldownSeconds = 45.0f;
	UDSTRReckoningCooldownEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRChargeCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	static constexpr float CooldownSeconds = 7.0f;
	UDSTRChargeCooldownEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRBossColossalCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	static constexpr float CooldownSeconds = 4.0f;
	UDSTRBossColossalCooldownEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRBossRushCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	static constexpr float CooldownSeconds = 8.0f;
	UDSTRBossRushCooldownEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRBossSiphonCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	static constexpr float CooldownSeconds = 12.0f;
	UDSTRBossSiphonCooldownEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTRBossSubjugateCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	static constexpr float CooldownSeconds = 20.0f;
	UDSTRBossSubjugateCooldownEffect();
};

UCLASS()
class DEDISERVERRPG_API UDSTREnemyAttackCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	static constexpr float CooldownSeconds = 1.6f;

	UDSTREnemyAttackCooldownEffect();
};
