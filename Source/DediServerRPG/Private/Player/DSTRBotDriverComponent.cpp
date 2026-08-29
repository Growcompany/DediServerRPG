#include "Player/DSTRBotDriverComponent.h"
#include "DSTRLog.h"

#include "AbilitySystem/DSTRAbilitySystemComponent.h"
#include "AbilitySystem/DSTRAttributeSet.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Combat/DSTRDamageRules.h"
#include "Components/CapsuleComponent.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "EngineUtils.h"
#include "Game/DSTRDungeonRules.h"
#include "Game/DSTRGameState.h"
#include "GameFramework/Controller.h"
#include "Misc/Parse.h"
#include "Player/DSTRPlayerController.h"
#include "Player/DSTRPlayerState.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "Presentation/DSTRVisualAssetRegistry.h"
#include "TimerManager.h"
#include "UI/DSTRLobbyViewModel.h"
#include "World/DSTRAttackBuffPickup.h"
#include "World/DSTRBossGate.h"

UDSTRBotDriverComponent::UDSTRBotDriverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UDSTRBotDriverComponent::ShouldAttach(const FString& CommandLine, const bool bLocallyControlled)
{
	return bLocallyControlled && FParse::Param(*CommandLine, TEXT("DSTRClientAutoPlay"));
}

void UDSTRBotDriverComponent::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->GetTimerManager().SetTimer(
		StepTimerHandle, this, &UDSTRBotDriverComponent::RunStep, StepSeconds, true, 0.5f);
}

float UDSTRBotDriverComponent::AimAt(
	ADediServerRPGCharacter& Character,
	const FVector& ToTarget,
	const bool bImmediate)
{
	AController* OwningController = Character.GetController();
	if (!OwningController || !Character.IsLocallyControlled() || ToTarget.IsNearlyZero())
	{
		return 180.0f;
	}
	const float CurrentYaw = static_cast<float>(OwningController->GetControlRotation().Yaw);
	const float TargetYaw = static_cast<float>(ToTarget.Rotation().Yaw);
	const float Error = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw));
	const float NewYaw = bImmediate ? TargetYaw : FMath::FixedTurn(CurrentYaw, TargetYaw, AimDegreesPerStep);
	OwningController->SetControlRotation(
		FRotator(ADediServerRPGCharacter::DefaultCameraPitch, NewYaw, 0.0f));
	return Error;
}

bool UDSTRBotDriverComponent::TryLobbyStart(ADediServerRPGCharacter& Character)
{
	UWorld* World = Character.GetWorld();
	const ADSTRGameState* State = World ? World->GetGameState<ADSTRGameState>() : nullptr;
	if (!State)
	{
		return true;
	}
	if (State->GetMatchPhase() != EDSTRMatchPhase::WaitingForPlayers)
	{
		return false;
	}

	const double Now = World->GetTimeSeconds();
	if (LobbyEnterTime < 0.0)
	{
		LobbyEnterTime = Now;
		LastPlayerCountChangeTime = Now;
		LastKnownPlayerCount = State->PlayerArray.Num();
	}
	if (State->PlayerArray.Num() != LastKnownPlayerCount)
	{
		LastKnownPlayerCount = State->PlayerArray.Num();
		LastPlayerCountChangeTime = Now;
	}

	ADSTRPlayerController* PC = Cast<ADSTRPlayerController>(Character.GetController());
	if (!PC)
	{
		return true;
	}
	PC->ReportPresentationReadyIfLoaded();

	const bool bIsHost = State->IsHost(Character.GetPlayerState());
	const bool bRetryDue = !State->IsCountdownActive()
		&& (LastBotStartRequestTime < 0.0 || Now - LastBotStartRequestTime >= BotStartRetrySeconds);
	if (bRetryDue && FDSTRLobbyViewModel::ShouldBotStart(
		bIsHost, State->GetMatchPhase(),
		static_cast<float>(Now - LobbyEnterTime),
		static_cast<float>(Now - LastPlayerCountChangeTime),
		FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(this)))
	{
		LastBotStartRequestTime = Now;
		UE_LOG(LogDSTR, Log, TEXT("DSTR_BOT_LOBBY_START Player=%s"), *Character.GetName());
		PC->Server_RequestStartMatch();
	}
	return true;
}

