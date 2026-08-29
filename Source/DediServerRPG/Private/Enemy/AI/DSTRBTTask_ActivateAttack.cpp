#include "Enemy/AI/DSTRBTTask_ActivateAttack.h"

#include "AbilitySystem/DSTRAbilitySystemComponent.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "AbilitySystem/Abilities/DSTRBossColossalBlowAbility.h"
#include "AbilitySystem/Abilities/DSTRBossPhantomRushAbility.h"
#include "AbilitySystem/Abilities/DSTRBossSiphonAbility.h"
#include "AbilitySystem/Abilities/DSTRBossSubjugateAbility.h"
#include "AbilitySystem/Abilities/DSTREnemyAttackAbility.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DSTRLog.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Enemy/AI/DSTRAIBlackboardKeys.h"
#include "Enemy/DSTRAIController.h"
#include "Enemy/DSTRBossSkillRules.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

UDSTRBTTask_ActivateAttack::UDSTRBTTask_ActivateAttack()
{
	NodeName = TEXT("MeleeAttack");
	INIT_TASK_NODE_NOTIFY_FLAGS();
}

void UDSTRBTTask_ActivateAttack::Configure(
	const bool bInUseRecommendedBossSkill)
{
	bUseRecommendedBossSkill = bInUseRecommendedBossSkill;
	NodeName = bUseRecommendedBossSkill ? TEXT("BossSkill") : TEXT("MeleeAttack");
}

TSubclassOf<UGameplayAbility> UDSTRBTTask_ActivateAttack::AbilityForSkill(
	const EDSTRBossSkill Skill)
{
	switch (Skill)
	{
	case EDSTRBossSkill::Swing:
		return UDSTREnemyAttackAbility::StaticClass();
	case EDSTRBossSkill::ColossalBlow:
		return UDSTRBossColossalBlowAbility::StaticClass();
	case EDSTRBossSkill::PhantomRush:
		return UDSTRBossPhantomRushAbility::StaticClass();
	case EDSTRBossSkill::Siphon:
		return UDSTRBossSiphonAbility::StaticClass();
	case EDSTRBossSkill::Subjugate:
		return UDSTRBossSubjugateAbility::StaticClass();
	case EDSTRBossSkill::None:
	default:
		return nullptr;
	}
}

EBTNodeResult::Type UDSTRBTTask_ActivateAttack::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	ADSTRAIController* Controller = Cast<ADSTRAIController>(OwnerComp.GetAIOwner());
	ADSTREnemyCharacter* Enemy = Controller
		? Cast<ADSTREnemyCharacter>(Controller->GetPawn())
		: nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	UDSTRAbilitySystemComponent* ASC = Enemy
		? Enemy->GetDSTRAbilitySystemComponent()
		: nullptr;
	ADediServerRPGCharacter* Target = Blackboard
		? Cast<ADediServerRPGCharacter>(
			Blackboard->GetValueAsObject(DSTRAIBlackboardKeys::TargetActor))
		: nullptr;
	if (!Controller || !Enemy || !Enemy->HasAuthority() || !Blackboard || !ASC || !Target)
	{
		return EBTNodeResult::Failed;
	}

	const EDSTRBossSkill Skill = bUseRecommendedBossSkill
		? static_cast<EDSTRBossSkill>(
			Blackboard->GetValueAsEnum(DSTRAIBlackboardKeys::RecommendedBossSkill))
		: EDSTRBossSkill::Swing;
	TSubclassOf<UGameplayAbility> AbilityClass =
		UDSTREnemyAttackAbility::StaticClass();
	if (bUseRecommendedBossSkill)
	{
		AbilityClass = AbilityForSkill(Skill);
	}
	if (!AbilityClass)
	{
		return EBTNodeResult::Failed;
	}

	Controller->StopMovement();
	Enemy->GetCharacterMovement()->StopMovementImmediately();
	Controller->SetFocus(Target);
	if (!ASC->TryActivateAbilityByClass(AbilityClass))
	{
		return EBTNodeResult::Failed;
	}

	Controller->SetAIState(EDSTREnemyAIState::Attack);
	Blackboard->SetValueAsBool(DSTRAIBlackboardKeys::IsAttacking, true);
	if (bUseRecommendedBossSkill)
	{
		UE_LOG(LogDSTR, Log, TEXT("DSTR_BOSS_SKILL Enemy=%s Skill=%s Dist=%.0f"),
			*Enemy->GetName(),
			FDSTRBossSkillRules::ToString(Skill),
			Blackboard->GetValueAsFloat(DSTRAIBlackboardKeys::Distance2D));
	}
	return EBTNodeResult::InProgress;
}

void UDSTRBTTask_ActivateAttack::TickTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	const float DeltaSeconds)
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	const ADSTREnemyCharacter* Enemy = Controller
		? Cast<ADSTREnemyCharacter>(Controller->GetPawn())
		: nullptr;
	const UDSTRAbilitySystemComponent* ASC = Enemy
		? Enemy->GetDSTRAbilitySystemComponent()
		: nullptr;
	if (!ASC
		|| !ASC->HasMatchingGameplayTag(DSTRGameplayTags::State_Attacking.GetTag()))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UDSTRBTTask_ActivateAttack::AbortTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	if (AAIController* Controller = OwnerComp.GetAIOwner())
	{
		Controller->StopMovement();
	}
	return EBTNodeResult::Aborted;
}
