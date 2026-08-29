#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "Enemy/DSTREnemyAIRules.h"
#include "DSTRBTTask_HoldState.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRBTTask_HoldState : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UDSTRBTTask_HoldState();

	void Configure(
		const FString& InNodeName,
		EDSTREnemyAIState InState,
		bool bInStopMovement,
		bool bInStopImmediately,
		bool bInFocusTarget);

protected:
	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

private:
	UPROPERTY()
	EDSTREnemyAIState State = EDSTREnemyAIState::Idle;

	UPROPERTY()
	bool bStopMovement = true;

	UPROPERTY()
	bool bStopImmediately = true;

	UPROPERTY()
	bool bFocusTarget = false;
};
