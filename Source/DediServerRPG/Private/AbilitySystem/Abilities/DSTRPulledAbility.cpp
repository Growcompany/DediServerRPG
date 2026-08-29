#include "AbilitySystem/Abilities/DSTRPulledAbility.h"
#include "DSTRLog.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/RootMotionSource.h"

const FName UDSTRPulledAbility::PullTaskName(TEXT("DSTRSiphonPull"));

UDSTRPulledAbility::UDSTRPulledAbility()
{
	AbilityTags.AddTag(DSTRGameplayTags::Ability_Reaction_Pulled.GetTag());
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	ActivationBlockedTags.Reset();
	ActivationBlockedTags.AddTag(DSTRGameplayTags::State_Dead.GetTag());

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = DSTRGameplayTags::Event_Combat_Pulled.GetTag();
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UDSTRPulledAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const FGameplayAbilityTargetData* Destination = TriggerEventData
		? TriggerEventData->TargetData.Get(0) : nullptr;
	if (!ActorInfo || !Destination || TriggerEventData->EventMagnitude <= 0.0f
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	const FVector PullTo = Destination->GetEndPointTransform().GetLocation();
	UE_LOG(LogDSTR, Log, TEXT("DSTR_PULL Body=%s To=%s Seconds=%.2f Authority=%d"),
		*GetNameSafe(ActorInfo->AvatarActor.Get()),
		*PullTo.ToCompactString(),
		TriggerEventData->EventMagnitude,
		ActorInfo->IsNetAuthority() ? 1 : 0);

	UAbilityTask_ApplyRootMotionMoveToForce* PullTask =
		UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
			this,
			PullTaskName,
			PullTo,
			TriggerEventData->EventMagnitude,
			false,
			MOVE_None,
			false,
			nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity,
			FVector::ZeroVector,
			0.0f);
	if (!PullTask)
	{
		FinishAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}
	PullTask->OnTimedOut.AddDynamic(this, &UDSTRPulledAbility::HandlePullFinished);
	PullTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UDSTRPulledAbility::HandlePullFinished);
	PullTask->ReadyForActivation();
}

void UDSTRPulledAbility::HandlePullFinished()
{
	FinishAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
}
