#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AITypes.h"
#include "Enemy/DSTREnemyAIRules.h"
#include "DSTRAIController.generated.h"

class ADediServerRPGCharacter;
class ADSTREnemyCharacter;
class UBehaviorTree;

UCLASS()
class DEDISERVERRPG_API ADSTRAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADSTRAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	EDSTREnemyAIState GetAIState() const;
	void SetAIState(EDSTREnemyAIState NewState);

	static FAIMoveRequest MakeApproachMoveRequest(const AActor* Goal, float EngageRange);

private:
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TSoftObjectPtr<UBehaviorTree> BehaviorTreeAsset;
};
