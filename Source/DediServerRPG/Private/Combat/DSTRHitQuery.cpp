#include "Combat/DSTRHitQuery.h"
#include "DSTRLog.h"

#include "Components/CapsuleComponent.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

namespace
{
	void SweepCandidates(
		const UWorld& World,
		const AActor* Source,
		const FVector& Center,
		const float Radius,
		TArray<FOverlapResult>& OutOverlaps)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DSTRHitQuery), false, Source);
		World.OverlapMultiByChannel(
			OutOverlaps,
			Center,
			FQuat::Identity,
			ECC_Pawn,
			FCollisionShape::MakeSphere(FMath::Max(Radius, 0.0f) + FDSTRHitQuery::CandidateRadiusSlack),
			Params);
	}
}

FVector FDSTRHitQuery::GetSightOrigin(const AActor* Source, const FVector& Center)
{
	// 바닥 이음매에 걸리지 않도록 시야선 시작점을 캡슐 안쪽으로 올린다.
	float Height = DefaultSightHeight;
	if (const ACharacter* Character = Cast<ACharacter>(Source))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			Height = Capsule->GetScaledCapsuleHalfHeight() * SightHeightFraction;
		}
	}
	return Center + FVector(0.0f, 0.0f, Height);
}

bool FDSTRHitQuery::HasLineOfSight(const AActor* Source, const FVector& Center, const AActor* Target)
{
	const UWorld* World = Source ? Source->GetWorld() : nullptr;
	if (!World || !Target)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DSTRHitSight), false, Source);
	Params.AddIgnoredActor(Target);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(
		Hit, GetSightOrigin(Source, Center), Target->GetActorLocation(), ECC_Visibility, Params))
	{
		return true;
	}
	UE_LOG(LogDSTR, Verbose, TEXT("DSTR_LOS_BLOCKED Source=%s Target=%s By=%s"),
		*GetNameSafe(Source), *GetNameSafe(Target), *GetNameSafe(Hit.GetActor()));
	return false;
}

void FDSTRHitQuery::GatherEnemies(
	const AActor* Source,
	const FVector& Center,
	const float Radius,
	TArray<ADSTREnemyCharacter*>& OutTargets)
{
	OutTargets.Reset();
	const UWorld* World = Source ? Source->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	SweepCandidates(*World, Source, Center, Radius, Overlaps);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ADSTREnemyCharacter* Enemy = Cast<ADSTREnemyCharacter>(Overlap.GetActor());
		if (!Enemy || Enemy == Source || Enemy->IsCombatantDead()
			|| Enemy->GetCombatTeam() != EDSTRCombatTeam::Enemy
			|| OutTargets.Contains(Enemy)
			|| !HasLineOfSight(Source, Center, Enemy))
		{
			continue;
		}
		OutTargets.Add(Enemy);
	}
}

void FDSTRHitQuery::GatherPlayers(
	const AActor* Source,
	const FVector& Center,
	const float Radius,
	TArray<ADediServerRPGCharacter*>& OutTargets)
{
	OutTargets.Reset();
	const UWorld* World = Source ? Source->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	SweepCandidates(*World, Source, Center, Radius, Overlaps);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ADediServerRPGCharacter* Player = Cast<ADediServerRPGCharacter>(Overlap.GetActor());
		if (!Player || Player == Source || Player->IsDowned()
			|| OutTargets.Contains(Player)
			|| !HasLineOfSight(Source, Center, Player))
		{
			continue;
		}
		OutTargets.Add(Player);
	}
}
