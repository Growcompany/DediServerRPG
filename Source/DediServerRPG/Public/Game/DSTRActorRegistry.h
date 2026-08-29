#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DSTRActorRegistry.generated.h"

class ADediServerRPGCharacter;
class ADSTRAttackBuffPickup;
class ADSTREnemyCharacter;

UCLASS()
class DEDISERVERRPG_API UDSTRActorRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDSTRActorRegistry* Get(const UWorld* World);

	void RegisterHero(ADediServerRPGCharacter* Hero);
	void UnregisterHero(ADediServerRPGCharacter* Hero);
	void RegisterEnemy(ADSTREnemyCharacter* Enemy);
	void UnregisterEnemy(ADSTREnemyCharacter* Enemy);
	void RegisterPickup(ADSTRAttackBuffPickup* Pickup);
	void UnregisterPickup(ADSTRAttackBuffPickup* Pickup);

	const TArray<TWeakObjectPtr<ADediServerRPGCharacter>>& GetHeroes() const;
	const TArray<TWeakObjectPtr<ADSTREnemyCharacter>>& GetEnemies() const;
	const TArray<TWeakObjectPtr<ADSTRAttackBuffPickup>>& GetPickups() const;

private:
	mutable TArray<TWeakObjectPtr<ADediServerRPGCharacter>> Heroes;
	mutable TArray<TWeakObjectPtr<ADSTREnemyCharacter>> Enemies;
	mutable TArray<TWeakObjectPtr<ADSTRAttackBuffPickup>> Pickups;
};
