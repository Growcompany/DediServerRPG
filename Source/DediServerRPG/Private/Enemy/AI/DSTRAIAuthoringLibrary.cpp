#include "Enemy/AI/DSTRAIAuthoringLibrary.h"

#include "DSTRLog.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTreeGraph.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_BehaviorTree.h"
#include "Enemy/AI/DSTRAIBlackboardKeys.h"
#include "Enemy/AI/DSTRBTDecorator_BoolKey.h"
#include "Enemy/AI/DSTRBTService_CombatContext.h"
#include "Enemy/AI/DSTRBTTask_ActivateAttack.h"
#include "Enemy/AI/DSTRBTTask_ApproachTarget.h"
#include "Enemy/AI/DSTRBTTask_HoldState.h"
#include "Enemy/DSTRBossSkillRules.h"
#include "Enemy/DSTREnemyAIRules.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

namespace
{
	const FString BlackboardPackageName(TEXT("/Game/DediServerRPG/AI/BB_DSTR_Enemy"));
	const FString BehaviorTreePackageName(TEXT("/Game/DediServerRPG/AI/BT_DSTR_Enemy"));
	const FName BlackboardAssetName(TEXT("BB_DSTR_Enemy"));
	const FName BehaviorTreeAssetName(TEXT("BT_DSTR_Enemy"));

	struct FDSTRBoolCondition
	{
		FName Key;
		bool bExpected;
	};

