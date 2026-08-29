#include "Enemy/DSTRAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AISystem.h"
#include "DSTRLog.h"
#include "Enemy/AI/DSTRAIBlackboardKeys.h"
#include "Enemy/DSTREnemyCharacter.h"

ADSTRAIController::ADSTRAIController()
{
	// 포커스 회전 갱신을 위해 컨트롤러 틱을 유지한다.
	PrimaryActorTick.bCanEverTick = true;
	Blackboard = CreateDefaultSubobject<UBlackboardComponent>(
		TEXT("BlackboardComponent"));
	BrainComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(
		TEXT("BehaviorTreeComponent"));
	BehaviorTreeAsset = TSoftObjectPtr<UBehaviorTree>(FSoftObjectPath(
		TEXT("/Game/DediServerRPG/AI/BT_DSTR_Enemy.BT_DSTR_Enemy")));
}

void ADSTRAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ADSTREnemyCharacter* Enemy = Cast<ADSTREnemyCharacter>(InPawn);
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}
	if (!UAISystem::GetCurrentSafe(GetWorld()))
	{
		return;
	}

	UBehaviorTree* Tree = BehaviorTreeAsset.LoadSynchronous();
	UBlackboardComponent* InitializedBlackboard = nullptr;
	const bool bBlackboardReady = Tree
		&& Tree->BlackboardAsset
		&& UseBlackboard(Tree->BlackboardAsset, InitializedBlackboard);
	const bool bStartRequested = bBlackboardReady && RunBehaviorTree(Tree);
	UBehaviorTreeComponent* TreeComponent =
		Cast<UBehaviorTreeComponent>(BrainComponent);
	// 요청 성공뿐 아니라 실제 루트 실행까지 확인한다.
	const bool bTreeRunning = bStartRequested
		&& TreeComponent
		&& TreeComponent->IsRunning()
		&& TreeComponent->GetRootTree() == Tree;
	if (!bTreeRunning)
	{
		if (TreeComponent)
		{
			TreeComponent->StopTree(EBTStopMode::Safe);
		}
		StopMovement();
		UE_LOG(LogDSTR, Error,
			TEXT("DSTR_AI_TREE_START_FAILED Enemy=%s Tree=%s"),
			*Enemy->GetName(),
			*GetNameSafe(Tree));
		return;
	}

	InitializedBlackboard->SetValueAsEnum(
		DSTRAIBlackboardKeys::AIState,
		static_cast<uint8>(EDSTREnemyAIState::Idle));
	UE_LOG(LogDSTR, Log, TEXT("DSTR_AI_TREE_STARTED Enemy=%s Tree=%s"),
		*Enemy->GetName(),
		*Tree->GetPathName());
}

void ADSTRAIController::OnUnPossess()
{
	if (UBehaviorTreeComponent* TreeComponent =
		Cast<UBehaviorTreeComponent>(BrainComponent))
	{
		TreeComponent->StopTree(EBTStopMode::Safe);
	}
	StopMovement();
	Super::OnUnPossess();
}

EDSTREnemyAIState ADSTRAIController::GetAIState() const
{
	const UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();
	return BlackboardComponent
		? static_cast<EDSTREnemyAIState>(BlackboardComponent->GetValueAsEnum(
			DSTRAIBlackboardKeys::AIState))
		: EDSTREnemyAIState::Idle;
}

void ADSTRAIController::SetAIState(const EDSTREnemyAIState NewState)
{
	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();
	ADSTREnemyCharacter* Enemy = Cast<ADSTREnemyCharacter>(GetPawn());
	if (!BlackboardComponent || !Enemy)
	{
		return;
	}

	const EDSTREnemyAIState Previous = GetAIState();
	if (Previous == NewState)
	{
		return;
	}
	const float Distance = BlackboardComponent->GetValueAsFloat(
		DSTRAIBlackboardKeys::Distance2D);
	BlackboardComponent->SetValueAsEnum(
		DSTRAIBlackboardKeys::AIState,
		static_cast<uint8>(NewState));
	UE_LOG(LogDSTR, Log, TEXT("DSTR_AI_STATE Enemy=%s From=%s To=%s Dist=%.0f"),
		*Enemy->GetName(),
		FDSTREnemyAIRules::ToString(Previous),
		FDSTREnemyAIRules::ToString(NewState),
		Distance);
}

FAIMoveRequest ADSTRAIController::MakeApproachMoveRequest(
	const AActor* Goal,
	const float EngageRange)
{
	FAIMoveRequest MoveRequest(Goal);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);
	MoveRequest.SetAcceptanceRadius(
		FDSTREnemyAIRules::ApproachAcceptanceRadius(EngageRange));
	MoveRequest.SetReachTestIncludesAgentRadius(false);
	MoveRequest.SetReachTestIncludesGoalRadius(false);
	MoveRequest.SetCanStrafe(false);
	return MoveRequest;
}
