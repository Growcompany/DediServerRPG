#pragma once

#include "BehaviorTree/BTService.h"
#include "DSTRBTService_CombatContext.generated.h"

class ADediServerRPGCharacter;
class ADSTREnemyCharacter;
enum class EDSTRBossSkill : uint8;

UCLASS()
class DEDISERVERRPG_API UDSTRBTService_CombatContext : public UBTService
{
	GENERATED_BODY()

public:
	UDSTRBTService_CombatContext();

protected:
	virtual void TickNode(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		float DeltaSeconds) override;

private:
	static ADediServerRPGCharacter* SelectTarget(
		ADSTREnemyCharacter& Enemy,
		ADediServerRPGCharacter* Current);
	static int32 CountCrowdAround(const ADediServerRPGCharacter* Target);
	static EDSTRBossSkill RecommendBossSkill(
		const ADSTREnemyCharacter& Enemy,
		const ADediServerRPGCharacter* Target,
		float Distance2D);
};
