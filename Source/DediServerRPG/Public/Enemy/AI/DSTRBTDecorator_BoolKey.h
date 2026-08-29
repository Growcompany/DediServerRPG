#pragma once

#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "DSTRBTDecorator_BoolKey.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRBTDecorator_BoolKey : public UBTDecorator_BlackboardBase
{
	GENERATED_BODY()

public:
	UDSTRBTDecorator_BoolKey();

	void Configure(FName KeyName, bool bExpected);

protected:
	virtual bool CalculateRawConditionValue(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;

private:
	UPROPERTY()
	bool bExpectedValue = true;
};
