#pragma once

#include "CoreMinimal.h"

class AActor;
class ADediServerRPGCharacter;
class ADSTREnemyCharacter;

class DEDISERVERRPG_API FDSTRHitQuery
{
public:
	static constexpr float CandidateRadiusSlack = 200.0f;
	static constexpr float SightHeightFraction = 0.5f;
	static constexpr float DefaultSightHeight = 44.0f;

	static void GatherEnemies(
		const AActor* Source,
		const FVector& Center,
		float Radius,
		TArray<ADSTREnemyCharacter*>& OutTargets);

	static void GatherPlayers(
		const AActor* Source,
		const FVector& Center,
		float Radius,
		TArray<ADediServerRPGCharacter*>& OutTargets);

	static bool HasLineOfSight(const AActor* Source, const FVector& Center, const AActor* Target);

	static FVector GetSightOrigin(const AActor* Source, const FVector& Center);
};
