#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/SoftObjectPath.h"
#include "Presentation/DSTRCombatFeedback.h"

class USkeletalMeshComponent;

enum class EDSTRVisualRole : uint8
{
	Player,
	Enemy,
	Boss
};

struct FDSTRCharacterVisualAssets
{
	FSoftObjectPath SkeletalMesh;
	FSoftClassPath AnimationBlueprint;
	FSoftObjectPath IdleAnimation;
	FSoftObjectPath MovementAnimation;
	FSoftObjectPath LocomotionBlendSpace;
	FVector RelativeLocation = FVector::ZeroVector;
	FRotator RelativeRotation = FRotator::ZeroRotator;
	FVector RelativeScale = FVector::OneVector;
};

struct FDSTRFeedbackSound
{
	FSoftObjectPath Path;
	float Volume = 1.0f;
	float Pitch = 1.0f;
};

class DEDISERVERRPG_API FDSTRVisualAssetRegistry
{
public:
	static const FDSTRCharacterVisualAssets& GetCharacterVisual(EDSTRVisualRole Role);
	static const FSoftObjectPath& GetBasicAttackEffect();
	static const FSoftObjectPath& GetMakeWayEffect();
	static const FSoftObjectPath& GetChargeEffect();
	static const FSoftObjectPath& GetDownedEffect();
	static const FSoftObjectPath& GetReviveEffect();
	static const FSoftObjectPath& GetAttackBuffEffect();
	static const FSoftObjectPath& GetEnemyImpactEffect();
	static const FSoftObjectPath& GetEnemyTelegraphEffect();
	static const FSoftObjectPath& GetEnemySpawnEffect();
	static const FSoftObjectPath& GetFortifyEffect();
	static const FSoftObjectPath& GetReckoningEffect();
	static const FSoftObjectPath& GetGateSealedEffect();
	static const FSoftObjectPath& GetAttackBuffPickupMesh();
	static const FDSTRFeedbackSound& GetFeedbackSound(EDSTRCombatFeedback Feedback, bool bBossVariant);
	static bool ShouldLoadVisualAssets(ENetMode NetMode);
	static TArray<FSoftObjectPath> GetPreloadPaths();
	static void PreloadVisualAssets(const UObject* WorldContextObject, TFunction<void()> Completion);
	static bool AreVisualAssetsLoaded(const UObject* WorldContextObject);
	static bool ApplyCharacterVisual(USkeletalMeshComponent& MeshComponent, EDSTRVisualRole Role);
	static float ComputeLocomotionDirection(const FVector& Forward, const FVector& Velocity);
	static void UpdateMovementAnimation(
		USkeletalMeshComponent& MeshComponent,
		EDSTRVisualRole Role,
		const FVector& Velocity,
		const FVector& Forward);
};
