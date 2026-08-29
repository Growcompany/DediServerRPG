#include "Presentation/DSTRCombatPresentation.h"
#include "DSTRLog.h"

#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	FSoftObjectPath AM(const TCHAR* Folder, const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("/Game/DediServerRPG/Animations/%s/%s.%s"), Folder, Name, Name));
	}

	const FDSTRCombatActionProfile NoneProfile;
	const FDSTRCombatActionProfile BasicAttackProfile{
		{AM(TEXT("Greystone"), TEXT("AM_Greystone_AttackA")), AM(TEXT("Greystone"), TEXT("AM_Greystone_AttackB")), AM(TEXT("Greystone"), TEXT("AM_Greystone_AttackC"))},
		EDSTRVisualRole::Player, 0.25f, 1.5f, 1.35f, true, false, false, 0.55f, {0.40f, 0.40f, 0.65f}, 200.0f, 70.0f};
	const FDSTRCombatActionProfile FortifyProfile{
		{AM(TEXT("Greystone"), TEXT("AM_Greystone_Fortify"))},
		EDSTRVisualRole::Player, 0.0f, 0.85f, 1.15f, true, false, true, 0.5f};
	const FDSTRCombatActionProfile MakeWayProfile{
		{AM(TEXT("Greystone"), TEXT("AM_Greystone_MakeWay"))},
		EDSTRVisualRole::Player, 0.57f, 2.2f, 1.4f, true, false, true, 0.9f, {0.80f}, 0.0f, 0.0f,
		EDSTRImpactMetric::LowestPelvis};
	const FDSTRCombatActionProfile ChargeProfile{
		{AM(TEXT("Greystone"), TEXT("AM_Greystone_Charge"))},
		EDSTRVisualRole::Player, 0.0f, 1.05f, 1.2f, false, false, true, 0.6f};
	const FDSTRCombatActionProfile ReckoningEntryProfile{
		{AM(TEXT("Greystone"), TEXT("AM_Greystone_ReckoningEntry"))},
		EDSTRVisualRole::Player, 0.0f, 1.0f, 0.97f, true, false, true, 1.0f};
	const FDSTRCombatActionProfile ReckoningProfile{
		{AM(TEXT("Greystone"), TEXT("AM_Greystone_Reckoning"))},
		EDSTRVisualRole::Player, 1.0f, 1.9f, 0.90f, true, false, true, 1.3f, {0.90f}, 0.0f, 0.0f,
		EDSTRImpactMetric::LowestWeaponPoint};
	const FDSTRCombatActionProfile ReviveProfile{
		{AM(TEXT("Greystone"), TEXT("AM_Greystone_Revive"))},
		EDSTRVisualRole::Player, 0.45f, 1.0f, 1.2f, true, false, true};
	const FDSTRCombatActionProfile DownedProfile{
		{AM(TEXT("Greystone"), TEXT("AM_Greystone_Downed"))},
		EDSTRVisualRole::Player, 0.0f, 0.0f, 1.0f, true, true, true};
	const FDSTRCombatActionProfile RevivedProfile{
		{AM(TEXT("Greystone"), TEXT("AM_Greystone_Revived"))},
		EDSTRVisualRole::Player, 0.0f, 1.1f, 1.2f, true, false, true};
	const FDSTRCombatActionProfile EnemyMeleeProfile{
		{AM(TEXT("BuffWhite"), TEXT("AM_BuffWhite_Attack1")), AM(TEXT("BuffWhite"), TEXT("AM_BuffWhite_Attack2")), AM(TEXT("BuffWhite"), TEXT("AM_BuffWhite_Attack3"))},
		EDSTRVisualRole::Enemy, 0.3f, 1.0f, 1.0f, true, false, false, 0.0f, {0.174f, 0.174f, 0.174f}, 130.0f, 60.0f};
	const FDSTRCombatActionProfile EnemyDeathProfile{
		{AM(TEXT("BuffWhite"), TEXT("AM_BuffWhite_Death"))},
		EDSTRVisualRole::Enemy, 0.0f, 0.0f, 1.0f, true, true};
	const FDSTRCombatActionProfile BossMeleeProfile{
		{AM(TEXT("Sevarog"), TEXT("AM_Sevarog_Swing1")), AM(TEXT("Sevarog"), TEXT("AM_Sevarog_Swing2")), AM(TEXT("Sevarog"), TEXT("AM_Sevarog_Swing3"))},
		EDSTRVisualRole::Boss, 0.35f, 1.35f, 0.85f, true, false, true, 0.0f, {0.30f, 0.25f, 0.25f}, 215.0f, 75.0f};
	const FDSTRCombatActionProfile BossColossalProfile{
		{AM(TEXT("Sevarog"), TEXT("AM_Sevarog_Colossal"))},
		EDSTRVisualRole::Boss, 0.6f, 1.9f, 0.92f, true, false, true, 0.0f, {0.55f}, 260.0f, 60.0f};
	const FDSTRCombatActionProfile BossRushProfile{
		{AM(TEXT("Sevarog"), TEXT("AM_Sevarog_Rush"))},
		EDSTRVisualRole::Boss, 0.0f, 1.3f, 1.0f, false, false, true, 0.7f};
	const FDSTRCombatActionProfile BossSiphonTargetingProfile{
		{AM(TEXT("Sevarog"), TEXT("AM_Sevarog_SiphonTargeting"))},
		EDSTRVisualRole::Boss, 0.0f, 0.45f, 0.925f, true, false, true, 0.4f};
	const FDSTRCombatActionProfile BossSiphonProfile{
		{AM(TEXT("Sevarog"), TEXT("AM_Sevarog_Siphon"))},
		EDSTRVisualRole::Boss, 0.0f, 1.45f, 1.3f, true, false, true, 1.0f};
	const FDSTRCombatActionProfile BossSubjugateTargetingProfile{
		{AM(TEXT("Sevarog"), TEXT("AM_Sevarog_SubjugateTargeting"))},
		EDSTRVisualRole::Boss, 0.0f, 1.05f, 0.87f, true, false, true, 1.0f};
	const FDSTRCombatActionProfile BossSubjugateProfile{
		{AM(TEXT("Sevarog"), TEXT("AM_Sevarog_Subjugate"))},
		EDSTRVisualRole::Boss, 0.75f, 2.7f, 1.0f, true, false, true, 0.0f, {0.75f}, 0.0f, 0.0f,
		EDSTRImpactMetric::LowestWeaponPoint};
	const FDSTRCombatActionProfile BossStunProfile{
		{AM(TEXT("Sevarog"), TEXT("AM_Sevarog_StunStart")), AM(TEXT("Sevarog"), TEXT("AM_Sevarog_StunLoop")), AM(TEXT("Sevarog"), TEXT("AM_Sevarog_StunEnd"))},
		EDSTRVisualRole::Boss, 0.0f, 0.8f, 1.0f, true, false, true};
	const FDSTRCombatActionProfile BossKnockbackProfile{
		{AM(TEXT("Sevarog"), TEXT("AM_Sevarog_Knockback"))},
		EDSTRVisualRole::Boss, 0.0f, 0.95f, 1.0f, true, false, true};
	const FDSTRCombatActionProfile BossDeathProfile{
		{AM(TEXT("Sevarog"), TEXT("AM_Sevarog_Death"))},
		EDSTRVisualRole::Boss, 0.0f, 0.0f, 1.0f, true, true, true};
	const FDSTRCombatActionProfile HitReactProfile{
		{AM(TEXT("Greystone"), TEXT("AM_Greystone_HitFront")), AM(TEXT("Greystone"), TEXT("AM_Greystone_HitBack")), AM(TEXT("Greystone"), TEXT("AM_Greystone_HitLeft")), AM(TEXT("Greystone"), TEXT("AM_Greystone_HitRight"))},
		EDSTRVisualRole::Player, 0.0f, 0.4f, 1.3f, false, false};
	const FDSTRCombatActionProfile EnemyHitReactProfile{
		{AM(TEXT("BuffWhite"), TEXT("AM_BuffWhite_HitFront")), AM(TEXT("BuffWhite"), TEXT("AM_BuffWhite_HitBack")), AM(TEXT("BuffWhite"), TEXT("AM_BuffWhite_HitLeft")), AM(TEXT("BuffWhite"), TEXT("AM_BuffWhite_HitRight"))},
		EDSTRVisualRole::Enemy, 0.0f, 0.45f, 1.2f, false, false};
	const FDSTRCombatActionProfile BossHitReactProfile{
		{AM(TEXT("Sevarog"), TEXT("AM_Sevarog_HitFront")), AM(TEXT("Sevarog"), TEXT("AM_Sevarog_HitBack")), AM(TEXT("Sevarog"), TEXT("AM_Sevarog_HitLeft")), AM(TEXT("Sevarog"), TEXT("AM_Sevarog_HitRight"))},
		EDSTRVisualRole::Boss, 0.0f, 0.4f, 1.3f, false, false};
}

