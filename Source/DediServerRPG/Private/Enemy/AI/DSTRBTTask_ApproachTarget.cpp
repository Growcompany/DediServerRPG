#include "Enemy/AI/DSTRBTTask_ApproachTarget.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "DSTRLog.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Enemy/AI/DSTRAIBlackboardKeys.h"
#include "Enemy/DSTRAIController.h"
#include "Enemy/DSTREnemyAIRules.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "Navigation/PathFollowingComponent.h"

UDSTRBTTask_ApproachTarget::UDSTRBTTask_ApproachTarget()
{
	NodeName = TEXT("Approach");
	INIT_TASK_NODE_NOTIFY_FLAGS();
}

bool UDSTRBTTask_ApproachTarget::RequestPath(
	AAIController& Controller,
	FDSTRBTApproachMemory& Memory,
	ADSTREnemyCharacter& Enemy,
	ADediServerRPGCharacter& Target)
{
	const FVector Goal = Target.GetActorLocation();
	const FAIMoveRequest Request = ADSTRAIController::MakeApproachMoveRequest(
		&Target,
		Enemy.GetAttackRange());
	const float StopDistance = FDSTREnemyAIRules::MoveStopDistance(
		Request.GetAcceptanceRadius(),
		Enemy.GetCapsuleComponent()->GetScaledCapsuleRadius(),
		Target.GetCapsuleComponent()->GetScaledCapsuleRadius(),
		Request.IsReachTestIncludingAgentRadius(),
		Request.IsReachTestIncludingGoalRadius());
	Memory.RequestedGoal = Goal;
	if (StopDistance >= Enemy.GetAttackRange())
	{
		Memory.bMoveRequestAccepted = false;
		UE_LOG(LogDSTR, Warning,
			TEXT("DSTR_AI_PATH_REJECTED Enemy=%s Stop=%.0f Engage=%.0f"),
			*Enemy.GetName(),
			StopDistance,
			Enemy.GetAttackRange());
		return false;
	}

	const FPathFollowingRequestResult Result = Controller.MoveTo(Request);
	Memory.bMoveRequestAccepted = Result.Code != EPathFollowingRequestResult::Failed;
	return Memory.bMoveRequestAccepted;
}

void UDSTRBTTask_ApproachTarget::EnterDirectMode(
	UBehaviorTreeComponent& OwnerComp,
	FDSTRBTApproachMemory& Memory,
	ADSTREnemyCharacter& Enemy,
	ADediServerRPGCharacter& Target)
{
	ADSTRAIController* Controller = Cast<ADSTRAIController>(OwnerComp.GetAIOwner());
	if (!Controller)
	{
		return;
	}
	Memory.bDirectSteer = true;
	Memory.bMoveRequestAccepted = false;
	Memory.ModeEnteredTime = Enemy.GetWorld()->GetTimeSeconds();
	Controller->StopMovement();
	Controller->SetFocus(&Target);
	Controller->SetAIState(EDSTREnemyAIState::DirectSteer);
}

void UDSTRBTTask_ApproachTarget::EnterPathMode(
	UBehaviorTreeComponent& OwnerComp,
	FDSTRBTApproachMemory& Memory,
	ADSTREnemyCharacter& Enemy,
	ADediServerRPGCharacter& Target)
{
	ADSTRAIController* Controller = Cast<ADSTRAIController>(OwnerComp.GetAIOwner());
	if (!Controller)
	{
		return;
	}
	Memory.bDirectSteer = false;
	Memory.bMoveRequestAccepted = false;
	Memory.ModeEnteredTime = Enemy.GetWorld()->GetTimeSeconds();
	Controller->ClearFocus(EAIFocusPriority::Gameplay);
	Controller->SetAIState(EDSTREnemyAIState::Approach);
	if (!RequestPath(*Controller, Memory, Enemy, Target))
	{
		EnterDirectMode(OwnerComp, Memory, Enemy, Target);
	}
}

