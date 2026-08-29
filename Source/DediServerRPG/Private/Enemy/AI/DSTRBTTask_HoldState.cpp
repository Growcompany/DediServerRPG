#include "Enemy/AI/DSTRBTTask_HoldState.h"

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Enemy/AI/DSTRAIBlackboardKeys.h"
#include "Enemy/DSTRAIController.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

UDSTRBTTask_HoldState::UDSTRBTTask_HoldState()
{
	NodeName = TEXT("Hold State");
}

void UDSTRBTTask_HoldState::Configure(
	const FString& InNodeName,
	const EDSTREnemyAIState InState,
	const bool bInStopMovement,
	const bool bInStopImmediately,
	const bool bInFocusTarget)
{
	NodeName = InNodeName;
	State = InState;
	bStopMovement = bInStopMovement;
	bStopImmediately = bInStopImmediately;
	bFocusTarget = bInFocusTarget;
}

EBTNodeResult::Type UDSTRBTTask_HoldState::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	ADSTRAIController* Controller = Cast<ADSTRAIController>(OwnerComp.GetAIOwner());
	ADSTREnemyCharacter* Enemy = Controller
		? Cast<ADSTREnemyCharacter>(Controller->GetPawn())
		: nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Controller || !Enemy || !Enemy->HasAuthority() || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	Controller->SetAIState(State);
	if (bStopMovement)
	{
		Controller->StopMovement();
	}
	if (bStopImmediately)
	{
		Enemy->GetCharacterMovement()->StopMovementImmediately();
	}

	AActor* Target = Cast<AActor>(
		Blackboard->GetValueAsObject(DSTRAIBlackboardKeys::TargetActor));
	if (bFocusTarget && Target)
	{
		Controller->SetFocus(Target);
	}
	else
	{
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
	}
	return EBTNodeResult::InProgress;
}
