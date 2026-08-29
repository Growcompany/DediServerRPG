#include "Presentation/DSTRVisualAssetRegistry.h"
#include "DSTRLog.h"

#include "Animation/AnimationAsset.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StreamableManager.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "Presentation/DSTRVisualAssetSubsystem.h"
#include "Animation/BlendSpace.h"

namespace
{
	const FDSTRCharacterVisualAssets EmptyCharacterVisual;
	const FDSTRCharacterVisualAssets PlayerVisual{
		FSoftObjectPath(TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Meshes/Greystone.Greystone")),
		FSoftClassPath(TEXT("/Game/DediServerRPG/Animations/Greystone/ABP_Greystone_FullBody.ABP_Greystone_FullBody_C")),
		FSoftObjectPath(TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Idle.Idle")),
		FSoftObjectPath(TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Jog_Fwd.Jog_Fwd")),
		FSoftObjectPath(),
		FVector(0.0f, 0.0f, -95.0f),
		FRotator(0.0f, -90.0f, 0.0f),
		FVector::OneVector};
	const FDSTRCharacterVisualAssets EnemyVisual{
		FSoftObjectPath(TEXT("/Game/ParagonMinions/Characters/Buff/Buff_White/Meshes/Buff_White.Buff_White")),
		FSoftClassPath(),
		FSoftObjectPath(TEXT("/Game/ParagonMinions/Characters/Buff/Buff_White/Animations/Melee_Idle_A.Melee_Idle_A")),
		FSoftObjectPath(TEXT("/Game/ParagonMinions/Characters/Buff/Buff_White/Animations/Melee_Run_Forward.Melee_Run_Forward")),
		FSoftObjectPath(TEXT("/Game/ParagonMinions/Characters/Buff/Buff_White/Animations/Blendspaces/Melee_Idle_Jogs_Runs_BS_A.Melee_Idle_Jogs_Runs_BS_A")),
		FVector(0.0f, 0.0f, -88.0f),
		FRotator(0.0f, -90.0f, 0.0f),
		FVector(1.15f)};
	const FDSTRCharacterVisualAssets BossVisual{
		FSoftObjectPath(TEXT("/Game/ParagonSevarog/Characters/Heroes/Sevarog/Meshes/Sevarog.Sevarog")),
		FSoftClassPath(TEXT("/Game/DediServerRPG/Animations/Sevarog/ABP_Sevarog_FullBody.ABP_Sevarog_FullBody_C")),
		FSoftObjectPath(TEXT("/Game/ParagonSevarog/Characters/Heroes/Sevarog/Animations/Idle.Idle")),
		FSoftObjectPath(TEXT("/Game/ParagonSevarog/Characters/Heroes/Sevarog/Animations/Jog_Fwd.Jog_Fwd")),
		FSoftObjectPath(),
		FVector(0.0f, 0.0f, -102.7f),
		FRotator(0.0f, -90.0f, 0.0f),
		FVector::OneVector};
	const FSoftObjectPath BasicAttackEffect(
		TEXT("/Game/FXVarietyPack/Particles/P_ky_hit2.P_ky_hit2"));
	const FSoftObjectPath DownedEffect(
		TEXT("/Game/FXVarietyPack/Particles/P_ky_darkStorm.P_ky_darkStorm"));
	const FSoftObjectPath ReviveEffect(
		TEXT("/Game/FXVarietyPack/Particles/P_ky_healAura.P_ky_healAura"));
	const FSoftObjectPath AttackBuffEffect(
		TEXT("/Game/FXVarietyPack/Particles/P_ky_lightning1.P_ky_lightning1"));
	const FSoftObjectPath EnemyImpactEffect(
		TEXT("/Game/FXVarietyPack/Particles/P_ky_explosion.P_ky_explosion"));
	const FSoftObjectPath EnemyTelegraphEffect(
		TEXT("/Game/FXVarietyPack/Particles/P_ky_magicCircle1.P_ky_magicCircle1"));
	const FSoftObjectPath EnemySpawnEffect(
		TEXT("/Game/FXVarietyPack/Particles/P_ky_darkStorm.P_ky_darkStorm"));
	const FSoftObjectPath FortifyEffect(
		TEXT("/Game/FXVarietyPack/Particles/P_ky_healAura.P_ky_healAura"));
	const FSoftObjectPath MakeWayEffect(
		TEXT("/Game/FXVarietyPack/Particles/P_ky_shotShockwave.P_ky_shotShockwave"));
	const FSoftObjectPath ChargeEffect(
		TEXT("/Game/FXVarietyPack/Particles/P_ky_shotShockwave.P_ky_shotShockwave"));
	const FSoftObjectPath ReckoningEffect(
		TEXT("/Game/FXVarietyPack/Particles/P_ky_explosion.P_ky_explosion"));
	const FSoftObjectPath GateSealedEffect(
		TEXT("/Game/FXVarietyPack/Particles/P_ky_aquaStorm.P_ky_aquaStorm"));

	const FSoftObjectPath AttackBuffPickupMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	const FDSTRFeedbackSound EmptyFeedbackSound;
	const FDSTRFeedbackSound BasicAttackSound{
		FSoftObjectPath(TEXT("/Game/ParagonGreystone/Audio/Cues/Greystone_Effort_Swing.Greystone_Effort_Swing")), 0.8f, 1.0f};
	const FDSTRFeedbackSound FortifySound{
		FSoftObjectPath(TEXT("/Game/ParagonGreystone/Audio/Cues/Greystone_Effort_Ability_Q.Greystone_Effort_Ability_Q")), 0.9f, 1.0f};
	const FDSTRFeedbackSound MakeWaySound{
		FSoftObjectPath(TEXT("/Game/ParagonGreystone/Audio/Cues/Greystone_Effort_Ability_RMB.Greystone_Effort_Ability_RMB")), 0.9f, 1.0f};
	const FDSTRFeedbackSound ChargeSound{
		FSoftObjectPath(TEXT("/Game/ParagonGreystone/Audio/Cues/Greystone_Effort_JumpHeavy.Greystone_Effort_JumpHeavy")), 0.9f, 0.9f};
	const FDSTRFeedbackSound ReckoningWarningSound{
		FSoftObjectPath(TEXT("/Game/ParagonGreystone/Audio/Cues/Greystone_Ability_Ultimate_Self.Greystone_Ability_Ultimate_Self")), 1.0f, 1.0f};
	const FDSTRFeedbackSound ReckoningSound{
		FSoftObjectPath(TEXT("/Game/ParagonGreystone/Audio/Cues/Greystone_Effort_Ability_Ultimate_Rebirth.Greystone_Effort_Ability_Ultimate_Rebirth")), 1.0f, 0.9f};
	const FDSTRFeedbackSound BossRushSound{
		FSoftObjectPath(TEXT("/Game/ParagonSevarog/Characters/Heroes/Sevarog/Sounds/SoundCues/Sevarog_Effort_Ability_Q.Sevarog_Effort_Ability_Q")), 1.0f, 1.0f};
	const FDSTRFeedbackSound BossSiphonSound{
		FSoftObjectPath(TEXT("/Game/ParagonSevarog/Characters/Heroes/Sevarog/Sounds/SoundCues/Sevarog_Effort_Ability_E.Sevarog_Effort_Ability_E")), 1.0f, 1.0f};
	const FDSTRFeedbackSound BossSubjugateSound{
		FSoftObjectPath(TEXT("/Game/ParagonSevarog/Characters/Heroes/Sevarog/Sounds/SoundCues/Sevarog_Effort_Ability_Ultimate.Sevarog_Effort_Ability_Ultimate")), 1.0f, 0.95f};
	const FDSTRFeedbackSound EnemyWakeSound{
		FSoftObjectPath(TEXT("/Game/StarterContent/Audio/Explosion_Cue.Explosion_Cue")), 0.7f, 1.1f};
	const FDSTRFeedbackSound BossWakeSound{
		FSoftObjectPath(TEXT("/Game/ParagonSevarog/Characters/Heroes/Sevarog/Sounds/SoundCues/Sevarog_Effort_Ability_RMB.Sevarog_Effort_Ability_RMB")), 1.0f, 0.85f};
	const FDSTRFeedbackSound HitDealtSound{
		FSoftObjectPath(TEXT("/Game/StarterContent/Audio/Collapse_Cue.Collapse_Cue")), 0.45f, 1.5f};
	const FDSTRFeedbackSound BossHitDealtSound{
		FSoftObjectPath(TEXT("/Game/ParagonSevarog/Characters/Heroes/Sevarog/Sounds/SoundCues/Sevarog_Effort_Pain.Sevarog_Effort_Pain")), 0.9f, 1.0f};
	const FDSTRFeedbackSound HitTakenSound{
		FSoftObjectPath(TEXT("/Game/ParagonGreystone/Audio/Cues/Greystone_Effort_Pain.Greystone_Effort_Pain")), 0.9f, 1.0f};
	const FDSTRFeedbackSound DownedSound{
		FSoftObjectPath(TEXT("/Game/ParagonGreystone/Audio/Cues/Greystone_Effort_Death.Greystone_Effort_Death")), 1.0f, 1.0f};
	const FDSTRFeedbackSound RevivedSound{
		FSoftObjectPath(TEXT("/Game/ParagonGreystone/Audio/Cues/Greystone_Revive.Greystone_Revive")), 1.0f, 1.0f};
	const FDSTRFeedbackSound AttackBuffSound{
		FSoftObjectPath(TEXT("/Game/ParagonGreystone/Audio/Cues/Greystone_Status_Buffed.Greystone_Status_Buffed")), 0.9f, 1.0f};
	const FDSTRFeedbackSound EnemyTelegraphSound{
		FSoftObjectPath(TEXT("/Game/ParagonSevarog/Characters/Heroes/Sevarog/Sounds/SoundCues/Sevarog_Effort_Ability_RMB.Sevarog_Effort_Ability_RMB")), 1.0f, 0.9f};
	const FDSTRFeedbackSound EnemyDeathSound{
		FSoftObjectPath(TEXT("/Game/StarterContent/Audio/Collapse_Cue.Collapse_Cue")), 0.6f, 0.8f};
	const FDSTRFeedbackSound BossDeathSound{
		FSoftObjectPath(TEXT("/Game/ParagonSevarog/Characters/Heroes/Sevarog/Sounds/SoundCues/Sevarog_Death.Sevarog_Death")), 1.0f, 1.0f};
	const FDSTRFeedbackSound BossImpactSound{
		FSoftObjectPath(TEXT("/Game/StarterContent/Audio/Explosion_Cue.Explosion_Cue")), 0.8f, 0.9f};
	const FDSTRFeedbackSound* const AllFeedbackSounds[] = {
		&BasicAttackSound, &HitDealtSound, &BossHitDealtSound,
		&HitTakenSound, &DownedSound, &RevivedSound, &AttackBuffSound,
		&EnemyTelegraphSound, &EnemyDeathSound, &BossDeathSound, &BossImpactSound,
		&FortifySound, &MakeWaySound, &ChargeSound, &ReckoningWarningSound, &ReckoningSound,
		&EnemyWakeSound, &BossWakeSound,
		&BossRushSound, &BossSiphonSound, &BossSubjugateSound};

	TArray<FSoftObjectPath> GetVisualPreloadPaths()
	{
		TArray<FSoftObjectPath> Paths;
		for (const FDSTRCharacterVisualAssets* Visual : {&PlayerVisual, &EnemyVisual, &BossVisual})
		{
			Paths.Add(Visual->SkeletalMesh);
			if (Visual->AnimationBlueprint.IsValid())
			{
				Paths.Add(FSoftObjectPath(Visual->AnimationBlueprint.ToString()));
			}
			else
			{
				Paths.Add(Visual->IdleAnimation);
				Paths.Add(Visual->MovementAnimation);
				if (Visual->LocomotionBlendSpace.IsValid())
				{
					Paths.Add(Visual->LocomotionBlendSpace);
				}
			}
		}
		for (const EDSTRCombatAction Action : {
			EDSTRCombatAction::BasicAttack, EDSTRCombatAction::Fortify,
			EDSTRCombatAction::MakeWay, EDSTRCombatAction::Charge,
			EDSTRCombatAction::ReckoningEntry, EDSTRCombatAction::Reckoning,
			EDSTRCombatAction::Revive,
			EDSTRCombatAction::Downed, EDSTRCombatAction::Revived,
			EDSTRCombatAction::EnemyMelee, EDSTRCombatAction::EnemyDeath,
			EDSTRCombatAction::BossMelee, EDSTRCombatAction::BossColossal,
			EDSTRCombatAction::BossRush,
			EDSTRCombatAction::BossSiphonTargeting, EDSTRCombatAction::BossSiphon,
			EDSTRCombatAction::BossSubjugateTargeting, EDSTRCombatAction::BossSubjugate,
			EDSTRCombatAction::BossStun, EDSTRCombatAction::BossKnockback,
			EDSTRCombatAction::BossDeath,
			EDSTRCombatAction::HitReact, EDSTRCombatAction::EnemyHitReact,
			EDSTRCombatAction::BossHitReact})
		{
			Paths.Append(FDSTRCombatPresentation::GetProfile(Action).Variants);
		}
		Paths.Append({BasicAttackEffect, DownedEffect,
			ReviveEffect, AttackBuffEffect, EnemyImpactEffect, EnemyTelegraphEffect,
			EnemySpawnEffect, FortifyEffect, MakeWayEffect, ChargeEffect, ReckoningEffect,
			GateSealedEffect, AttackBuffPickupMesh});
		for (const FDSTRFeedbackSound* Sound : AllFeedbackSounds)
		{
			Paths.Add(Sound->Path);
		}
		Paths.RemoveAll([](const FSoftObjectPath& Path) { return !Path.IsValid(); });
		return Paths;
	}
}

const FDSTRCharacterVisualAssets& FDSTRVisualAssetRegistry::GetCharacterVisual(
	const EDSTRVisualRole Role)
{
	switch (Role)
	{
	case EDSTRVisualRole::Player:
		return PlayerVisual;
	case EDSTRVisualRole::Enemy:
		return EnemyVisual;
	case EDSTRVisualRole::Boss:
		return BossVisual;
	default:
		return EmptyCharacterVisual;
	}
}

const FSoftObjectPath& FDSTRVisualAssetRegistry::GetBasicAttackEffect()
{
	return BasicAttackEffect;
}

const FSoftObjectPath& FDSTRVisualAssetRegistry::GetMakeWayEffect()
{
	return MakeWayEffect;
}

const FSoftObjectPath& FDSTRVisualAssetRegistry::GetChargeEffect()
{
	return ChargeEffect;
}

const FSoftObjectPath& FDSTRVisualAssetRegistry::GetDownedEffect()
{
	return DownedEffect;
}

const FSoftObjectPath& FDSTRVisualAssetRegistry::GetReviveEffect()
{
	return ReviveEffect;
}

const FSoftObjectPath& FDSTRVisualAssetRegistry::GetAttackBuffEffect()
{
	return AttackBuffEffect;
}

const FSoftObjectPath& FDSTRVisualAssetRegistry::GetEnemyImpactEffect()
{
	return EnemyImpactEffect;
}

const FSoftObjectPath& FDSTRVisualAssetRegistry::GetEnemyTelegraphEffect()
{
	return EnemyTelegraphEffect;
}

const FSoftObjectPath& FDSTRVisualAssetRegistry::GetEnemySpawnEffect()
{
	return EnemySpawnEffect;
}

const FSoftObjectPath& FDSTRVisualAssetRegistry::GetFortifyEffect()
{
	return FortifyEffect;
}

const FSoftObjectPath& FDSTRVisualAssetRegistry::GetReckoningEffect()
{
	return ReckoningEffect;
}

const FSoftObjectPath& FDSTRVisualAssetRegistry::GetGateSealedEffect()
{
	return GateSealedEffect;
}

const FSoftObjectPath& FDSTRVisualAssetRegistry::GetAttackBuffPickupMesh()
{
	return AttackBuffPickupMesh;
}

const FDSTRFeedbackSound& FDSTRVisualAssetRegistry::GetFeedbackSound(
	const EDSTRCombatFeedback Feedback,
	const bool bBossVariant)
{
	switch (Feedback)
	{
	case EDSTRCombatFeedback::BasicAttack:
		return BasicAttackSound;
	case EDSTRCombatFeedback::Fortify:
		return FortifySound;
	case EDSTRCombatFeedback::MakeWay:
		return MakeWaySound;
	case EDSTRCombatFeedback::Charge:
		return ChargeSound;
	case EDSTRCombatFeedback::ReckoningWarning:
		return ReckoningWarningSound;
	case EDSTRCombatFeedback::Reckoning:
		return ReckoningSound;
	case EDSTRCombatFeedback::BossRush:
		return BossRushSound;
	case EDSTRCombatFeedback::BossSiphon:
		return BossSiphonSound;
	case EDSTRCombatFeedback::BossSubjugate:
		return BossSubjugateSound;
	case EDSTRCombatFeedback::EnemyWake:
		return bBossVariant ? BossWakeSound : EnemyWakeSound;
	case EDSTRCombatFeedback::HitDealt:
		return bBossVariant ? BossHitDealtSound : HitDealtSound;
	case EDSTRCombatFeedback::HitTaken:
		return HitTakenSound;
	case EDSTRCombatFeedback::Downed:
		return DownedSound;
	case EDSTRCombatFeedback::Revived:
		return RevivedSound;
	case EDSTRCombatFeedback::AttackBuff:
		return AttackBuffSound;
	case EDSTRCombatFeedback::EnemyTelegraph:
		return EnemyTelegraphSound;
	case EDSTRCombatFeedback::EnemyDeath:
		return bBossVariant ? BossDeathSound : EnemyDeathSound;
	case EDSTRCombatFeedback::BossImpact:
		return BossImpactSound;
	default:
		return EmptyFeedbackSound;
	}
}

bool FDSTRVisualAssetRegistry::ShouldLoadVisualAssets(const ENetMode NetMode)
{
	return NetMode != NM_DedicatedServer;
}

TArray<FSoftObjectPath> FDSTRVisualAssetRegistry::GetPreloadPaths()
{
	return GetVisualPreloadPaths();
}

void FDSTRVisualAssetRegistry::PreloadVisualAssets(
	const UObject* WorldContextObject,
	TFunction<void()> Completion)
{
	if (UDSTRVisualAssetSubsystem* Subsystem = UDSTRVisualAssetSubsystem::Get(WorldContextObject))
	{
		Subsystem->Preload(MoveTemp(Completion));
	}
}

bool FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(const UObject* WorldContextObject)
{
	const UDSTRVisualAssetSubsystem* Subsystem = UDSTRVisualAssetSubsystem::Get(WorldContextObject);
	return Subsystem && Subsystem->IsLoaded();
}

bool FDSTRVisualAssetRegistry::ApplyCharacterVisual(
	USkeletalMeshComponent& MeshComponent,
	const EDSTRVisualRole Role)
{
	const FDSTRCharacterVisualAssets& Visual = GetCharacterVisual(Role);
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Visual.SkeletalMesh.ResolveObject());
	if (!SkeletalMesh)
	{
		return false;
	}

	MeshComponent.SetSkeletalMeshAsset(SkeletalMesh);
	if (Visual.AnimationBlueprint.IsValid())
	{
		UClass* AnimationClass = Cast<UClass>(
			FSoftObjectPath(Visual.AnimationBlueprint.ToString()).ResolveObject());
		if (!AnimationClass)
		{
			return false;
		}
		MeshComponent.SetAnimationMode(EAnimationMode::AnimationBlueprint);
		MeshComponent.SetAnimInstanceClass(AnimationClass);
	}
	else
	{
		UAnimationAsset* Locomotion = Cast<UAnimationAsset>(Visual.LocomotionBlendSpace.ResolveObject());
		if (!Locomotion)
		{
			Locomotion = Cast<UAnimationAsset>(Visual.IdleAnimation.ResolveObject());
		}
		if (!Locomotion || !Visual.MovementAnimation.IsValid())
		{
			return false;
		}
		MeshComponent.SetAnimationMode(EAnimationMode::AnimationSingleNode);
		MeshComponent.SetAnimation(Locomotion);
		MeshComponent.Play(true);
	}
	MeshComponent.SetRelativeLocation(Visual.RelativeLocation);
	MeshComponent.SetRelativeRotation(Visual.RelativeRotation);
	MeshComponent.SetRelativeScale3D(Visual.RelativeScale);
	if (ACharacter* Character = Cast<ACharacter>(MeshComponent.GetOwner()))
	{
		Character->CacheInitialMeshOffset(Visual.RelativeLocation, Visual.RelativeRotation);
	}
	return true;
}

float FDSTRVisualAssetRegistry::ComputeLocomotionDirection(const FVector& Forward, const FVector& Velocity)
{
	const FVector Flat(Velocity.X, Velocity.Y, 0.0f);
	if (Flat.IsNearlyZero(1.0f))
	{
		return 0.0f;
	}
	const FVector Fwd = FVector(Forward.X, Forward.Y, 0.0f).GetSafeNormal();
	const FVector Dir = Flat.GetSafeNormal();
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X) - FMath::Atan2(Fwd.Y, Fwd.X));
	return FRotator::NormalizeAxis(Angle);
}