EBTNodeResult::Type UDSTRBTTask_ApproachTarget::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	ADSTRAIController* Controller = Cast<ADSTRAIController>(OwnerComp.GetAIOwner());
	ADSTREnemyCharacter* Enemy = Controller
		? Cast<ADSTREnemyCharacter>(Controller->GetPawn())
		: nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	ADediServerRPGCharacter* Target = Blackboard
		? Cast<ADediServerRPGCharacter>(
			Blackboard->GetValueAsObject(DSTRAIBlackboardKeys::TargetActor))
		: nullptr;
	if (!Controller || !Enemy || !Enemy->HasAuthority() || !Target)
	{
		return EBTNodeResult::Failed;
	}

	FDSTRBTApproachMemory& Memory =
		*reinterpret_cast<FDSTRBTApproachMemory*>(NodeMemory);
	Memory = FDSTRBTApproachMemory();
	if (FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation())
		<= FDSTREnemyAIRules::DirectSteerRange)
	{
		EnterDirectMode(OwnerComp, Memory, *Enemy, *Target);
	}
	else
	{
		EnterPathMode(OwnerComp, Memory, *Enemy, *Target);
	}
	return EBTNodeResult::InProgress;
}

void UDSTRBTTask_ApproachTarget::TickTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	const float DeltaSeconds)
{
	ADSTRAIController* Controller = Cast<ADSTRAIController>(OwnerComp.GetAIOwner());
	ADSTREnemyCharacter* Enemy = Controller
		? Cast<ADSTREnemyCharacter>(Controller->GetPawn())
		: nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	ADediServerRPGCharacter* Target = Blackboard
		? Cast<ADediServerRPGCharacter>(
			Blackboard->GetValueAsObject(DSTRAIBlackboardKeys::TargetActor))
		: nullptr;
	if (!Controller || !Enemy || !Enemy->HasAuthority() || !Target || Target->IsDowned())
	{
		if (Controller)
		{
			Controller->StopMovement();
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	FDSTRBTApproachMemory& Memory =
		*reinterpret_cast<FDSTRBTApproachMemory*>(NodeMemory);
	const FVector ToTarget = Target->GetActorLocation() - Enemy->GetActorLocation();
	const float Distance2D = ToTarget.Size2D();
	const float Speed2D = Enemy->GetVelocity().Size2D();
	const float TimeInMode = Enemy->GetWorld()->GetTimeSeconds() - Memory.ModeEnteredTime;

	if (Memory.bDirectSteer)
	{
		Enemy->AddMovementInput(ToTarget.GetSafeNormal2D(), 1.0f);
		const bool bStuckForFullWindow =
			Speed2D < FDSTREnemyAIRules::StuckSpeed
			&& TimeInMode >= FDSTREnemyAIRules::StuckSeconds;
		const bool bOutsideAndNotClosing =
			Distance2D > FDSTREnemyAIRules::DirectSteerRange
			&& Speed2D < FDSTREnemyAIRules::StuckSpeed
			&& TimeInMode >= FDSTREnemyAIRules::DirectSteerHoldSeconds;
		if (bStuckForFullWindow || bOutsideAndNotClosing)
		{
			EnterPathMode(OwnerComp, Memory, *Enemy, *Target);
		}
		return;
	}

	if (!Memory.bMoveRequestAccepted
		|| FDSTREnemyAIRules::ShouldRepath(
			Memory.RequestedGoal,
			Target->GetActorLocation()))
	{
		if (!RequestPath(*Controller, Memory, *Enemy, *Target))
		{
			EnterDirectMode(OwnerComp, Memory, *Enemy, *Target);
			return;
		}
	}
	const bool bCloseEnoughForDirect =
		Distance2D <= FDSTREnemyAIRules::DirectSteerRange
		&& TimeInMode >= FDSTREnemyAIRules::DirectSteerHoldSeconds;
	const bool bPathStuck =
		Speed2D < FDSTREnemyAIRules::StuckSpeed
		&& TimeInMode >= FDSTREnemyAIRules::StuckSeconds;
	if (bCloseEnoughForDirect || bPathStuck)
	{
		EnterDirectMode(OwnerComp, Memory, *Enemy, *Target);
	}
}

EBTNodeResult::Type UDSTRBTTask_ApproachTarget::AbortTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	if (AAIController* Controller = OwnerComp.GetAIOwner())
	{
		Controller->StopMovement();
	}
	return EBTNodeResult::Aborted;
}
