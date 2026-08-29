#include "Enemy/AI/DSTRBTDecorator_BoolKey.h"

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UDSTRBTDecorator_BoolKey::UDSTRBTDecorator_BoolKey()
{
	NodeName = TEXT("DSTR Bool");
	BlackboardKey.AddBoolFilter(
		this,
		GET_MEMBER_NAME_CHECKED(UDSTRBTDecorator_BoolKey, BlackboardKey));
	FlowAbortMode = EBTFlowAbortMode::Both;
}

void UDSTRBTDecorator_BoolKey::Configure(
	const FName KeyName,
	const bool bExpected)
{
	BlackboardKey.SelectedKeyName = KeyName;
	bExpectedValue = bExpected;
	NodeName = FString::Printf(
		TEXT("%s is %s"),
		*KeyName.ToString(),
		bExpected ? TEXT("true") : TEXT("false"));
}

bool UDSTRBTDecorator_BoolKey::CalculateRawConditionValue(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory) const
{
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	return Blackboard
		&& Blackboard->GetValueAsBool(BlackboardKey.SelectedKeyName) == bExpectedValue;
}

FString UDSTRBTDecorator_BoolKey::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("%s is %s"),
		*BlackboardKey.SelectedKeyName.ToString(),
		bExpectedValue ? TEXT("true") : TEXT("false"));
}
