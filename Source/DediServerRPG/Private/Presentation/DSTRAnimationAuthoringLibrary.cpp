#include "Presentation/DSTRAnimationAuthoringLibrary.h"
#include "DSTRLog.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Presentation/DSTRAnimNotifies.h"
#include "Presentation/DSTRCombatPresentation.h"

#if WITH_EDITOR
#include "AnimationBlueprintLibrary.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "Animation/AnimBlueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphNode_Comment.h"
#include "Factories/AnimMontageFactory.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

TArray<FName> UDSTRAnimationAuthoringLibrary::GetAnimBlueprintSlotNames(UClass* AnimClass)
{
	return FDSTRCombatPresentation::GetAnimClassSlotNames(AnimClass);
}

FName UDSTRAnimationAuthoringLibrary::GetFullBodySlotName()
{
	return FDSTRCombatPresentation::GetFullBodySlotName();
}

TArray<FString> UDSTRAnimationAuthoringLibrary::GetFullBodyMontageNames()
{
	TArray<FString> Names;
	const UEnum* ActionEnum = StaticEnum<EDSTRCombatAction>();
	if (!ActionEnum)
	{
		return Names;
	}
	for (int32 Index = 0; Index < ActionEnum->NumEnums() - 1; ++Index)
	{
		const FDSTRCombatActionProfile& Profile =
			FDSTRCombatPresentation::GetProfile(static_cast<EDSTRCombatAction>(ActionEnum->GetValueByIndex(Index)));
		if (!Profile.bFullBodySlot)
		{
			continue;
		}
		for (const FSoftObjectPath& Variant : Profile.Variants)
		{
			Names.AddUnique(Variant.GetAssetName());
		}
	}
	return Names;
}

#if WITH_EDITOR
namespace
{
	constexpr int32 LegacyPlayerCastCount = 4;
	constexpr int32 LegacyPlayerBranchNodeCount = 16;