void UDSTRBotDriverComponent::RunStep()
{
	ADediServerRPGCharacter* Character = Cast<ADediServerRPGCharacter>(GetOwner());
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World || !Character->IsLocallyControlled())
	{
		return;
	}
	if (!bAnnounced)
	{
		bAnnounced = true;
		UE_LOG(LogDSTR, Log, TEXT("DSTR_CLIENT_AUTOPLAY_READY Player=%s"), *Character->GetName());
	}

	Character->SetDesiredMoveDirection(FVector::ZeroVector);
	if (Character->IsDowned())
	{
		return;
	}
	if (TryLobbyStart(*Character))
	{
		return;
	}

	const FVector SelfLocation = Character->GetActorLocation();
	ADSTREnemyCharacter* NearestEnemy = nullptr;
	float EnemyDistanceSquared = TNumericLimits<float>::Max();
	int32 EnemiesInChargeScan = 0;
	for (TActorIterator<ADSTREnemyCharacter> It(World); It; ++It)
	{
		ADSTREnemyCharacter* Candidate = *It;
		if (!Candidate || Candidate->IsCombatantDead() || Candidate->IsDormant())
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(SelfLocation, Candidate->GetActorLocation());
		if (DistanceSquared <= FMath::Square(FDSTRDamageRules::ChargeBotEnemyRadius))
		{
			++EnemiesInChargeScan;
		}
		if (DistanceSquared < EnemyDistanceSquared)
		{
			EnemyDistanceSquared = DistanceSquared;
			NearestEnemy = Candidate;
		}
	}
	const bool bEnemyEngaged = NearestEnemy
		&& EnemyDistanceSquared <= FMath::Square(FDSTRDungeonRules::AdvanceCombatRange);

	ADediServerRPGCharacter* DownedTeammate = nullptr;
	float DownedDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<ADediServerRPGCharacter> It(World); It; ++It)
	{
		ADediServerRPGCharacter* Candidate = *It;
		if (!Candidate || Candidate == Character || !Candidate->IsDowned() || Candidate->IsEliminated())
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(SelfLocation, Candidate->GetActorLocation());
		if (DistanceSquared < DownedDistanceSquared)
		{
			DownedDistanceSquared = DistanceSquared;
			DownedTeammate = Candidate;
		}
	}
	if (DownedTeammate
		&& bEnemyEngaged
		&& DownedDistanceSquared > FMath::Square(FDSTRDamageRules::InteractReach))
	{
		DownedTeammate = nullptr;
	}

	if (DownedTeammate)
	{
		const FVector ToTeammate = DownedTeammate->GetActorLocation() - SelfLocation;
		AimAt(*Character, ToTeammate, false);
		if (DownedDistanceSquared <= FMath::Square(FDSTRDamageRules::InteractReach))
		{
			Character->PressAbilityInput(DSTRGameplayTags::InputTag_Ability_Revive.GetTag());
		}
		else
		{
			Character->SetDesiredMoveDirection(ToTeammate.GetSafeNormal2D());
		}
		return;
	}

	ADSTRAttackBuffPickup* NearestPickup = nullptr;
	float PickupDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<ADSTRAttackBuffPickup> It(World); It; ++It)
	{
		ADSTRAttackBuffPickup* Candidate = *It;
		if (!Candidate || Candidate->IsConsumed())
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(SelfLocation, Candidate->GetActorLocation());
		if (DistanceSquared < PickupDistanceSquared)
		{
			PickupDistanceSquared = DistanceSquared;
			NearestPickup = Candidate;
		}
	}

	// 교전 중에는 버프보다 적 처리를 우선한다.
	if (NearestPickup && !bEnemyEngaged)
	{
		const FVector ToPickup = NearestPickup->GetActorLocation() - SelfLocation;
		AimAt(*Character, ToPickup, false);
		if (PickupDistanceSquared <= FMath::Square(FDSTRDamageRules::InteractReach))
		{
			Character->PressAbilityInput(DSTRGameplayTags::InputTag_Ability_Revive.GetTag());
		}
		else
		{
			Character->SetDesiredMoveDirection(ToPickup.GetSafeNormal2D());
		}
		return;
	}

	const ADSTRGameState* State = World->GetGameState<ADSTRGameState>();
	const ADSTRBossGate* Gate = State ? State->GetBossGateActor() : nullptr;
	const bool bAdvancing = State && State->GetMatchPhase() == EDSTRMatchPhase::Advance;
	const bool bLockedOutside = State && Gate && Gate->IsLocked()
		&& FVector::Dist2D(SelfLocation, State->GetBossRoom()) > FDSTRDungeonRules::BossRoomRadius;
	if ((bAdvancing || bLockedOutside) && !bEnemyEngaged)
	{
		const FVector Goal = Gate && !Gate->IsSealed()
			? Gate->GetEntryPoint()
			: FDSTRDungeonRules::NextWaypoint(State->GetAdvancePath(), SelfLocation, State->GetBossRoom());
		const FVector ToGoal = Goal - SelfLocation;
		if (ToGoal.Size2D() > FDSTRDungeonRules::AdvanceWaypointRadius)
		{
			Character->SetDesiredMoveDirection(ToGoal.GetSafeNormal2D());
		}
		AimAt(*Character, ToGoal, false);
		return;
	}

	if (!NearestEnemy)
	{
		return;
	}

	const FVector ToEnemy = NearestEnemy->GetActorLocation() - SelfLocation;
	if (NearestEnemy->IsPreparingAreaAttack()
		&& EnemyDistanceSquared <= FMath::Square(TelegraphEscapeRange))
	{
		const float AimError = AimAt(*Character, -ToEnemy, true);
		if (AimError <= AimCommitDegrees)
		{
			Character->PressAbilityInput(DSTRGameplayTags::InputTag_Ability_Charge.GetTag());
		}
		Character->SetDesiredMoveDirection((-ToEnemy).GetSafeNormal2D());
		return;
	}

	const float AimError = AimAt(*Character, ToEnemy, false);
	const float AttackDistance = FDSTRCombatPresentation::GetHitDistance(
		EDSTRCombatAction::BasicAttack, NearestEnemy->GetCapsuleComponent()->GetScaledCapsuleRadius());
	if (EnemyDistanceSquared > FMath::Square(AttackDistance - FDSTRCombatPresentation::EngageMargin))
	{
		Character->SetDesiredMoveDirection(ToEnemy.GetSafeNormal2D());
	}

	const ADSTRPlayerState* PlayerState = Character->GetPlayerState<ADSTRPlayerState>();
	const UDSTRAttributeSet* Attributes = PlayerState ? PlayerState->GetAttributeSet() : nullptr;
	const float HealthRatio = Attributes && Attributes->GetMaxHealth() > 0.0f
		? Attributes->GetHealth() / Attributes->GetMaxHealth() : 1.0f;
	if (HealthRatio < BotFortifyHealthRatio
		&& Character->IsAbilityReady(DSTRGameplayTags::Effect_Cooldown_Fortify.GetTag()))
	{
		Character->PressAbilityInput(DSTRGameplayTags::InputTag_Ability_Fortify.GetTag());
	}
	if (FDSTRDamageRules::ShouldBotCharge(
		EnemiesInChargeScan, Character->IsAbilityReady(DSTRGameplayTags::Effect_Cooldown_Charge.GetTag())))
	{
		Character->PressAbilityInput(DSTRGameplayTags::InputTag_Ability_Charge.GetTag());
	}
	if (FDSTRDamageRules::ShouldBotLeap(
		static_cast<float>(FMath::Sqrt(EnemyDistanceSquared)),
		AimError,
		Character->IsAbilityReady(DSTRGameplayTags::Effect_Cooldown_MakeWay.GetTag())))
	{
		Character->PressAbilityInput(DSTRGameplayTags::InputTag_Ability_MakeWay.GetTag());
	}
	if (NearestEnemy->IsBoss()
		&& EnemyDistanceSquared <= FMath::Square(FDSTRDamageRules::ReckoningRadius)
		&& Character->IsAbilityReady(DSTRGameplayTags::Effect_Cooldown_Reckoning.GetTag()))
	{
		Character->PressAbilityInput(DSTRGameplayTags::InputTag_Ability_Reckoning.GetTag());
	}
	if (EnemyDistanceSquared <= FMath::Square(AttackDistance))
	{
		Character->PressAbilityInput(DSTRGameplayTags::InputTag_Ability_BasicAttack.GetTag());
	}
}