const FDSTRCombatActionProfile& FDSTRCombatPresentation::GetProfile(
	const EDSTRCombatAction Action)
{
	switch (Action)
	{
	case EDSTRCombatAction::BasicAttack: return BasicAttackProfile;
	case EDSTRCombatAction::Fortify: return FortifyProfile;
	case EDSTRCombatAction::MakeWay: return MakeWayProfile;
	case EDSTRCombatAction::Charge: return ChargeProfile;
	case EDSTRCombatAction::ReckoningEntry: return ReckoningEntryProfile;
	case EDSTRCombatAction::Reckoning: return ReckoningProfile;
	case EDSTRCombatAction::Revive: return ReviveProfile;
	case EDSTRCombatAction::Downed: return DownedProfile;
	case EDSTRCombatAction::Revived: return RevivedProfile;
	case EDSTRCombatAction::EnemyMelee: return EnemyMeleeProfile;
	case EDSTRCombatAction::EnemyDeath: return EnemyDeathProfile;
	case EDSTRCombatAction::BossMelee: return BossMeleeProfile;
	case EDSTRCombatAction::BossColossal: return BossColossalProfile;
	case EDSTRCombatAction::BossRush: return BossRushProfile;
	case EDSTRCombatAction::BossSiphonTargeting: return BossSiphonTargetingProfile;
	case EDSTRCombatAction::BossSiphon: return BossSiphonProfile;
	case EDSTRCombatAction::BossSubjugateTargeting: return BossSubjugateTargetingProfile;
	case EDSTRCombatAction::BossSubjugate: return BossSubjugateProfile;
	case EDSTRCombatAction::BossStun: return BossStunProfile;
	case EDSTRCombatAction::BossKnockback: return BossKnockbackProfile;
	case EDSTRCombatAction::BossDeath: return BossDeathProfile;
	case EDSTRCombatAction::HitReact: return HitReactProfile;
	case EDSTRCombatAction::EnemyHitReact: return EnemyHitReactProfile;
	case EDSTRCombatAction::BossHitReact: return BossHitReactProfile;
	default: return NoneProfile;
	}
}

