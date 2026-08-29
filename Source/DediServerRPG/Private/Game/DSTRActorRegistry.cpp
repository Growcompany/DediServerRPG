#include "Game/DSTRActorRegistry.h"

#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "Engine/World.h"
#include "World/DSTRAttackBuffPickup.h"

namespace
{
	template <typename T>
	void AddUniqueHandle(TArray<TWeakObjectPtr<T>>& List, T* Actor)
	{
		if (Actor)
		{
			List.AddUnique(Actor);
		}
	}

	template <typename T>
	void RemoveHandle(TArray<TWeakObjectPtr<T>>& List, T* Actor)
	{
		List.RemoveAllSwap([Actor](const TWeakObjectPtr<T>& Entry)
		{
			return !Entry.IsValid() || Entry.Get() == Actor;
		});
	}

	template <typename T>
	const TArray<TWeakObjectPtr<T>>& Compacted(TArray<TWeakObjectPtr<T>>& List)
	{
		List.RemoveAllSwap([](const TWeakObjectPtr<T>& Entry) { return !Entry.IsValid(); });
		return List;
	}
}

UDSTRActorRegistry* UDSTRActorRegistry::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UDSTRActorRegistry>() : nullptr;
}

void UDSTRActorRegistry::RegisterHero(ADediServerRPGCharacter* Hero)
{
	AddUniqueHandle(Heroes, Hero);
}

void UDSTRActorRegistry::UnregisterHero(ADediServerRPGCharacter* Hero)
{
	RemoveHandle(Heroes, Hero);
}

void UDSTRActorRegistry::RegisterEnemy(ADSTREnemyCharacter* Enemy)
{
	AddUniqueHandle(Enemies, Enemy);
}

void UDSTRActorRegistry::UnregisterEnemy(ADSTREnemyCharacter* Enemy)
{
	RemoveHandle(Enemies, Enemy);
}

void UDSTRActorRegistry::RegisterPickup(ADSTRAttackBuffPickup* Pickup)
{
	AddUniqueHandle(Pickups, Pickup);
}

void UDSTRActorRegistry::UnregisterPickup(ADSTRAttackBuffPickup* Pickup)
{
	RemoveHandle(Pickups, Pickup);
}

const TArray<TWeakObjectPtr<ADediServerRPGCharacter>>& UDSTRActorRegistry::GetHeroes() const
{
	return Compacted(Heroes);
}

const TArray<TWeakObjectPtr<ADSTREnemyCharacter>>& UDSTRActorRegistry::GetEnemies() const
{
	return Compacted(Enemies);
}

const TArray<TWeakObjectPtr<ADSTRAttackBuffPickup>>& UDSTRActorRegistry::GetPickups() const
{
	return Compacted(Pickups);
}
