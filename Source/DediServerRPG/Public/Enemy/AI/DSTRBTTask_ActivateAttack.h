#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "DSTRBTTask_ActivateAttack.generated.h"

class UGameplayAbility;
enum class EDSTRBossSkill : uint8;

UCLASS()
class DEDISERVERRPG_API UDSTRBTTask_ActivateAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UDSTRBTTask_ActivateAttack();

	void Configure(bool bInUseRecommendedBossSkill);

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

private:
	static TSubclassOf<UGameplayAbility> AbilityForSkill(EDSTRBossSkill Skill);

	UPROPERTY()
	bool bUseRecommendedBossSkill = false;
};