FVector FDSTRCombatPresentation::GetTelegraphScale3D(const float DamageRadius)
{
	const float Footprint = GetTelegraphScale(DamageRadius);
	return FVector(Footprint, Footprint, 1.0f);
}

float FDSTRCombatPresentation::GetImpactMontageTime(const EDSTRCombatAction Action, const uint8 Variant)
{
	const FDSTRCombatActionProfile& Profile = GetProfile(Action);
	if (Profile.ImpactTimes.Num() == 0)
	{
		return Profile.ImpactDelay * Profile.PlayRate;
	}
	return Profile.ImpactTimes[Variant % Profile.ImpactTimes.Num()];
}

float FDSTRCombatPresentation::GetImpactDelay(const EDSTRCombatAction Action, const uint8 Variant)
{
	const FDSTRCombatActionProfile& Profile = GetProfile(Action);
	const float PlayRate = Profile.PlayRate > KINDA_SMALL_NUMBER ? Profile.PlayRate : 1.0f;
	return GetImpactMontageTime(Action, Variant) / PlayRate;
}

const FSoftObjectPath& FDSTRCombatPresentation::ResolveVariant(const EDSTRCombatAction Action, const uint8 Variant)
{
	static const FSoftObjectPath Empty;
	const TArray<FSoftObjectPath>& Variants = GetProfile(Action).Variants;
	return Variants.Num() > 0 ? Variants[Variant % Variants.Num()] : Empty;
}