	UEdGraphPin* FindPosePin(const UEdGraphNode& Node, const EEdGraphPinDirection Direction)
	{
		for (UEdGraphPin* Pin : Node.Pins)
		{
			if (Pin && Pin->Direction == Direction && UAnimationGraphSchema::IsPosePin(Pin->PinType))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	bool IsLegacyPlayerCharacterClass(const UClass* Class)
	{
		if (!Class)
		{
			return false;
		}
		static const TSet<FName> Packages = {
			FName(TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/GreystonePlayerCharacter")),
			FName(TEXT("/Game/ParagonSevarog/Characters/Heroes/Sevarog/SevarogPlayerCharacter"))
		};
		return Packages.Contains(Class->GetOutermost()->GetFName());
	}

	FName GetLegacyBranchMemberName(const UEdGraphNode& Node)
	{
		if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(&Node))
		{
			return Call->FunctionReference.GetMemberName();
		}
		if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(&Node))
		{
			return CustomEvent->CustomFunctionName;
		}
		if (const UK2Node_Event* Event = Cast<UK2Node_Event>(&Node))
		{
			return Event->EventReference.GetMemberName();
		}
		if (const UK2Node_VariableSet* Variable = Cast<UK2Node_VariableSet>(&Node))
		{
			return Variable->VariableReference.GetMemberName();
		}
		if (const UK2Node_BaseMCDelegate* Delegate = Cast<UK2Node_BaseMCDelegate>(&Node))
		{
			return Delegate->DelegateReference.GetMemberName();
		}
		return NAME_None;
	}

	bool IsExpectedLegacyBranchNode(const UEdGraphNode& Node)
	{
		if (const UK2Node_DynamicCast* DynamicCast = Cast<UK2Node_DynamicCast>(&Node))
		{
			return IsLegacyPlayerCharacterClass(DynamicCast->TargetType);
		}
		static const TSet<FName> Members = {
			FName(TEXT("BlueprintInitializeAnimation")), FName(TEXT("TryGetPawnOwner")),
			FName(TEXT("Attacking")), FName(TEXT("Attacking_Event_0")), FName(TEXT("isAttacking")),
			FName(TEXT("AnimNotify_SaveAttack")), FName(TEXT("ComboAttackSave")),
			FName(TEXT("AnimNotify_ResetCombo")), FName(TEXT("ResetCombo")), FName(TEXT("Character"))
		};
		return Members.Contains(GetLegacyBranchMemberName(Node));
	}

	bool SaveAuthoredAsset(UObject& Asset)
	{
		UPackage* Package = Asset.GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const FString FileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		return UPackage::SavePackage(Package, nullptr, *FileName, SaveArgs);
	}
}
#endif

bool UDSTRAnimationAuthoringLibrary::RemoveLegacyPlayerCharacterNodes(UAnimBlueprint* AnimBlueprint)
{
#if WITH_EDITOR
	if (!AnimBlueprint
		|| !AnimBlueprint->GetOutermost()->GetName().StartsWith(TEXT("/Game/DediServerRPG/Animations/")))
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_ANIMBP_LEGACY_CLEANUP_FAILED reason=NotProjectAnimBlueprint"));
		return false;
	}
	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(AnimBlueprint);
	if (!EventGraph)
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_ANIMBP_LEGACY_CLEANUP_FAILED asset=%s reason=NoEventGraph"),
			*AnimBlueprint->GetName());
		return false;
	}

	TArray<UEdGraphNode*> Pending;
	for (UEdGraphNode* Node : EventGraph->Nodes)
	{
		const UK2Node_DynamicCast* DynamicCast = Cast<UK2Node_DynamicCast>(Node);
		if (DynamicCast && IsLegacyPlayerCharacterClass(DynamicCast->TargetType))
		{
			Pending.Add(Node);
		}
	}
	if (Pending.IsEmpty())
	{
		UE_LOG(LogDSTR, Log, TEXT("DSTR_ANIMBP_LEGACY_GRAPH_CLEAN asset=%s removed=0"),
			*AnimBlueprint->GetName());
		return true;
	}
	if (Pending.Num() != LegacyPlayerCastCount)
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_ANIMBP_LEGACY_CLEANUP_FAILED asset=%s reason=UnexpectedCastCount count=%d"),
			*AnimBlueprint->GetName(), Pending.Num());
		return false;
	}

	TSet<UEdGraphNode*> NodesToRemove;
	while (!Pending.IsEmpty())
	{
		UEdGraphNode* Node = Pending.Pop(EAllowShrinking::No);
		if (!Node || NodesToRemove.Contains(Node))
		{
			continue;
		}
		NodesToRemove.Add(Node);
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}
			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked && Linked->GetOwningNode() && !NodesToRemove.Contains(Linked->GetOwningNode()))
				{
					Pending.Add(Linked->GetOwningNode());
				}
			}
		}
	}
	if (NodesToRemove.Num() != LegacyPlayerBranchNodeCount)
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_ANIMBP_LEGACY_CLEANUP_FAILED asset=%s reason=UnexpectedBranchSize count=%d"),
			*AnimBlueprint->GetName(), NodesToRemove.Num());
		return false;
	}
	for (const UEdGraphNode* Node : NodesToRemove)
	{
		if (!Node || !IsExpectedLegacyBranchNode(*Node))
		{
			UE_LOG(LogDSTR, Error, TEXT("DSTR_ANIMBP_LEGACY_CLEANUP_FAILED asset=%s reason=UnexpectedNode node=%s class=%s"),
				*AnimBlueprint->GetName(), Node ? *Node->GetName() : TEXT("None"),
				Node ? *Node->GetClass()->GetName() : TEXT("None"));
			return false;
		}
	}

	TArray<UEdGraphNode_Comment*> CommentsToRemove;
	for (UEdGraphNode* Node : EventGraph->Nodes)
	{
		if (UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node);
			Comment && (Comment->NodeComment == TEXT("When the Character BP sends a Left Mouse Button, we are attacking.")
				|| Comment->NodeComment == TEXT("These notifies are set in Montages to define end of attack and define when combo input will time out")))
		{
			CommentsToRemove.Add(Comment);
		}
	}

	AnimBlueprint->Modify();
	for (UEdGraphNode* Node : NodesToRemove)
	{
		FBlueprintEditorUtils::RemoveNode(AnimBlueprint, Node, true);
	}
	for (UEdGraphNode_Comment* Comment : CommentsToRemove)
	{
		FBlueprintEditorUtils::RemoveNode(AnimBlueprint, Comment, true);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
	if (AnimBlueprint->Status == BS_Error)
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_ANIMBP_LEGACY_CLEANUP_FAILED asset=%s reason=CompileFailed"),
			*AnimBlueprint->GetName());
		return false;
	}

	if (!SaveAuthoredAsset(*AnimBlueprint))
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_ANIMBP_LEGACY_CLEANUP_FAILED asset=%s reason=SaveFailed"),
			*AnimBlueprint->GetName());
		return false;
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_ANIMBP_LEGACY_GRAPH_CLEAN asset=%s removed=%d comments=%d"),
		*AnimBlueprint->GetName(), NodesToRemove.Num(), CommentsToRemove.Num());
	return true;
