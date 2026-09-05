#include "Enemy/AI/DSTRBTService_CombatContext.h"

#include "AIController.h"
#include "AbilitySystem/DSTRAbilitySystemComponent.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DSTRLog.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Enemy/AI/DSTRAIBlackboardKeys.h"
#include "Enemy/DSTRBossSkillRules.h"
#include "Enemy/DSTREnemyAIRules.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "EngineUtils.h"
#include "Game/DSTRDungeonRules.h"

UDSTRBTService_CombatContext::UDSTRBTService_CombatContext()
{
	NodeName = TEXT("Combat Context");
	Interval = FDSTREnemyAIRules::DecisionInterval;
	RandomDeviation = 0.0f;
	bCallTickOnSearchStart = true;
}

ADediServerRPGCharacter* UDSTRBTService_CombatContext::SelectTarget(
	ADSTREnemyCharacter& Enemy,
	ADediServerRPGCharacter* Current)
{
	UWorld* World = Enemy.GetWorld();
	if (!World)
	{
		return nullptr;
	}
	if (Current && Current->IsDowned())
	{
		Current = nullptr;
	}

	const FVector EnemyLocation = Enemy.GetActorLocation();
	auto ScoreOf = [&Enemy, &EnemyLocation](const ADediServerRPGCharacter* Candidate)
	{
		return FDSTREnemyAIRules::ThreatScore(
			FVector::Dist2D(Candidate->GetActorLocation(), EnemyLocation),
			Enemy.GetThreat(Candidate));
	};

	ADediServerRPGCharacter* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (TActorIterator<ADediServerRPGCharacter> It(World); It; ++It)
	{
		ADediServerRPGCharacter* Candidate = *It;
		if (!Candidate || Candidate->IsDowned())
		{
			continue;
		}
		const float Score = ScoreOf(Candidate);
		if (Score < BestScore)
		{
			Best = Candidate;
			BestScore = Score;
		}
	}

	if (Current && Best && Best != Current)
	{
		const float CurrentDistance = FVector::Dist2D(
			Current->GetActorLocation(), EnemyLocation);
		if (!FDSTREnemyAIRules::ShouldSwitchTarget(
			ScoreOf(Current),
			BestScore,
			CurrentDistance,
			Enemy.GetAttackRange()))
		{
			return Current;
		}
	}
	else if (Current)
	{
		return Current;
	}

	if (Best && Best != Current)
	{
		UE_LOG(LogDSTR, Log, TEXT("DSTR_AI_TARGET Enemy=%s Target=%s Dist=%.0f"),
			*Enemy.GetName(),
			*Best->GetName(),
			FVector::Dist2D(Best->GetActorLocation(), EnemyLocation));
	}
	return Best;
}

int32 UDSTRBTService_CombatContext::CountCrowdAround(
	const ADediServerRPGCharacter* Target)
{
	UWorld* World = Target ? Target->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}

	int32 Count = 0;
	for (TActorIterator<ADediServerRPGCharacter> It(World); It; ++It)
	{
		const ADediServerRPGCharacter* Candidate = *It;
		if (Candidate
			&& !Candidate->IsDowned()
			&& FVector::Dist2D(
				Candidate->GetActorLocation(), Target->GetActorLocation())
				<= FDSTRBossSkillRules::SubjugateCrowdRadius)
		{
			++Count;
		}
	}
	return Count;
}

EDSTRBossSkill UDSTRBTService_CombatContext::RecommendBossSkill(
	const ADSTREnemyCharacter& Enemy,
	const ADediServerRPGCharacter* Target,
	const float Distance2D)
{
	const UDSTRAbilitySystemComponent* ASC = Enemy.GetDSTRAbilitySystemComponent();
	if (!ASC || !Target)
	{
		return EDSTRBossSkill::None;
	}

	auto Ready = [ASC](const FGameplayTag& CooldownTag)
	{
		return !ASC->HasMatchingGameplayTag(CooldownTag);
	};
	FDSTRBossSkillInput Input;
	Input.bHasTarget = true;
	Input.Distance2D = Distance2D;
	Input.AttackRange = Enemy.GetAttackRange();
	Input.bSwingReady = Ready(DSTRGameplayTags::Effect_Cooldown_EnemyAttack.GetTag());
	Input.bColossalReady = Ready(DSTRGameplayTags::Effect_Cooldown_BossColossal.GetTag());
	Input.bRushReady = Ready(DSTRGameplayTags::Effect_Cooldown_BossRush.GetTag());
	Input.bSiphonReady = Ready(DSTRGameplayTags::Effect_Cooldown_BossSiphon.GetTag());
	Input.bSubjugateReady = Ready(DSTRGameplayTags::Effect_Cooldown_BossSubjugate.GetTag());
	Input.SwingsSinceColossal = Enemy.GetSwingsSinceColossal();
	Input.SwingsSinceSubjugate = Enemy.GetSwingsSinceSubjugate();
	Input.PlayersInCrowdRadius = CountCrowdAround(Target);
	return FDSTRBossSkillRules::Select(Input);
}

