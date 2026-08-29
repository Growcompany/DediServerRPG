#pragma once

#include "CoreMinimal.h"
#include "Presentation/DSTRVisualAssetRegistry.h"
#include "UObject/SoftObjectPath.h"
#include "DSTRCombatPresentation.generated.h"

class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EDSTRCombatAction : uint8
{
	None,
	BasicAttack,
	Revive,
	Downed,
	Revived,
	EnemyMelee,
	EnemyDeath,
	BossMelee,
	BossDeath,
	HitReact,
	EnemyHitReact,
	BossHitReact,
	Fortify,
	MakeWay,
	Charge,
	ReckoningEntry,
	Reckoning,
	BossColossal,
	BossRush,
	BossSiphonTargeting,
	BossSiphon,
	BossSubjugateTargeting,
	BossSubjugate,
	BossStun,
	BossKnockback
};

UENUM()
enum class EDSTRImpactMetric : uint8
{
	ForwardWeaponPeak,
	HighestWeaponPoint,
	LowestWeaponPoint,
	LowestPelvis,
	WidestHorizontalSweep
};

USTRUCT()
struct FDSTRReplicatedCombatAction
{
	GENERATED_BODY()

	UPROPERTY()
	EDSTRCombatAction Action = EDSTRCombatAction::None;

	UPROPERTY()
	uint16 Sequence = 0;

	UPROPERTY()
	uint8 Variant = 0;
};

UENUM()
enum class EDSTRHitDirection : uint8
{
	Front,
	Back,
	Left,
	Right
};

struct FDSTRCombatActionProfile
{
	TArray<FSoftObjectPath> Variants;
	EDSTRVisualRole RequiredRole = EDSTRVisualRole::Player;
	float ImpactDelay = 0.0f;
	float RecoveryDuration = 0.0f;
	float PlayRate = 1.0f;
	bool bLocksMovement = false;
	bool bHoldLastFrame = false;
	bool bFullBodySlot = false;
	float CancelDelay = 0.0f;
	TArray<float> ImpactTimes;
	float HitReach = 0.0f;
	float HitConeHalfAngleDegrees = 0.0f;
	EDSTRImpactMetric ImpactMetric = EDSTRImpactMetric::ForwardWeaponPeak;
};

class DEDISERVERRPG_API FDSTRCombatPresentation
{
public:
	struct FStunSegments
	{
		float StartSeconds = 0.0f;
		float LoopSeconds = 0.0f;
		float EndSeconds = 0.0f;
	};
	static constexpr float BossStunStartSeconds = 0.60f;
	static constexpr float BossStunEndSeconds = 0.73f;
	static FStunSegments GetStunSegments(float StunSeconds);

	static constexpr float CorpseLifetime = 8.0f;
	static constexpr float BossCorpseLifetime = 12.0f;
	static constexpr float CorpseSinkSeconds = 2.0f;
	static constexpr float CorpseSinkSpeed = 60.0f;
	static constexpr float BossCorpseSinkSpeed = 200.0f;
	static float GetCorpseLifetime(bool bBoss) { return bBoss ? BossCorpseLifetime : CorpseLifetime; }
	static float GetCorpseSinkSpeed(bool bBoss) { return bBoss ? BossCorpseSinkSpeed : CorpseSinkSpeed; }
	static float CorpseSinkOffset(float SecondsSinceDeath, bool bBoss);

	static constexpr float TelegraphParticleRadius = 150.0f;
	static constexpr float TelegraphFloorClearance = 2.0f;
	static float GetTelegraphScale(float DamageRadius)
	{
		return FMath::Max(0.0f, DamageRadius) / TelegraphParticleRadius;
	}
	static FVector GetTelegraphScale3D(float DamageRadius);
	static constexpr float ShockwaveParticleRadius = 455.0f;
	static float GetGroundRingScale(float Radius)
	{
		return FMath::Max(0.0f, Radius) / ShockwaveParticleRadius;
	}
	static FVector GetGroundRingScale3D(float Radius)
	{
		const float Footprint = GetGroundRingScale(Radius);
		return FVector(Footprint, Footprint, 1.0f);
	}
	static constexpr float BossAreaDamageRadius = 500.0f;
	static float GetImpactBurstScale(float DamageRadius)
	{
		return FMath::Clamp(DamageRadius / BossAreaDamageRadius, 0.2f, 0.4f);
	}

	static constexpr float EnemyHitReactMontageSeconds = 0.45f;

	static constexpr float AttackBuffEffectScale = 0.08f;
	static constexpr float AttackBuffEffectSeconds = 0.5f;

	static constexpr float SpawnEffectSeconds = 1.5f;
	static constexpr float WakeShockwaveRadius = 300.0f;
	static bool ShouldPlaySpawnEffect(bool bDormant) { return !bDormant; }

	static constexpr float PlayerCapsuleRadius = 42.0f;
	static constexpr float EngageMargin = 10.0f;

	static const FDSTRCombatActionProfile& GetProfile(EDSTRCombatAction Action);
	static int32 GetVariantCount(EDSTRCombatAction Action) { return GetProfile(Action).Variants.Num(); }
	static float GetImpactMontageTime(EDSTRCombatAction Action, uint8 Variant);
	static float GetImpactDelay(EDSTRCombatAction Action, uint8 Variant);
	static float GetHitDistance(EDSTRCombatAction Action, float TargetCapsuleRadius)
	{
		return GetProfile(Action).HitReach + TargetCapsuleRadius;
	}
	static float GetHitConeHalfAngle(EDSTRCombatAction Action) { return GetProfile(Action).HitConeHalfAngleDegrees; }
	static float GetEngageRange(EDSTRCombatAction Action)
	{
		return GetProfile(Action).HitReach + PlayerCapsuleRadius - EngageMargin;
	}
	static const FSoftObjectPath& ResolveVariant(EDSTRCombatAction Action, uint8 Variant);
	static EDSTRHitDirection ResolveHitDirection(const FVector& ActorForward, const FVector& ToAttacker);
	static TArray<FName> GetAnimClassSlotNames(const UClass* AnimClass);
	static const FName& GetFullBodySlotName();
	static bool ShouldScheduleAuthorityImpact(bool bAvatarHasAuthority)
	{
		return bAvatarHasAuthority;
	}
	static constexpr float MovementLockAfterImpact = 0.10f;
	static float GetCancelDelay(EDSTRCombatAction Action, uint8 Variant = 0)
	{
		const FDSTRCombatActionProfile& Profile = GetProfile(Action);
		const float Base = Profile.CancelDelay > 0.0f ? Profile.CancelDelay : Profile.RecoveryDuration;
		return FMath::Max(Base, GetImpactDelay(Action, Variant) + MovementLockAfterImpact);
	}
	static float GetMovementLockDuration(EDSTRCombatAction Action, uint8 Variant = 0)
	{
		const FDSTRCombatActionProfile& Profile = GetProfile(Action);
		if (!Profile.bLocksMovement)
		{
			return 0.0f;
		}
		return Profile.bHoldLastFrame
			? TNumericLimits<float>::Max()
			: GetImpactDelay(Action, Variant) + MovementLockAfterImpact;
	}
	static bool PlayAction(USkeletalMeshComponent& MeshComponent, EDSTRCombatAction Action, uint8 Variant);
};