EDSTRHitDirection FDSTRCombatPresentation::ResolveHitDirection(const FVector& ActorForward, const FVector& ToAttacker)
{
	const FVector Flat(ToAttacker.X, ToAttacker.Y, 0.0f);
	if (Flat.IsNearlyZero())
	{
		return EDSTRHitDirection::Front;
	}
	const FVector Forward = FVector(ActorForward.X, ActorForward.Y, 0.0f).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const float Ahead = FVector::DotProduct(Flat, Forward);
	const float Side = FVector::DotProduct(Flat, Right);
	if (FMath::Abs(Ahead) >= FMath::Abs(Side))
	{
		return Ahead >= 0.0f ? EDSTRHitDirection::Front : EDSTRHitDirection::Back;
	}
	return Side >= 0.0f ? EDSTRHitDirection::Right : EDSTRHitDirection::Left;
}

const FName& FDSTRCombatPresentation::GetFullBodySlotName()
{
	static const FName FullBodySlot(TEXT("DefaultSlot"));
	return FullBodySlot;
}

TArray<FName> FDSTRCombatPresentation::GetAnimClassSlotNames(const UClass* AnimClass)
{
	TArray<FName> Names;
	const IAnimClassInterface* AnimInterface = IAnimClassInterface::GetFromClass(AnimClass);
	if (!AnimInterface)
	{
		return Names;
	}
	const UObject* DefaultObject = AnimClass->GetDefaultObject();
	for (const FStructProperty* Property : AnimInterface->GetAnimNodeProperties())
	{
		if (Property && Property->Struct == FAnimNode_Slot::StaticStruct())
		{
			const FAnimNode_Slot* Node = Property->ContainerPtrToValuePtr<FAnimNode_Slot>(DefaultObject);
			if (Node)
			{
				Names.AddUnique(Node->SlotName);
			}
		}
	}
	return Names;
}

FDSTRCombatPresentation::FStunSegments FDSTRCombatPresentation::GetStunSegments(const float StunSeconds)
{
	FStunSegments Segments;
	const float Total = FMath::Max(0.0f, StunSeconds);
	Segments.EndSeconds = FMath::Min(Total, BossStunEndSeconds);
	Segments.StartSeconds = FMath::Min(Total - Segments.EndSeconds, BossStunStartSeconds);
	Segments.LoopSeconds = Total - Segments.EndSeconds - Segments.StartSeconds;
	return Segments;
}

float FDSTRCombatPresentation::CorpseSinkOffset(const float SecondsSinceDeath, const bool bBoss)
{
	const float SinkStart = GetCorpseLifetime(bBoss) - CorpseSinkSeconds;
	const float Sinking = FMath::Clamp(SecondsSinceDeath - SinkStart, 0.0f, CorpseSinkSeconds);
	return -GetCorpseSinkSpeed(bBoss) * Sinking;
}

bool FDSTRCombatPresentation::PlayAction(
	USkeletalMeshComponent& MeshComponent,
	const EDSTRCombatAction Action,
	const uint8 Variant)
{
	const FDSTRCombatActionProfile& Profile = GetProfile(Action);
	UAnimMontage* Montage = Cast<UAnimMontage>(ResolveVariant(Action, Variant).ResolveObject());
	if (!Montage)
	{
		return false;
	}

	if (UAnimSingleNodeInstance* SingleNode = MeshComponent.GetSingleNodeInstance())
	{
		UE_CLOG(MeshComponent.GetAnimClass() != nullptr, LogDSTR, Warning,
			TEXT("DSTR_ANIM_SINGLENODE_FALLBACK Mesh=%s Action=%d"),
			*MeshComponent.GetName(), static_cast<int32>(Action));
		SingleNode->SetAnimationAsset(Montage, false, Profile.PlayRate);
		SingleNode->SetPlaying(true);
		return true;
	}
	if (UAnimInstance* AnimInstance = MeshComponent.GetAnimInstance())
	{
		return AnimInstance->Montage_Play(Montage, Profile.PlayRate) > 0.0f;
	}
	return false;
}