void UDSTRBTService_CombatContext::TickNode(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	const float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* Controller = OwnerComp.GetAIOwner();
	ADSTREnemyCharacter* Enemy = Controller
		? Cast<ADSTREnemyCharacter>(Controller->GetPawn())
		: nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	// 감지는 서버 전용. 클라는 블랙보드 갱신 불가
	if (!Enemy || !Enemy->HasAuthority() || !Blackboard)
	{
		return;
	}

	if (Enemy->IsDormant() && !Enemy->IsBoss())
	{
		const ADediServerRPGCharacter* Nearest = Enemy->FindNearestLivingPlayer();
		const float Distance = Nearest
			? FVector::Dist2D(Nearest->GetActorLocation(), Enemy->GetActorLocation())
			: TNumericLimits<float>::Max();
		if (FDSTRDungeonRules::ShouldWake(
			true,
			Distance,
			FDSTRDungeonRules::AmbushAggroRange))
		{
			Enemy->Wake(TEXT("Proximity"));
		}
	}

	ADediServerRPGCharacter* Current = Cast<ADediServerRPGCharacter>(
		Blackboard->GetValueAsObject(DSTRAIBlackboardKeys::TargetActor));
	ADediServerRPGCharacter* Target = Enemy->IsCombatantDead() || Enemy->IsDormant()
		? nullptr
		: SelectTarget(*Enemy, Current);
	const FVector ToTarget = Target
		? Target->GetActorLocation() - Enemy->GetActorLocation()
		: FVector::ZeroVector;
	// 대상이 없을 때는 최댓값으로 두어 공격 분기를 막는다.
	const float Distance2D = Target
		? ToTarget.Size2D()
		: TNumericLimits<float>::Max();
	UDSTRAbilitySystemComponent* ASC = Enemy->GetDSTRAbilitySystemComponent();
	const bool bAttacking = ASC
		&& ASC->HasMatchingGameplayTag(DSTRGameplayTags::State_Attacking.GetTag());
	const bool bAttackReady = !ASC
		|| !ASC->HasMatchingGameplayTag(
			DSTRGameplayTags::Effect_Cooldown_EnemyAttack.GetTag());
	const EDSTREnemyAIState State = static_cast<EDSTREnemyAIState>(
		Blackboard->GetValueAsEnum(DSTRAIBlackboardKeys::AIState));
	const bool bWasEngaged = State == EDSTREnemyAIState::Attack
		|| State == EDSTREnemyAIState::Recover;
	const float AttackBand = Enemy->GetAttackRange()
		+ (bWasEngaged ? FDSTREnemyAIRules::AttackExitMargin : 0.0f);
	const EDSTRBossSkill Recommended = Enemy->IsBoss() && Target
		? RecommendBossSkill(*Enemy, Target, Distance2D)
		: EDSTRBossSkill::None;

	// 서비스는 감지값만 갱신. 우선순위 결정은 트리
	Blackboard->SetValueAsObject(DSTRAIBlackboardKeys::TargetActor, Target);
	Blackboard->SetValueAsBool(DSTRAIBlackboardKeys::HasTarget, Target != nullptr);
	Blackboard->SetValueAsFloat(DSTRAIBlackboardKeys::Distance2D, Distance2D);
	Blackboard->SetValueAsBool(
		DSTRAIBlackboardKeys::InAttackBand,
		Target && Distance2D <= AttackBand);
	Blackboard->SetValueAsBool(DSTRAIBlackboardKeys::AttackReady, bAttackReady);
	Blackboard->SetValueAsBool(
		DSTRAIBlackboardKeys::FacingTarget,
		!Target || FDSTREnemyAIRules::IsFacingTarget(
			Enemy->GetActorForwardVector(), ToTarget));
	Blackboard->SetValueAsBool(
		DSTRAIBlackboardKeys::IsDead,
		Enemy->IsCombatantDead());
	Blackboard->SetValueAsBool(
		DSTRAIBlackboardKeys::IsDormant,
		Enemy->IsDormant());
	Blackboard->SetValueAsBool(
		DSTRAIBlackboardKeys::IsStunned,
		Enemy->IsStunned());
	Blackboard->SetValueAsBool(DSTRAIBlackboardKeys::IsAttacking, bAttacking);
	Blackboard->SetValueAsBool(
		DSTRAIBlackboardKeys::IsPreparingArea,
		Enemy->IsPreparingAreaAttack());
	Blackboard->SetValueAsBool(
		DSTRAIBlackboardKeys::IsRushing,
		Enemy->IsRushing());
	Blackboard->SetValueAsBool(
		DSTRAIBlackboardKeys::IsSiphoning,
		Enemy->IsSiphoning());
	Blackboard->SetValueAsBool(DSTRAIBlackboardKeys::IsBoss, Enemy->IsBoss());
	Blackboard->SetValueAsEnum(
		DSTRAIBlackboardKeys::RecommendedBossSkill,
		static_cast<uint8>(Recommended));
	Blackboard->SetValueAsBool(
		DSTRAIBlackboardKeys::HasRecommendedBossSkill,
		Recommended != EDSTRBossSkill::None);
}