#else
	return false;
#endif
}

bool UDSTRAnimationAuthoringLibrary::AddFullBodySlot(UAnimBlueprint* AnimBlueprint, const FName SlotName)
{
#if WITH_EDITOR
	if (!AnimBlueprint || SlotName.IsNone())
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_ANIMBP_SLOT_FAILED reason=BadArguments"));
		return false;
	}
	UEdGraph* AnimGraph = FindObject<UEdGraph>(AnimBlueprint, *UEdGraphSchema_K2::GN_AnimGraph.ToString());
	if (!AnimGraph)
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_ANIMBP_SLOT_FAILED asset=%s reason=NoAnimGraph"), *AnimBlueprint->GetName());
		return false;
	}

	UAnimGraphNode_Root* Root = nullptr;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (UAnimGraphNode_Root* Candidate = Cast<UAnimGraphNode_Root>(Node))
		{
			Root = Candidate;
			break;
		}
	}
	UEdGraphPin* FinalPose = Root ? FindPosePin(*Root, EGPD_Input) : nullptr;
	if (!FinalPose)
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_ANIMBP_SLOT_FAILED asset=%s reason=NoOutputPoseNode"), *AnimBlueprint->GetName());
		return false;
	}
	UEdGraphPin* Upstream = FinalPose->LinkedTo.Num() > 0 ? FinalPose->LinkedTo[0] : nullptr;
	if (!Upstream || !Upstream->GetOwningNode())
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_ANIMBP_SLOT_FAILED asset=%s reason=FinalPoseUnconnected"), *AnimBlueprint->GetName());
		return false;
	}

	if (const UAnimGraphNode_Slot* Existing = Cast<UAnimGraphNode_Slot>(Upstream->GetOwningNode()))
	{
		if (Existing->Node.SlotName == SlotName)
		{
			UE_LOG(LogDSTR, Log, TEXT("DSTR_ANIMBP_SLOT_PRESENT asset=%s slot=%s"),
				*AnimBlueprint->GetName(), *SlotName.ToString());
			return true;
		}
	}

	UAnimGraphNode_Slot* SlotNode = nullptr;
	{
		FGraphNodeCreator<UAnimGraphNode_Slot> Creator(*AnimGraph);
		SlotNode = Creator.CreateNode(false);
		SlotNode->Node.SlotName = SlotName;
		SlotNode->NodePosX = Root->NodePosX - 300;
		SlotNode->NodePosY = Root->NodePosY;
		Creator.Finalize();
	}
	UEdGraphPin* SlotSource = FindPosePin(*SlotNode, EGPD_Input);
	UEdGraphPin* SlotPose = FindPosePin(*SlotNode, EGPD_Output);
	if (!SlotSource || !SlotPose)
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_ANIMBP_SLOT_FAILED asset=%s reason=SlotNodeHasNoPosePins"), *AnimBlueprint->GetName());
		return false;
	}

	const UEdGraphSchema* Schema = AnimGraph->GetSchema();
	FinalPose->BreakLinkTo(Upstream);
	if (!Schema || !Schema->TryCreateConnection(Upstream, SlotSource) || !Schema->TryCreateConnection(SlotPose, FinalPose))
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_ANIMBP_SLOT_FAILED asset=%s reason=ConnectionRefused"), *AnimBlueprint->GetName());
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);

	if (!SaveAuthoredAsset(*AnimBlueprint))
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_ANIMBP_SLOT_FAILED asset=%s reason=SaveFailed"), *AnimBlueprint->GetName());
		return false;
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_ANIMBP_SLOT_ADDED asset=%s slot=%s source=%s"),
		*AnimBlueprint->GetName(), *SlotName.ToString(), *Upstream->GetOwningNode()->GetClass()->GetName());
	return true;