	template <typename AssetType>
	AssetType* LoadOrCreateAsset(
		const FString& PackageName,
		const FName AssetName)
	{
		const FString ObjectPath = FString::Printf(
			TEXT("%s.%s"),
			*PackageName,
			*AssetName.ToString());
		if (FPackageName::DoesPackageExist(PackageName))
		{
			if (AssetType* Existing = LoadObject<AssetType>(nullptr, *ObjectPath))
			{
				return Existing;
			}
		}

		UPackage* Package = CreatePackage(*PackageName);
		AssetType* Asset = NewObject<AssetType>(
			Package,
			AssetName,
			RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	}

	void AddKey(
		UBlackboardData& Blackboard,
		const FName Name,
		UBlackboardKeyType* Type)
	{
		FBlackboardEntry Entry;
		Entry.EntryName = Name;
		Entry.KeyType = Type;
		Entry.bInstanceSynced = false;
		Blackboard.Keys.Add(MoveTemp(Entry));
	}

	void AddBoolKey(UBlackboardData& Blackboard, const FName Name)
	{
		AddKey(
			Blackboard,
			Name,
			NewObject<UBlackboardKeyType_Bool>(&Blackboard));
	}

	UDSTRBTTask_HoldState* NewHoldTask(
		UBehaviorTree& Tree,
		const TCHAR* Name,
		const EDSTREnemyAIState State,
		const bool bStopMovement,
		const bool bStopImmediately,
		const bool bFocusTarget)
	{
		UDSTRBTTask_HoldState* Task = NewObject<UDSTRBTTask_HoldState>(&Tree);
		Task->Configure(
			Name,
			State,
			bStopMovement,
			bStopImmediately,
			bFocusTarget);
		return Task;
	}

	void AddBranch(
		UBehaviorTree& Tree,
		UBTComposite_Selector& Root,
		UBTTaskNode* Task,
		const TArray<FDSTRBoolCondition>& Conditions)
	{
		FBTCompositeChild Child;
		Child.ChildTask = Task;
		for (const FDSTRBoolCondition& Condition : Conditions)
		{
			UDSTRBTDecorator_BoolKey* Decorator =
				NewObject<UDSTRBTDecorator_BoolKey>(&Tree);
			Decorator->Configure(Condition.Key, Condition.bExpected);
			Child.Decorators.Add(Decorator);
		}
		Child.DecoratorOps.Reset();
		Root.Children.Add(MoveTemp(Child));
	}

	bool BuildBlackboard(UBlackboardData& Blackboard)
	{
		// 키를 전부 다시 만들어 재실행 결과를 같게 한다.
		Blackboard.Modify();
		Blackboard.Parent = nullptr;
		Blackboard.Keys.Reset();

		UBlackboardKeyType_Object* TargetType =
			NewObject<UBlackboardKeyType_Object>(&Blackboard);
		TargetType->BaseClass = ADediServerRPGCharacter::StaticClass();
		AddKey(Blackboard, DSTRAIBlackboardKeys::TargetActor, TargetType);
		AddBoolKey(Blackboard, DSTRAIBlackboardKeys::HasTarget);

		UBlackboardKeyType_Enum* StateType =
			NewObject<UBlackboardKeyType_Enum>(&Blackboard);
		StateType->EnumType = StaticEnum<EDSTREnemyAIState>();
		StateType->EnumName = StateType->EnumType->GetName();
		StateType->bIsEnumNameValid = true;
		AddKey(Blackboard, DSTRAIBlackboardKeys::AIState, StateType);

		AddKey(
			Blackboard,
			DSTRAIBlackboardKeys::Distance2D,
			NewObject<UBlackboardKeyType_Float>(&Blackboard));
		AddBoolKey(Blackboard, DSTRAIBlackboardKeys::InAttackBand);
		AddBoolKey(Blackboard, DSTRAIBlackboardKeys::AttackReady);
		AddBoolKey(Blackboard, DSTRAIBlackboardKeys::FacingTarget);
		AddBoolKey(Blackboard, DSTRAIBlackboardKeys::IsDead);
		AddBoolKey(Blackboard, DSTRAIBlackboardKeys::IsDormant);
		AddBoolKey(Blackboard, DSTRAIBlackboardKeys::IsStunned);
		AddBoolKey(Blackboard, DSTRAIBlackboardKeys::IsAttacking);
		AddBoolKey(Blackboard, DSTRAIBlackboardKeys::IsPreparingArea);
		AddBoolKey(Blackboard, DSTRAIBlackboardKeys::IsRushing);
		AddBoolKey(Blackboard, DSTRAIBlackboardKeys::IsSiphoning);
		AddBoolKey(Blackboard, DSTRAIBlackboardKeys::IsBoss);

		UBlackboardKeyType_Enum* SkillType =
			NewObject<UBlackboardKeyType_Enum>(&Blackboard);
		SkillType->EnumType = StaticEnum<EDSTRBossSkill>();
		SkillType->EnumName = SkillType->EnumType->GetName();
		SkillType->bIsEnumNameValid = true;
		AddKey(
			Blackboard,
			DSTRAIBlackboardKeys::RecommendedBossSkill,
			SkillType);
		AddBoolKey(Blackboard, DSTRAIBlackboardKeys::HasRecommendedBossSkill);

		Blackboard.UpdateParentKeys();
		Blackboard.PropagateKeyChangesToDerivedBlackboardAssets();
		Blackboard.MarkPackageDirty();
		return Blackboard.Keys.Num() == 18 && Blackboard.IsValid();
	}

	bool BuildBehaviorTree(
		UBehaviorTree& Tree,
		UBlackboardData& Blackboard)
	{
		Tree.Modify();
		Tree.BlackboardAsset = &Blackboard;
		// 셀렉터 순서 = 행동 우선순위
		UBTComposite_Selector* Root = NewObject<UBTComposite_Selector>(&Tree);
		Tree.RootNode = Root;
		Root->Children.Reset();
		Root->Services.Reset();
		Root->Services.Add(NewObject<UDSTRBTService_CombatContext>(&Tree));

		// 첫 만족 분기 하나만 실행 → 생존 상태를 최상단에
		AddBranch(Tree, *Root,
			NewHoldTask(Tree, TEXT("Dead"), EDSTREnemyAIState::Dead, true, true, false),
			{{DSTRAIBlackboardKeys::IsDead, true}});
		AddBranch(Tree, *Root,
			NewHoldTask(Tree, TEXT("Stunned"), EDSTREnemyAIState::Stunned, true, true, true),
			{{DSTRAIBlackboardKeys::IsStunned, true}});
		AddBranch(Tree, *Root,
			NewHoldTask(Tree, TEXT("Dormant"), EDSTREnemyAIState::Idle, true, true, false),
			{{DSTRAIBlackboardKeys::IsDormant, true}});
		AddBranch(Tree, *Root,
			NewHoldTask(Tree, TEXT("Rushing"), EDSTREnemyAIState::Rush, true, false, false),
			{{DSTRAIBlackboardKeys::IsRushing, true}});
		AddBranch(Tree, *Root,
			NewHoldTask(Tree, TEXT("Siphoning"), EDSTREnemyAIState::Siphon, true, true, true),
			{{DSTRAIBlackboardKeys::IsSiphoning, true}});
		AddBranch(Tree, *Root,
			NewHoldTask(Tree, TEXT("PreparingArea"), EDSTREnemyAIState::Telegraph, true, true, true),
			{{DSTRAIBlackboardKeys::IsPreparingArea, true}});
		AddBranch(Tree, *Root,
			NewHoldTask(Tree, TEXT("Attacking"), EDSTREnemyAIState::Attack, true, true, true),
			{{DSTRAIBlackboardKeys::IsAttacking, true}});

		UDSTRBTTask_ActivateAttack* BossSkill =
			NewObject<UDSTRBTTask_ActivateAttack>(&Tree);
		BossSkill->Configure(true);
		AddBranch(Tree, *Root, BossSkill, {
			{DSTRAIBlackboardKeys::IsBoss, true},
			{DSTRAIBlackboardKeys::HasTarget, true},
			{DSTRAIBlackboardKeys::HasRecommendedBossSkill, true}});

		// 보스 분기 통과 후에만 일반 공격 조건 평가
		UDSTRBTTask_ActivateAttack* Melee =
			NewObject<UDSTRBTTask_ActivateAttack>(&Tree);
		Melee->Configure(false);
		AddBranch(Tree, *Root, Melee, {
			{DSTRAIBlackboardKeys::IsBoss, false},
			{DSTRAIBlackboardKeys::HasTarget, true},
			{DSTRAIBlackboardKeys::InAttackBand, true},
			{DSTRAIBlackboardKeys::AttackReady, true},
			{DSTRAIBlackboardKeys::FacingTarget, true}});

		AddBranch(Tree, *Root,
			NewHoldTask(Tree, TEXT("Recover"), EDSTREnemyAIState::Recover, true, true, true),
			{{DSTRAIBlackboardKeys::HasTarget, true},
			 {DSTRAIBlackboardKeys::InAttackBand, true}});
		AddBranch(Tree, *Root,
			NewObject<UDSTRBTTask_ApproachTarget>(&Tree),
			{{DSTRAIBlackboardKeys::HasTarget, true},
			 {DSTRAIBlackboardKeys::InAttackBand, false}});
		AddBranch(Tree, *Root,
			NewHoldTask(Tree, TEXT("Idle"), EDSTREnemyAIState::Idle, true, true, false),
			{});

		if (Tree.BTGraph)
		{
			Tree.BTGraph->Rename(
				nullptr,
				GetTransientPackage(),
				REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
			Tree.BTGraph = nullptr;
		}
		Tree.BTGraph = FBlueprintEditorUtils::CreateNewGraph(
			&Tree,
			TEXT("Behavior Tree"),
			UBehaviorTreeGraph::StaticClass(),
			UEdGraphSchema_BehaviorTree::StaticClass());
		const UEdGraphSchema* Schema = Tree.BTGraph
			? Tree.BTGraph->GetSchema()
			: nullptr;
		UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree.BTGraph);
		if (!Schema || !Graph)
		{
			return false;
		}
		Graph->LockUpdates();
		Schema->CreateDefaultNodesForGraph(*Graph);
		Graph->OnCreated();
		Graph->Initialize();
		Graph->UnlockUpdates();
		Tree.MarkPackageDirty();
		return Tree.BlackboardAsset == &Blackboard
			&& Tree.RootNode
			&& Tree.RootNode->Services.Num() == 1
			&& Tree.RootNode->Children.Num() == 12
			&& Tree.BTGraph;
	}
}

#endif

bool UDSTRAIAuthoringLibrary::CreateOrUpdateEnemyBehaviorTreeAssets()
{
#if WITH_EDITOR
	UBlackboardData* Blackboard = LoadOrCreateAsset<UBlackboardData>(
		BlackboardPackageName,
		BlackboardAssetName);
	UBehaviorTree* Tree = LoadOrCreateAsset<UBehaviorTree>(
		BehaviorTreePackageName,
		BehaviorTreeAssetName);
	const bool bSucceeded = Blackboard
		&& Tree
		&& BuildBlackboard(*Blackboard)
		&& BuildBehaviorTree(*Tree, *Blackboard);
	if (!bSucceeded)
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_BT_AUTHORING_FAILED"));
		return false;
	}

	UE_LOG(LogDSTR, Log,
		TEXT("DSTR_BT_AUTHORED Blackboard=%s Tree=%s Keys=%d Branches=%d"),
		*Blackboard->GetPathName(),
		*Tree->GetPathName(),
		Blackboard->Keys.Num(),
		Tree->RootNode->Children.Num());
	return true;
#else
	return false;
#endif
}
