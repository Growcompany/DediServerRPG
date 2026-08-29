#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "DSTRBTTask_ApproachTarget.generated.h"

class AAIController;
class ADediServerRPGCharacter;
class ADSTREnemyCharacter;

struct FDSTRBTApproachMemory
{
	FVector RequestedGoal = FVector::ZeroVector;
	float ModeEnteredTime = 0.0f;
	bool bMoveRequestAccepted = false;
	bool bDirectSteer = false;
};

UCLASS()
class DEDISERVERRPG_API UDSTRBTTask_ApproachTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UDSTRBTTask_ApproachTarget();

protected:
	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	virtual void TickTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override
	{
		return static_cast<uint16>(sizeof(FDSTRBTApproachMemory));
	}

private:
	static void EnterPathMode(
		UBehaviorTreeComponent& OwnerComp,
		FDSTRBTApproachMemory& Memory,
		ADSTREnemyCharacter& Enemy,
		ADediServerRPGCharacter& Target);
	static void EnterDirectMode(
		UBehaviorTreeComponent& OwnerComp,
		FDSTRBTApproachMemory& Memory,
		ADSTREnemyCharacter& Enemy,
		ADediServerRPGCharacter& Target);
	static bool RequestPath(
		AAIController& Controller,
		FDSTRBTApproachMemory& Memory,
		ADSTREnemyCharacter& Enemy,
		ADediServerRPGCharacter& Target);
};