#else
	return false;
#endif
}

UAnimMontage* UDSTRAnimationAuthoringLibrary::CreateCombatMontage(
	UAnimSequence* Source, const FName SlotName, const FString& PackagePath, const FString& AssetName,
	const float ImpactTimeSeconds, const bool bHoldLastFrame, const float MaxLengthSeconds)
{
#if WITH_EDITOR
	if (!Source)
	{
		return nullptr;
	}
	const FString PackageName = PackagePath / AssetName;
	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();

	UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
	Factory->SourceAnimation = Source;
	Factory->TargetSkeleton = Source->GetSkeleton();
	UAnimMontage* Montage = Cast<UAnimMontage>(Factory->FactoryCreateNew(
		UAnimMontage::StaticClass(), Package, *AssetName, RF_Public | RF_Standalone, nullptr, GWarn));
	if (!Montage)
	{
		return nullptr;
	}

	if (!SlotName.IsNone() && Montage->SlotAnimTracks.Num() > 0)
	{
		Montage->SlotAnimTracks[0].SlotName = SlotName;
	}
	if (Montage->SlotAnimTracks.Num() > 0 && Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() > 0)
	{
		FAnimSegment& Segment = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
		if (MaxLengthSeconds > 0.0f)
		{
			Segment.AnimEndTime = FMath::Min(
				Segment.AnimEndTime, Segment.AnimStartTime + MaxLengthSeconds * Segment.GetValidPlayRate());
		}
		Montage->SetCompositeLength(Segment.GetLength());
	}
	Montage->BlendIn.SetBlendTime(0.1f);
	Montage->BlendOut.SetBlendTime(bHoldLastFrame ? 0.0f : 0.15f);
	Montage->bEnableAutoBlendOut = !bHoldLastFrame;

	if (ImpactTimeSeconds >= 0.0f)
	{
		UAnimationBlueprintLibrary::AddAnimationNotifyTrack(Montage, TEXT("Impact"), FLinearColor::Red);
		UAnimationBlueprintLibrary::AddAnimationNotifyEvent(Montage, TEXT("Impact"), ImpactTimeSeconds, UDSTRAnimNotify_Impact::StaticClass());
	}

	Montage->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Montage);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	const FString FileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	UPackage::SavePackage(Package, Montage, *FileName, SaveArgs);
	return Montage;
#else
	return nullptr;
#endif
}

FString UDSTRAnimationAuthoringLibrary::DescribeAnimation(UAnimationAsset* Asset)
{
	if (!Asset)
	{
		return TEXT("null");
	}
	if (const UAnimSequence* Sequence = Cast<UAnimSequence>(Asset))
	{
		return FString::Printf(TEXT("%s length=%.3f rootmotion=%d additive=%d"), *Asset->GetName(), Sequence->GetPlayLength(),
			Sequence->bEnableRootMotion ? 1 : 0, Sequence->AdditiveAnimType != AAT_None ? 1 : 0);
	}
	if (const UAnimMontage* Montage = Cast<UAnimMontage>(Asset))
	{
		const FName Slot = Montage->SlotAnimTracks.Num() > 0 ? Montage->SlotAnimTracks[0].SlotName : NAME_None;
		return FString::Printf(TEXT("%s length=%.3f slot=%s notifies=%d autoblendout=%d"),
			*Asset->GetName(), Montage->GetPlayLength(), *Slot.ToString(), Montage->Notifies.Num(),
			Montage->bEnableAutoBlendOut ? 1 : 0);
	}
	if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(Asset))
	{
		FString Axes;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const FBlendParameter& Parameter = BlendSpace->GetBlendParameter(Index);
			Axes += FString::Printf(TEXT(" axis%d=%s[%.0f..%.0f]"), Index, *Parameter.DisplayName, Parameter.Min, Parameter.Max);
		}
		return Asset->GetName() + Axes;
	}
	return Asset->GetName();
}