void FDSTRVisualAssetRegistry::UpdateMovementAnimation(
	USkeletalMeshComponent& MeshComponent,
	const EDSTRVisualRole Role,
	const FVector& Velocity,
	const FVector& Forward)
{
	const FDSTRCharacterVisualAssets& Visual = GetCharacterVisual(Role);
	UAnimSingleNodeInstance* SingleNode = MeshComponent.GetSingleNodeInstance();
	if (!SingleNode)
	{
		return;
	}

	if (UBlendSpace* BlendSpace = Cast<UBlendSpace>(Visual.LocomotionBlendSpace.ResolveObject()))
	{
		if (SingleNode->GetCurrentAsset() != BlendSpace)
		{
			SingleNode->SetAnimationAsset(BlendSpace, true, 1.0f);
			SingleNode->SetPlaying(true);
		}
		SingleNode->SetBlendSpacePosition(FVector(ComputeLocomotionDirection(Forward, Velocity), Velocity.Size2D(), 0.0f));
		return;
	}

	const FSoftObjectPath& DesiredPath = Velocity.Size2D() > 10.0f ? Visual.MovementAnimation : Visual.IdleAnimation;
	UAnimationAsset* DesiredAnimation = Cast<UAnimationAsset>(DesiredPath.ResolveObject());
	if (DesiredAnimation && SingleNode->GetCurrentAsset() != DesiredAnimation)
	{
		SingleNode->SetAnimationAsset(DesiredAnimation, true, 1.0f);
	}
}
