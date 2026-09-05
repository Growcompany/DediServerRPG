// Copyright Epic Games, Inc. All Rights Reserved.

#include "DediServerRPGGameMode.h"
#include "DSTRLog.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DSTRAttributeSet.h"
#include "AbilitySystem/DSTRGameplayEffects.h"
#include "Components/StaticMeshComponent.h"
#include "DediServerRPGCharacter.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Game/DSTRDownedRules.h"
#include "Game/DSTRDungeonRules.h"
#include "Game/DSTRGameState.h"
#include "Game/DSTRSpawnRules.h"
#include "Game/DSTRStallRules.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Player/DSTRPlayerController.h"
#include "Player/DSTRPlayerState.h"
#include "TimerManager.h"
#include "UI/DSTRHUD.h"
#include "UI/DSTRLobbyViewModel.h"
#include "World/DSTRAttackBuffPickup.h"
#include "World/DSTRBossGate.h"
#include "World/DSTRBossRoomMarker.h"

namespace
{
	constexpr int32 FallbackSpawnPointCount = 6;
	const FVector BossSpawnOffset(340.0f, 0.0f, 100.0f);
	constexpr float DoorNavProjectionExtent = 200.0f;
	constexpr float NavProbeDelaySeconds = 4.0f;
	constexpr float AutoStartRetrySeconds = 1.0f;
	constexpr float EncounterNavRetrySeconds = 0.5f;
	constexpr float EncounterNavMaxWaitSeconds = 5.0f;

	// 내비 빌드 완료 전에 질의하면 경로가 전부 Partial로 나와 문·보스방 후보가 모두 탈락한다.
	bool IsEncounterNavigationReady(UWorld* World, const FVector& Origin,
		int32& OutNavData, bool& bOutBuilding, bool& bOutOriginOnNav)
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		OutNavData = Navigation ? Navigation->NavDataSet.Num() : 0;
		bOutBuilding = Navigation && Navigation->IsNavigationBuildInProgress();
		FNavLocation OriginOnNav;
		const ANavigationData* AnyNavData = nullptr;
		bOutOriginOnNav = Navigation && Navigation->ProjectPointToNavigation(
			Origin, OriginOnNav, FVector(DoorNavProjectionExtent), AnyNavData);
		return OutNavData > 0 && !bOutBuilding && bOutOriginOnNav;
	}
}

ADediServerRPGGameMode::ADediServerRPGGameMode()
{
	// 출혈, 정체 감시, 관문 판정은 서버 틱에서 처리한다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	PlayerStateClass = ADSTRPlayerState::StaticClass();
	PlayerControllerClass = ADSTRPlayerController::StaticClass();
	GameStateClass = ADSTRGameState::StaticClass();
	HUDClass = ADSTRHUD::StaticClass();

	DefaultPawnClass = ADediServerRPGCharacter::StaticClass();
}

void ADediServerRPGGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	RoomSession.bEnabled = FParse::Param(FCommandLine::Get(), TEXT("DSTRRoomServer"))
		|| UGameplayStatics::ParseOption(Options, TEXT("DSTRRoomServer")) == TEXT("1");
}

void ADediServerRPGGameMode::PreLogin(
	const FString& Options,
	const FString& Address,
	const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	if (UGameplayStatics::ParseOption(Options, TEXT("DSTRStatus")) == TEXT("1"))
	{
		FDSTRServerStatus Status;
		Status.Players = GetNumPlayers();
		Status.bManagedRoom = RoomSession.bEnabled;
		Status.RoomId = RoomSession.RoomId;
		Status.RoomName = RoomSession.bEnabled ? RoomSession.RoomName : FDSTRServerStatus::GetConfiguredRoomName();
		if (const ADSTRGameState* State = GetGameState<ADSTRGameState>())
		{
			Status.Phase = State->GetMatchPhase();
			Status.bCountdown = State->IsCountdownActive();
		}
		// A protocol reply closes this handshake before Welcome/Join can allocate a player.
		ErrorMessage = Status.Encode();
		return;
	}
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (ErrorMessage.IsEmpty())
	{
		ErrorMessage = GetAdmissionError(Options);
	}
}

FString ADediServerRPGGameMode::GetAdmissionError(const FString& Options)
{
	const FString RoomError = RoomSession.CheckLogin(FDSTRRoomRequest::FromOptions(Options));
	if (!RoomError.IsEmpty())
	{
		return RoomError;
	}
	if (!CanAcceptPlayerCount(GetNumPlayers()))
	{
		return TEXT("ServerFull");
	}
	const ADSTRGameState* State = GetGameState<ADSTRGameState>();
	if (State && (State->IsCountdownActive() || State->GetMatchPhase() != EDSTRMatchPhase::WaitingForPlayers))
	{
		return TEXT("MatchInProgress");
	}
	return FString();
}

APlayerController* ADediServerRPGGameMode::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal,
	const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	// Capacity or countdown may change while an accepted connection is loading the map.
	ErrorMessage = GetAdmissionError(Options);
	if (!ErrorMessage.IsEmpty())
	{
		return nullptr;
	}
	APlayerController* Controller = Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);
	if (Controller && RoomSession.Create(FDSTRRoomRequest::FromOptions(Options)))
	{
		if (ADSTRGameState* State = GetGameState<ADSTRGameState>())
		{
			State->SetRoomName(RoomSession.RoomName);
		}
		UE_LOG(LogDSTR, Log, TEXT("DSTR_ROOM_CREATED Id=%s Name=%s"), *RoomSession.RoomId, *RoomSession.RoomName);
	}
	return Controller;
}

void ADediServerRPGGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	const int32 PlayerCount = GetNumPlayers();
	if (ADSTRGameState* State = GetGameState<ADSTRGameState>())
	{
		if (State->GetHostPlayerId() < 0 && NewPlayer && NewPlayer->PlayerState)
		{
			State->SetHostPlayerId(NewPlayer->PlayerState->GetPlayerId());
		}
	}

#if !UE_BUILD_SHIPPING
	if (!bEncounterStarted && ShouldAutoStart(FCommandLine::Get()))
	{
		GetWorldTimerManager().ClearTimer(AutoStartTimer);
		GetWorldTimerManager().SetTimer(
			AutoStartTimer,
			this,
			&ADediServerRPGGameMode::TryAutoStart,
			AutoStartRetrySeconds,
			true,
			GetEncounterStartDelay(PlayerCount));
	}

	if (PlayerCount >= 2 && !bReviveSmokeScheduled
		&& IsReviveSmokeEnabled(FCommandLine::Get()))
	{
		bReviveSmokeScheduled = true;
		GetWorldTimerManager().SetTimer(
			ReviveSmokeTimer,
			this,
			&ADediServerRPGGameMode::TriggerReviveSmoke,
			3.0f,
			false);
	}
#endif
}

float ADediServerRPGGameMode::GetEncounterStartDelay(const int32 PlayerCount)
{
	return PlayerCount >= 2 ? 2.0f : 10.0f;
}

int32 ADediServerRPGGameMode::GetWaveEnemyCount(const int32 PlayerCount)
{
	return PlayerCount <= 1 ? 1 : 1 + FMath::Clamp(PlayerCount, 1, 4);
}

TConstArrayView<FVector> ADediServerRPGGameMode::GetWaveSpawnOffsets()
{
	static const TArray<FVector> Offsets = []()
	{
		TArray<FVector> Ring;
		for (int32 Index = 0; Index < FallbackSpawnPointCount; ++Index)
		{
			Ring.Add(FDSTRSpawnRules::FallbackRingOffset(Index, FallbackSpawnPointCount));
		}
		return Ring;
	}();
	return MakeArrayView(Offsets);
}

FVector ADediServerRPGGameMode::GetBossSpawnOffset()
{
	return BossSpawnOffset;
}

bool ADediServerRPGGameMode::IsReviveSmokeEnabled(const FString& CommandLine)
{
	return FParse::Param(*CommandLine, TEXT("DSTRReviveSmoke"));
}

bool ADediServerRPGGameMode::IsStallSmokeEnabled(const FString& CommandLine)
{
	return FParse::Param(*CommandLine, TEXT("DSTRStallSmoke"));
}

float ADediServerRPGGameMode::GetCaptureTimeDilation(const FString& CommandLine)
{
	float Dilation = 1.0f;
	if (!FParse::Value(*CommandLine, TEXT("DSTRTimeDilation="), Dilation))
	{
		return 1.0f;
	}
	return FMath::Clamp(Dilation, 0.05f, 1.0f);
}

void ADediServerRPGGameMode::Logout(AController* Exiting)
{
	ADSTRGameState* State = GetGameState<ADSTRGameState>();
	const APlayerState* Leaving = Exiting ? Exiting->PlayerState : nullptr;
	if (State && Leaving && State->IsHost(Leaving))
	{
		int32 NextHost = -1;
		for (const APlayerState* Candidate : State->PlayerArray)
		{
			if (Candidate && Candidate != Leaving)
			{
				NextHost = Candidate->GetPlayerId();
				break;
			}
		}
		State->SetHostPlayerId(NextHost);
	}
	Super::Logout(Exiting);
	if (GetNetMode() == NM_DedicatedServer && Cast<APlayerController>(Exiting)
		&& State && (RoomSession.HasRoom() || bEncounterStarted || State->IsCountdownActive()))
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (IsValid(It->Get()) && It->Get() != Exiting)
			{
				return;
			}
		}
		// Commit the empty-session reset now; a same-frame arrival must not cancel it.
		// Wait one tick so the departing controller is fully removed before travel.
		GetWorldTimerManager().SetTimerForNextTick(this, &ADediServerRPGGameMode::ResetEmptyDedicatedServer);
	}
}

void ADediServerRPGGameMode::ResetEmptyDedicatedServer()
{
	UWorld* World = GetWorld();
	const ADSTRGameState* State = GetGameState<ADSTRGameState>();
	if (!World || GetNetMode() != NM_DedicatedServer
		|| !State || (!RoomSession.HasRoom() && !bEncounterStarted && !State->IsCountdownActive()))
	{
		return;
	}

	// Recreate the map, enemies, gate and game state together for the next party.
	UE_LOG(LogDSTR, Log, TEXT("DSTR_SERVER_EMPTY_RESET Map=%s"), *World->URL.Map);
	if (!World->ServerTravel(World->URL.Map))
	{
		UE_LOG(LogDSTR, Error, TEXT("DSTR_SERVER_EMPTY_RESET_FAILED"));
	}
}

bool ADediServerRPGGameMode::ShouldAutoStart(const FString& CommandLine)
{
	return FParse::Param(*CommandLine, TEXT("DSTRAutoStart"));
}

// 요청자 불신. 호스트·페이즈·인원·준비 수 서버 재검사
bool ADediServerRPGGameMode::TryStartMatchByHost(APlayerController* Requester)
{
	const ADSTRGameState* State = GetGameState<ADSTRGameState>();
	const APlayerState* RequesterState = Requester ? Requester->PlayerState : nullptr;
	const bool bIsHost = State && State->IsHost(RequesterState);
	const EDSTRMatchPhase Phase = State ? State->GetMatchPhase() : EDSTRMatchPhase::Failed;
	const int32 PlayerCount = GetNumPlayers();
	const int32 ReadyCount = State ? State->CountPresentationReadyPlayers() : 0;
	if (const TCHAR* Reason = FDSTRLobbyViewModel::GetStartRejectionReason(
		bIsHost, Phase, PlayerCount, ReadyCount, State && State->IsCountdownActive()))
	{
		UE_LOG(LogDSTR, Log, TEXT("DSTR_LOBBY_START_REJECTED Reason=%s Ready=%d/%d"), Reason, ReadyCount, PlayerCount);
		return false;
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_LOBBY_START_ACCEPTED PlayerId=%d Players=%d Ready=%d"),
		RequesterState->GetPlayerId(), PlayerCount, ReadyCount);
	BeginStartCountdown();
	return true;
}

void ADediServerRPGGameMode::TryAutoStart()
{
	ADSTRGameState* State = GetGameState<ADSTRGameState>();
	if (bEncounterStarted || (State && State->IsCountdownActive()))
	{
		GetWorldTimerManager().ClearTimer(AutoStartTimer);
		return;
	}
	const int32 PlayerCount = GetNumPlayers();
	const int32 ReadyCount = State ? State->CountPresentationReadyPlayers() : 0;
	if (const TCHAR* Reason = FDSTRLobbyViewModel::GetStartRejectionReason(
		true, State ? State->GetMatchPhase() : EDSTRMatchPhase::Failed, PlayerCount, ReadyCount, false))
	{
		UE_LOG(LogDSTR, Log, TEXT("DSTR_LOBBY_START_REJECTED Reason=%s Ready=%d/%d"), Reason, ReadyCount, PlayerCount);
		return;
	}
	GetWorldTimerManager().ClearTimer(AutoStartTimer);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_LOBBY_START_ACCEPTED PlayerId=-1 Players=%d Ready=%d"), PlayerCount, ReadyCount);
	BeginStartCountdown();
}

void ADediServerRPGGameMode::BeginStartCountdown()
{
	if (ADSTRGameState* State = GetGameState<ADSTRGameState>())
	{
		State->SetStartCountdown(ADSTRGameState::StartCountdownSeconds);
	}
	GetWorldTimerManager().ClearTimer(EncounterStartTimer);
	GetWorldTimerManager().SetTimer(
		EncounterStartTimer,
		this,
		&ADediServerRPGGameMode::StartEncounter,
		ADSTRGameState::StartCountdownSeconds,
		false);
}

void ADediServerRPGGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogDSTR, Log, TEXT("DSTR_SERVER_READY Mode=Dedicated Players=%d"), GetNumPlayers());
	}

#if !UE_BUILD_SHIPPING
	const float Dilation = GetCaptureTimeDilation(FCommandLine::Get());
	if (Dilation < 1.0f)
	{
		GetWorldSettings()->SetTimeDilation(Dilation);
		UE_LOG(LogDSTR, Log, TEXT("DSTR_TIME_DILATION Value=%.2f"), Dilation);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("DSTRNavProbe")))
	{
		GetWorldTimerManager().SetTimer(
			NavProbeTimer, this, &ADediServerRPGGameMode::RunNavProbe, NavProbeDelaySeconds, false);
	}
#endif
}

void ADediServerRPGGameMode::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bEncounterStarted)
	{
		return;
	}
	TickBleedOut();
	TickStallWatchdog(DeltaSeconds);
	if (!BossGate.IsValid())
	{
		return;
	}
	CheckGateBypass();
	EnforceArenaLock(DeltaSeconds);
}

void ADediServerRPGGameMode::RunNavProbe()
{
	// 내비 로딩이 끝날 때까지 제한 횟수만 재시도한다.
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	++NavProbeAttempts;
	EncounterOrigin = GetEncounterOrigin();
	FNavLocation OriginOnNav;
	const ANavigationData* AnyNavData = nullptr;
	const bool bReady = Navigation
		&& Navigation->NavDataSet.Num() > 0
		&& !Navigation->IsNavigationBuildInProgress()
		&& Navigation->ProjectPointToNavigation(
			EncounterOrigin, OriginOnNav, FVector(DoorNavProjectionExtent), AnyNavData);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_NAV_PROBE_WAIT Attempt=%d NavData=%d Building=%d OriginOnNav=%d"),
		NavProbeAttempts,
		Navigation ? Navigation->NavDataSet.Num() : 0,
		Navigation && Navigation->IsNavigationBuildInProgress() ? 1 : 0,
		bReady ? 1 : 0);
	if (!bReady && NavProbeAttempts < 15)
	{
		GetWorldTimerManager().SetTimer(
			NavProbeTimer, this, &ADediServerRPGGameMode::RunNavProbe, NavProbeDelaySeconds, false);
		return;
	}
	for (const ANavigationData* NavData : Navigation ? Navigation->NavDataSet : TArray<TObjectPtr<ANavigationData>>())
	{
		if (NavData)
		{
			const FNavDataConfig& Config = NavData->GetConfig();
			UE_LOG(LogDSTR, Log, TEXT("DSTR_NAV_PROBE_AGENT Name=%s Radius=%.0f Height=%.0f Step=%.0f"),
				*Config.Name.ToString(), Config.AgentRadius, Config.AgentHeight, Config.AgentStepHeight);
		}
	}
	// 지오메트리를 넓혔는데 내비 볼륨이 안 따라오면 새 구역은 영원히 구워지지 않는다.
	for (const ANavigationData* NavData : Navigation ? Navigation->NavDataSet : TArray<TObjectPtr<ANavigationData>>())
	{
		if (!NavData)
		{
			continue;
		}
		const FBox MeshBounds = NavData->GetBounds();
		UE_LOG(LogDSTR, Log, TEXT("DSTR_NAV_PROBE_MESH Name=%s Runtime=%d Min=%s Max=%s"),
			*NavData->GetName(),
			static_cast<int32>(NavData->GetRuntimeGenerationMode()),
			*MeshBounds.Min.ToCompactString(), *MeshBounds.Max.ToCompactString());
		for (const FBox& Volume : NavData->GetNavigableBounds())
		{
			UE_LOG(LogDSTR, Log, TEXT("DSTR_NAV_PROBE_VOLUME Min=%s Max=%s"),
				*Volume.Min.ToCompactString(), *Volume.Max.ToCompactString());
		}
	}
	// 원점이 내비 밖이면 구멍이 얼마나 큰지 알아야 원인 규모를 가늠할 수 있다.
	FNavLocation WideOrigin;
	const ANavigationData* WideNavData = nullptr;
	const bool bWideFound = Navigation && Navigation->ProjectPointToNavigation(
		EncounterOrigin, WideOrigin, FVector(4000.0f), WideNavData);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_NAV_PROBE_ORIGIN_WIDE Found=%d At=%s Dist=%.0f"),
		bWideFound ? 1 : 0,
		*WideOrigin.Location.ToCompactString(),
		bWideFound ? FVector::Dist(EncounterOrigin, WideOrigin.Location) : -1.0f);
	// 원점 아래에 뭐가 있고, 그 위 여유가 에이전트 높이를 넘는지 봐야 구멍의 정체를 안다.
	FHitResult FloorHit;
	FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(DSTRNavProbeFloor), true);
	if (GetWorld()->LineTraceSingleByChannel(FloorHit,
		EncounterOrigin + FVector(0.0f, 0.0f, 200.0f),
		EncounterOrigin - FVector(0.0f, 0.0f, 1000.0f), ECC_WorldStatic, FloorParams))
	{
		const UPrimitiveComponent* FloorComponent = FloorHit.GetComponent();
		UE_LOG(LogDSTR, Log,
			TEXT("DSTR_NAV_PROBE_FLOOR Actor=%s Comp=%s At=%s AffectsNav=%d Collision=%d"),
			*GetNameSafe(FloorHit.GetActor()), *GetNameSafe(FloorComponent),
			*FloorHit.ImpactPoint.ToCompactString(),
			FloorComponent && FloorComponent->CanEverAffectNavigation() ? 1 : 0,
			FloorComponent ? static_cast<int32>(FloorComponent->GetCollisionEnabled()) : -1);

		FHitResult CeilingHit;
		if (GetWorld()->LineTraceSingleByChannel(CeilingHit,
			FloorHit.ImpactPoint + FVector(0.0f, 0.0f, 5.0f),
			FloorHit.ImpactPoint + FVector(0.0f, 0.0f, 2000.0f), ECC_WorldStatic, FloorParams))
		{
			UE_LOG(LogDSTR, Log,
				TEXT("DSTR_NAV_PROBE_CEILING Actor=%s Comp=%s At=%s Clearance=%.0f Agent=144"),
				*GetNameSafe(CeilingHit.GetActor()), *GetNameSafe(CeilingHit.GetComponent()),
				*CeilingHit.ImpactPoint.ToCompactString(),
				CeilingHit.ImpactPoint.Z - FloorHit.ImpactPoint.Z);
		}
		else
		{
			UE_LOG(LogDSTR, Log, TEXT("DSTR_NAV_PROBE_CEILING None Clearance=inf"));
		}
	}
	else
	{
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_NAV_PROBE_FLOOR None Origin=%s"),
			*EncounterOrigin.ToCompactString());
	}
	DoorSpawnPoints = CollectDoorSpawnPoints();
	ResolveBossRoom();
	UE_LOG(LogDSTR, Log, TEXT("DSTR_NAV_PROBE_DONE Origin=%s Usable=%d Attempts=%d Room=%s"),
		*EncounterOrigin.ToCompactString(),
		DoorSpawnPoints.Num(),
		NavProbeAttempts,
		*BossRoomLocation.ToCompactString());
	FPlatformMisc::RequestExit(false);
}

void ADediServerRPGGameMode::TriggerReviveSmoke()
{
	if (!bEncounterStarted)
	{
		GetWorldTimerManager().SetTimer(
			ReviveSmokeTimer,
			this,
			&ADediServerRPGGameMode::TriggerReviveSmoke,
			1.0f,
			false);
		return;
	}
	for (TActorIterator<ADediServerRPGCharacter> It(GetWorld()); It; ++It)
	{
		ADediServerRPGCharacter* Character = *It;
		UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		if (!Character || Character->IsDowned() || !ASC)
		{
			continue;
		}

		ASC->SetNumericAttributeBase(UDSTRAttributeSet::GetHealthAttribute(), 0.0f);
		Character->HandleOutOfHealth();
		UE_LOG(LogDSTR, Log, TEXT("DSTR_REVIVE_SMOKE_TRIGGER Player=%s"), *Character->GetName());
		return;
	}
}

void ADediServerRPGGameMode::StartEncounter()
{
	if (bEncounterStarted || GetNumPlayers() < 1)
	{
		return;
	}
#if !UE_BUILD_SHIPPING
	if (IsReviveSmokeEnabled(FCommandLine::Get()) && GetNumPlayers() < 2)
	{
		GetWorldTimerManager().SetTimer(
			EncounterStartTimer,
			this,
			&ADediServerRPGGameMode::StartEncounter,
			1.0f,
			false);
		return;
	}
#endif

	const FVector PendingOrigin = GetEncounterOrigin();
	int32 NavDataCount = 0;
	bool bNavBuilding = false;
	bool bOriginOnNav = false;
	const bool bNavReady = IsEncounterNavigationReady(
		GetWorld(), PendingOrigin, NavDataCount, bNavBuilding, bOriginOnNav);
	if (!bNavReady && EncounterNavWaitSeconds < EncounterNavMaxWaitSeconds)
	{
		EncounterNavWaitSeconds += EncounterNavRetrySeconds;
		GetWorldTimerManager().SetTimer(
			EncounterStartTimer,
			this,
			&ADediServerRPGGameMode::StartEncounter,
			EncounterNavRetrySeconds,
			false);
		return;
	}
	if (bNavReady)
	{
		UE_LOG(LogDSTR, Log, TEXT("DSTR_ENCOUNTER_NAV Ready=1 Waited=%.1f NavData=%d Building=%d OriginOnNav=%d Origin=%s"),
			EncounterNavWaitSeconds, NavDataCount, bNavBuilding ? 1 : 0, bOriginOnNav ? 1 : 0,
			*PendingOrigin.ToCompactString());
	}
	else
	{
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_ENCOUNTER_NAV Ready=0 Waited=%.1f NavData=%d Building=%d OriginOnNav=%d Origin=%s"),
			EncounterNavWaitSeconds, NavDataCount, bNavBuilding ? 1 : 0, bOriginOnNav ? 1 : 0,
			*PendingOrigin.ToCompactString());
	}

	bEncounterStarted = true;
	EncounterOrigin = PendingOrigin;
	DoorSpawnPoints = CollectDoorSpawnPoints();
	if (ADSTRGameState* State = GetGameState<ADSTRGameState>())
	{
		State->ClearStartCountdown();
		State->StartMatchFlow();
		State->AdvancePhase(EDSTRMatchPhase::Wave);
		State->PushEvent(EDSTRMatchEventKind::MatchStart);
	}
	ResolveBossRoom();
	PlaceDormantBoss();
	SpawnWave();

	UE_LOG(LogDSTR, Log, TEXT("DSTR_MATCH_START Players=%d Origin=%s"),
		GetNumPlayers(), *EncounterOrigin.ToCompactString());
}

TArray<FVector> ADediServerRPGGameMode::GetLivingPlayerLocations() const
{
	TArray<FVector> Locations;
	for (TActorIterator<ADediServerRPGCharacter> It(GetWorld()); It; ++It)
	{
		if (const ADediServerRPGCharacter* Character = *It; Character && !Character->IsDowned())
		{
			Locations.Add(Character->GetActorLocation());
		}
	}
	return Locations;
}

TArray<FVector> ADediServerRPGGameMode::CollectDoorSpawnPoints() const
{
	TArray<FVector> Points;
	UWorld* World = GetWorld();
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!World || !Navigation)
	{
		return Points;
	}

	const TArray<AActor*> Doors = FDSTRSpawnRules::CollectSpawnDoors(World);
	// 완전 경로가 있는 문만 실제 스폰 후보로 사용한다.
	for (const AActor* Actor : Doors)
	{
		FNavLocation Projected;
		const ANavigationData* AnyNavData = nullptr;
		const bool bProjected = Navigation->ProjectPointToNavigation(
			Actor->GetActorLocation(), Projected, FVector(DoorNavProjectionExtent), AnyNavData);
		const UNavigationPath* Path = bProjected
			? UNavigationSystemV1::FindPathToLocationSynchronously(World, Projected.Location, EncounterOrigin)
			: nullptr;
		const bool bComplete = Path && Path->IsValid() && !Path->IsPartial();
		UE_LOG(LogDSTR, Log, TEXT("DSTR_SPAWN_POINT_DOOR Name=%s At=%s Projected=%d On=%s Path=%s"),
			*Actor->GetName(),
			*Actor->GetActorLocation().ToCompactString(),
			bProjected ? 1 : 0,
			*Projected.Location.ToCompactString(),
			!Path ? TEXT("None") : (bComplete ? TEXT("Complete") : TEXT("Partial")));
		if (bComplete)
		{
			Points.Add(Projected.Location);
		}
	}

	UE_LOG(LogDSTR, Log, TEXT("DSTR_SPAWN_POINTS Doors=%d Usable=%d Rule=Tag"),
		Doors.Num(), Points.Num());
	return Points;
}

ADSTREnemyCharacter* ADediServerRPGGameMode::SpawnEnemyAt(const FVector& Location, const FRotator& Rotation)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADSTREnemyCharacter* Enemy = GetWorld()->SpawnActor<ADSTREnemyCharacter>(
		ADSTREnemyCharacter::StaticClass(), Location, Rotation, SpawnParameters);
	if (Enemy)
	{
		Enemy->OnEnemyDefeated.AddUObject(this, &ADediServerRPGGameMode::HandleEnemyDefeated);
		Enemy->OnEnemyAwakened.AddUObject(this, &ADediServerRPGGameMode::HandleEnemyAwakened);
	}
	return Enemy;
}

TArray<FVector> ADediServerRPGGameMode::CollectFloorTileCentres(TArray<FVector>& OutTiles) const
{
	OutTiles.Reset();
	TArray<FVector> Centres;
	UWorld* World = const_cast<UWorld*>(GetWorld());
	if (!World)
	{
		return Centres;
	}

	TSet<FIntVector> Occupied;
	for (const UStaticMeshComponent* Mesh : FDSTRSpawnRules::CollectMinimapFloors(World))
	{
		const FBox Box = Mesh->Bounds.GetBox();
		const FVector Tile(Box.GetCenter().X, Box.GetCenter().Y, Box.Max.Z);
		OutTiles.Add(Tile);
		const FIntVector Cell(
			FMath::FloorToInt(Tile.X / FDSTRDungeonRules::FloorTileGrid),
			FMath::FloorToInt(Tile.Y / FDSTRDungeonRules::FloorTileGrid),
			FMath::FloorToInt(Tile.Z / FDSTRDungeonRules::FloorTileGrid));
		bool bAlready = false;
		Occupied.Add(Cell, &bAlready);
		if (!bAlready)
		{
			Centres.Add(Tile);
		}
	}
	return Centres;
}

// 손으로 지은 맵에서는 런타임 추론보다 레벨에 찍어둔 마커가 항상 옳다.
bool ADediServerRPGGameMode::ResolveBossRoomFromMarker(UWorld* World, UNavigationSystemV1* Navigation)
{
	const ADSTRBossRoomMarker* Marker = ADSTRBossRoomMarker::FindInWorld(World);
	if (!Marker)
	{
		return false;
	}

	ArenaRadius = Marker->GetArenaRadius();
	ArenaHalfHeight = Marker->GetArenaHalfHeight();

	FNavLocation Projected;
	const ANavigationData* AnyNavData = nullptr;
	const bool bOnNav = Navigation->ProjectPointToNavigation(
		Marker->GetActorLocation(), Projected, FVector(DoorNavProjectionExtent), AnyNavData);
	BossRoomLocation = bOnNav ? Projected.Location : Marker->GetActorLocation();
	if (!bOnNav)
	{
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_BOSS_ROOM_MARKER_OFFNAV Name=%s At=%s Extent=%.0f"),
			*Marker->GetName(), *Marker->GetActorLocation().ToCompactString(), DoorNavProjectionExtent);
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_BOSS_ROOM_MARKER Name=%s At=%s OnNav=%d Radius=%.0f HalfHeight=%.0f"),
		*Marker->GetName(), *BossRoomLocation.ToCompactString(),
		bOnNav ? 1 : 0, ArenaRadius, ArenaHalfHeight);
	return true;
}

bool ADediServerRPGGameMode::ResolveBossRoomByScan(UWorld* World, UNavigationSystemV1* Navigation)
{
	TArray<FVector> Tiles;
	TArray<FVector> Points = CollectFloorTileCentres(Tiles);
	Points.Append(DoorSpawnPoints);

	TArray<FDSTRBossRoomCandidate> Candidates;
	Candidates.Reserve(Points.Num());
	for (const FVector& Point : Points)
	{
		FNavLocation Projected;
		const ANavigationData* AnyNavData = nullptr;
		if (!Navigation->ProjectPointToNavigation(
			Point, Projected, FVector(DoorNavProjectionExtent), AnyNavData))
		{
			continue;
		}
		const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
			World, EncounterOrigin, Projected.Location);
		if (!Path || !Path->IsValid() || Path->IsPartial())
		{
			continue;
		}

		FDSTRBossRoomCandidate Candidate;
		Candidate.Location = Projected.Location;
		Candidate.PathLength = Path->GetPathLength();
		for (const FVector& Tile : Tiles)
		{
			Candidate.TileCount += FVector::Dist2D(Tile, Projected.Location) <= FDSTRDungeonRules::BossRoomRadius
				&& FMath::Abs(Tile.Z - Projected.Location.Z) <= FDSTRDungeonRules::BossRoomRadius * 0.3f ? 1 : 0;
		}
		Candidates.Add(Candidate);
	}

	float LongestPath = 0.0f;
	int32 Halls = 0;
	for (const FDSTRBossRoomCandidate& Candidate : Candidates)
	{
		LongestPath = FMath::Max(LongestPath, Candidate.PathLength);
		Halls += Candidate.TileCount >= FDSTRDungeonRules::BossRoomMinTiles ? 1 : 0;
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_BOSS_ROOM_SCAN Reachable=%d Halls=%d LongestPath=%.0f Budget=%.0f"),
		Candidates.Num(), Halls, LongestPath, FDSTRDungeonRules::BossRoomMaxPathLength);

	const int32 Pick = FDSTRDungeonRules::PickBossRoom(Candidates);
	if (Pick == INDEX_NONE)
	{
		const int32 DoorPick = FDSTRSpawnRules::PickFarthest(DoorSpawnPoints, {EncounterOrigin});
		BossRoomLocation = DoorSpawnPoints.IsValidIndex(DoorPick)
			? DoorSpawnPoints[DoorPick]
			: EncounterOrigin + GetBossSpawnOffset();
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_BOSS_ROOM At=%s PathLen=0 Tiles=0 Candidates=%d Fallback=%s"),
			*BossRoomLocation.ToCompactString(), Candidates.Num(),
			DoorSpawnPoints.IsValidIndex(DoorPick) ? TEXT("Door") : TEXT("Offset"));
	}
	else
	{
		BossRoomLocation = Candidates[Pick].Location;
		UE_LOG(LogDSTR, Log, TEXT("DSTR_BOSS_ROOM At=%s PathLen=%.0f Tiles=%d Candidates=%d Doors=%d"),
			*BossRoomLocation.ToCompactString(),
			Candidates[Pick].PathLength,
			Candidates[Pick].TileCount,
			Candidates.Num(),
			DoorSpawnPoints.Num());
	}
	return Pick != INDEX_NONE;
}

bool ADediServerRPGGameMode::ResolveBossRoom()
{
	BossRoomLocation = FVector::ZeroVector;
	AdvancePath.Reset();
	ArenaRadius = FDSTRDungeonRules::BossRoomRadius;
	ArenaHalfHeight = FDSTRDungeonRules::BossRoomHalfHeight;
	UWorld* World = GetWorld();
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	ADSTRGameState* State = GetGameState<ADSTRGameState>();
	if (!World || !Navigation)
	{
		return false;
	}

	const bool bResolved = ResolveBossRoomFromMarker(World, Navigation)
		|| ResolveBossRoomByScan(World, Navigation);

	const UNavigationPath* BestPath = UNavigationSystemV1::FindPathToLocationSynchronously(
		World, EncounterOrigin, BossRoomLocation);
	const bool bPathUsable = BestPath && BestPath->IsValid() && !BestPath->IsPartial();
	AdvancePath = bPathUsable
		? FDSTRDungeonRules::SimplifyPath(BestPath->PathPoints)
		: TArray<FVector>({EncounterOrigin, BossRoomLocation});
	if (State)
	{
		State->SetBossRoom(BossRoomLocation, ArenaRadius, ArenaHalfHeight);
		State->SetAdvancePath(AdvancePath);
	}
	if (!bPathUsable)
	{
		// 경로가 없으면 진격로·매복·관문이 전부 직선 폴백으로 깔려 벽을 관통한다.
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_ADVANCE_PATH_UNREACHABLE Room=%s Valid=%d Partial=%d"),
			*BossRoomLocation.ToCompactString(),
			BestPath && BestPath->IsValid() ? 1 : 0,
			BestPath && BestPath->IsPartial() ? 1 : 0);
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_ADVANCE_PATH Points=%d Raw=%d"),
		AdvancePath.Num(), BestPath ? BestPath->PathPoints.Num() : 0);
	return bResolved;
}

bool ADediServerRPGGameMode::PlaceDormantBoss()
{
	const FVector SpawnLocation = BossRoomLocation
		+ FVector(0.0f, 0.0f, ADSTREnemyCharacter::GetSpawnGroundClearance(true));
	const FVector Entrance = AdvancePath.Num() >= 2
		? AdvancePath[AdvancePath.Num() - 2]
		: EncounterOrigin;
	ADSTREnemyCharacter* Boss = SpawnEnemyAt(
		SpawnLocation, FRotator(0.0f, FDSTRSpawnRules::FacingYaw(SpawnLocation, Entrance), 0.0f));
	if (!Boss)
	{
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_BOSS_PLACED At=%s Dormant=0 Spawned=0"),
			*SpawnLocation.ToCompactString());
		return false;
	}

	Boss->ConfigureAsBoss(true);
	Boss->SetDormant(true);
	DormantBoss = Boss;
	UE_LOG(LogDSTR, Log, TEXT("DSTR_BOSS_PLACED Name=%s At=%s Dormant=1 FacingYaw=%.0f"),
		*Boss->GetName(),
		*SpawnLocation.ToCompactString(),
		FDSTRSpawnRules::FacingYaw(SpawnLocation, Entrance));
	return true;
}

void ADediServerRPGGameMode::BeginAdvance()
{
	ADSTRGameState* State = GetGameState<ADSTRGameState>();
	if (!State || !State->AdvancePhase(EDSTRMatchPhase::Advance))
	{
		return;
	}

	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const FDSTRBossGatePlacement Placement = FDSTRDungeonRules::PlaceGate(
		AdvancePath, BossRoomLocation, ArenaRadius);
	const TArray<FVector> Corridor = Placement.bValid
		? FDSTRDungeonRules::TrimPathAtGate(AdvancePath, Placement.Location, Placement.Forward)
		: AdvancePath;
	const TArray<FVector> Anchors = FDSTRDungeonRules::PlaceAmbushes(Corridor);
	const int32 PerGroup = FDSTRDungeonRules::GetAmbushCountPerGroup(GetNumPlayers());
	const TArray<FVector> PlayerLocations = GetLivingPlayerLocations();
	const FVector PartyCentre = PlayerLocations.IsEmpty()
		? EncounterOrigin : FDSTRSpawnRules::PartyCentre(PlayerLocations);

	int32 Placed = 0;
	for (int32 Group = 0; Group < Anchors.Num(); ++Group)
	{
		for (int32 Index = 0; Index < PerGroup; ++Index)
		{
			const float Angle = FMath::DegreesToRadians(360.0f * Index / FMath::Max(1, PerGroup));
			const FVector Offset = PerGroup > 1
				? FVector(
					FDSTRDungeonRules::AmbushGroupSpread * FMath::Cos(Angle),
					FDSTRDungeonRules::AmbushGroupSpread * FMath::Sin(Angle),
					0.0f)
				: FVector::ZeroVector;
			FNavLocation Projected;
			const ANavigationData* AnyNavData = nullptr;
			const FVector Wanted = Anchors[Group] + Offset;
			const FVector Ground = Navigation && Navigation->ProjectPointToNavigation(
				Wanted, Projected, FVector(DoorNavProjectionExtent), AnyNavData)
				? Projected.Location : Wanted;
			const FVector SpawnLocation = Ground
				+ FVector(0.0f, 0.0f, ADSTREnemyCharacter::GetSpawnGroundClearance(false));
			ADSTREnemyCharacter* Minion = SpawnEnemyAt(
				SpawnLocation,
				FRotator(0.0f, FDSTRSpawnRules::FacingYaw(SpawnLocation, PartyCentre), 0.0f));
			if (Minion)
			{
				Minion->SetDormant(true);
				AmbushEnemies.Add(Minion);
				++Placed;
			}
		}
	}

	State->SetRemainingEnemies(State->GetRemainingEnemies() + Placed);
	State->SetAmbushRemaining(Placed);
	State->SetPendingSpawnCount(0);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_ADVANCE_BEGIN Ambushes=%d Groups=%d PerGroup=%d PathPoints=%d Room=%s"),
		Placed, Anchors.Num(), PerGroup, Corridor.Num(), *BossRoomLocation.ToCompactString());

	PlaceBossGate(Placement);
	CheckGateOpen();
}

bool ADediServerRPGGameMode::PlaceBossGate(const FDSTRBossGatePlacement& Placement)
{
	if (!Placement.bValid)
	{
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_GATE_PLACE_FAILED PathPoints=%d Room=%s"),
			AdvancePath.Num(), *BossRoomLocation.ToCompactString());
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADSTRBossGate* Gate = GetWorld()->SpawnActor<ADSTRBossGate>(
		ADSTRBossGate::StaticClass(),
		Placement.Location,
		Placement.Forward.Rotation(),
		SpawnParameters);
	BossGate = Gate;
	if (!Gate)
	{
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_GATE_PLACE_FAILED At=%s Spawned=0"),
			*Placement.Location.ToCompactString());
		return false;
	}

	Gate->OnGateEntered.AddUObject(this, &ADediServerRPGGameMode::HandleGateEntered);
	if (ADSTRGameState* State = GetGameState<ADSTRGameState>())
	{
		State->SetBossGate(Placement.Location);
		State->SetBossGateActor(Gate);
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_GATE Sealed At=%s Forward=%s Ambush=%d"),
		*Placement.Location.ToCompactString(),
		*Placement.Forward.ToCompactString(),
		AmbushEnemies.Num());
	return true;
}

void ADediServerRPGGameMode::SetGateState(const EDSTRGateState NewState)
{
	ADSTRBossGate* Gate = BossGate.Get();
	if (!Gate || Gate->GetGateState() == NewState)
	{
		return;
	}
	Gate->SetGateState(NewState);

	const ADSTRGameState* State = GetGameState<ADSTRGameState>();
	const TCHAR* Name = NewState == EDSTRGateState::Open ? TEXT("Open")
		: (NewState == EDSTRGateState::Locked ? TEXT("Locked") : TEXT("Sealed"));
	UE_LOG(LogDSTR, Log, TEXT("DSTR_GATE %s At=%s Time=%.2f"),
		Name,
		*Gate->GetActorLocation().ToCompactString(),
		State ? State->GetElapsedMatchSeconds() : 0.0f);
}

void ADediServerRPGGameMode::CheckGateOpen()
{
	ADSTRGameState* State = GetGameState<ADSTRGameState>();
	const ADSTRBossGate* Gate = BossGate.Get();
	if (!State || !Gate
		|| !FDSTRDungeonRules::ShouldOpenGate(Gate->IsSealed(), State->GetAmbushRemaining()))
	{
		return;
	}
	SetGateState(EDSTRGateState::Open);
	State->PushEvent(EDSTRMatchEventKind::GateOpened);
}

TArray<ADediServerRPGCharacter*> ADediServerRPGGameMode::GetPlayerCharacters() const
{
	TArray<ADediServerRPGCharacter*> Players;
	const AGameStateBase* State = GameState;
	if (!State)
	{
		return Players;
	}
	Players.Reserve(State->PlayerArray.Num());
	for (const APlayerState* Entry : State->PlayerArray)
	{
		if (ADediServerRPGCharacter* Character = Entry
			? Cast<ADediServerRPGCharacter>(Entry->GetPawn()) : nullptr)
		{
			Players.Add(Character);
		}
	}
	return Players;
}

void ADediServerRPGGameMode::CheckGateBypass()
{
	const ADSTRBossGate* Gate = BossGate.Get();
	if (!Gate->IsSealed())
	{
		ReportedGateBypass.Reset();
		return;
	}

	const ADSTRGameState* State = GetGameState<ADSTRGameState>();
	for (ADediServerRPGCharacter* Character : GetPlayerCharacters())
	{
		if (Character->IsDowned())
		{
			continue;
		}
		const float Distance = static_cast<float>(
			FVector::Dist2D(Character->GetActorLocation(), BossRoomLocation));
		if (!FDSTRDungeonRules::IsInsideArena(
			Character->GetActorLocation(), BossRoomLocation, ArenaRadius, ArenaHalfHeight))
		{
			continue;
		}
		bool bAlreadyReported = false;
		ReportedGateBypass.Add(Character, &bAlreadyReported);
		if (bAlreadyReported)
		{
			continue;
		}
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_GATE_BYPASS Player=%s At=%s Dist=%.0f Time=%.2f"),
			*GetNameSafe(Character),
			*Character->GetActorLocation().ToCompactString(),
			Distance,
			State ? State->GetElapsedMatchSeconds() : 0.0f);
	}
}

void ADediServerRPGGameMode::HandleGateEntered(APawn* EnteringPawn)
{
	const ADediServerRPGCharacter* Player = Cast<ADediServerRPGCharacter>(EnteringPawn);
	if (!Player || Player->IsDowned())
	{
		return;
	}
	const ADSTRGameState* State = GetGameState<ADSTRGameState>();
	UE_LOG(LogDSTR, Log, TEXT("DSTR_GATE_ENTER Player=%s At=%s Time=%.2f"),
		*GetNameSafe(Player),
		*Player->GetActorLocation().ToCompactString(),
		State ? State->GetElapsedMatchSeconds() : 0.0f);
	if (ADSTREnemyCharacter* Boss = DormantBoss.Get())
	{
		Boss->Wake(TEXT("GateEntry"));
	}
	SetGateState(EDSTRGateState::Locked);
}

void ADediServerRPGGameMode::EnforceArenaLock(const float DeltaSeconds)
{
	const ADSTRBossGate* Gate = BossGate.Get();
	if (!Gate->IsLocked())
	{
		SecondsOutsideArena.Reset();
		return;
	}

	const FVector Entry = Gate->GetEntryPoint();
	for (ADediServerRPGCharacter* Character : GetPlayerCharacters())
	{
		if (Character->IsDowned())
		{
			continue;
		}
		const bool bInside = FDSTRDungeonRules::IsInsideArena(
			Character->GetActorLocation(), BossRoomLocation, ArenaRadius, ArenaHalfHeight);
		float& Outside = SecondsOutsideArena.FindOrAdd(Character);
		Outside = bInside ? 0.0f : Outside + DeltaSeconds;
		if (!FDSTRDungeonRules::ShouldPullToArena(true, bInside, Outside))
		{
			continue;
		}

		const FVector Destination = Entry
			+ FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight());
		UE_LOG(LogDSTR, Log, TEXT("DSTR_ARENA_PULL Player=%s From=%s To=%s Seconds=%.1f"),
			*GetNameSafe(Character),
			*Character->GetActorLocation().ToCompactString(),
			*Destination.ToCompactString(),
			Outside);
		Character->TeleportTo(Destination, Character->GetActorRotation(), false, false);
		Outside = 0.0f;
	}
}

void ADediServerRPGGameMode::HandleEnemyAwakened(ADSTREnemyCharacter* Enemy)
{
	ADSTRGameState* State = GetGameState<ADSTRGameState>();
	if (!State || !Enemy || !Enemy->IsBoss())
	{
		return;
	}
	if (!State->AdvancePhase(EDSTRMatchPhase::Boss))
	{
		return;
	}

	State->SetRemainingEnemies(State->GetRemainingEnemies() + 1);
	State->SetBoss(Enemy);
	State->MarkBossIncoming();
	State->PushEvent(EDSTRMatchEventKind::BossAwakened);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_BOSS_AWAKE Name=%s At=%s Time=%.2f"),
		*Enemy->GetName(), *Enemy->GetActorLocation().ToCompactString(), State->GetElapsedMatchSeconds());
}

void ADediServerRPGGameMode::SpawnWave()
{
	WaveSpawnIndex = 0;
	WaveSpawnedCount = 0;

#if !UE_BUILD_SHIPPING
	if (IsStallSmokeEnabled(FCommandLine::Get()))
	{
		if (ADSTRGameState* State = GetGameState<ADSTRGameState>())
		{
			State->SetRemainingEnemies(GetWaveEnemyCount(GetNumPlayers()));
			State->SetPendingSpawnCount(0);
		}
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_STALL_SMOKE Wave=%d Suppressed=1"),
			GetWaveEnemyCount(GetNumPlayers()));
		return;
	}
#endif

	const TArray<FVector> PartyLocations = GetLivingPlayerLocations();
	int32 GatePick = FDSTRSpawnRules::PickNearestTo(
		DoorSpawnPoints,
		FDSTRSpawnRules::FilterUsable(DoorSpawnPoints, PartyLocations),
		FDSTRSpawnRules::PartyCentre(PartyLocations));
	if (GatePick == INDEX_NONE)
	{
		GatePick = FDSTRSpawnRules::PickFarthest(DoorSpawnPoints, PartyLocations);
	}
	bWaveSpawnFromDoor = GatePick != INDEX_NONE;
	WaveSpawnGateLocation = bWaveSpawnFromDoor ? DoorSpawnPoints[GatePick] : FVector::ZeroVector;
	WaveSpawnTotal = bWaveSpawnFromDoor
		? GetWaveEnemyCount(GetNumPlayers())
		: FMath::Min(GetWaveEnemyCount(GetNumPlayers()), GetWaveSpawnOffsets().Num());
	const FVector Gate = bWaveSpawnFromDoor ? WaveSpawnGateLocation : EncounterOrigin;

	if (ADSTRGameState* State = GetGameState<ADSTRGameState>())
	{
		State->SetRemainingEnemies(WaveSpawnTotal);
		State->SetWaveSpawnGate(Gate);
		State->SetPendingSpawnCount(WaveSpawnTotal);
		State->PushEvent(EDSTRMatchEventKind::WaveIncoming);
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_WAVE_SPAWN Count=%d Gate=%s Door=%d"),
		WaveSpawnTotal, *Gate.ToCompactString(), GatePick != INDEX_NONE ? 1 : 0);

	SpawnNextWaveEnemy();
	if (WaveSpawnIndex < WaveSpawnTotal)
	{
		GetWorldTimerManager().SetTimer(
			WaveSpawnTimer,
			this,
			&ADediServerRPGGameMode::SpawnNextWaveEnemy,
			FDSTRSpawnRules::SpawnInterval,
			true);
	}
}

void ADediServerRPGGameMode::SpawnNextWaveEnemy()
{
	if (WaveSpawnIndex >= WaveSpawnTotal)
	{
		GetWorldTimerManager().ClearTimer(WaveSpawnTimer);
		return;
	}

	const int32 Index = WaveSpawnIndex++;
	const TArray<FVector> PlayerLocations = GetLivingPlayerLocations();
	const FVector SpawnLocation = bWaveSpawnFromDoor
		? WaveSpawnGateLocation + FVector(0.0f, 0.0f, ADSTREnemyCharacter::GetSpawnGroundClearance(false))
		: EncounterOrigin + GetWaveSpawnOffsets()[Index % GetWaveSpawnOffsets().Num()];
	const FVector PartyCentre = PlayerLocations.IsEmpty()
		? EncounterOrigin : FDSTRSpawnRules::PartyCentre(PlayerLocations);
	const FRotator SpawnRotation(0.0f, FDSTRSpawnRules::FacingYaw(SpawnLocation, PartyCentre), 0.0f);

	const ADSTREnemyCharacter* Enemy = SpawnEnemyAt(SpawnLocation, SpawnRotation);
	WaveSpawnedCount += Enemy ? 1 : 0;
	UE_LOG(LogDSTR, Log, TEXT("DSTR_ENEMY_SPAWN Index=%d At=%s DistToNearestPlayer=%.0f Door=%d Spawned=%d Yaw=%.0f"),
		Index,
		*SpawnLocation.ToCompactString(),
		FMath::Min(FDSTRSpawnRules::DistanceToNearestPlayer(SpawnLocation, PlayerLocations), 999999.0f),
		bWaveSpawnFromDoor ? 1 : 0,
		Enemy ? 1 : 0,
		SpawnRotation.Yaw);

	if (ADSTRGameState* State = GetGameState<ADSTRGameState>())
	{
		State->SetPendingSpawnCount(WaveSpawnTotal - WaveSpawnIndex);
		if (!Enemy)
		{
			State->SetRemainingEnemies(State->GetRemainingEnemies() - 1);
			CheckWaveCleared();
		}
	}
	if (WaveSpawnIndex >= WaveSpawnTotal)
	{
		GetWorldTimerManager().ClearTimer(WaveSpawnTimer);
		const ADSTRGameState* State = GetGameState<ADSTRGameState>();
		if (WaveSpawnedCount == 0 && State && State->GetMatchPhase() == EDSTRMatchPhase::Wave)
		{
			FailMatch(TEXT("WaveSpawnFailed"));
		}
	}
}

void ADediServerRPGGameMode::HandleEnemyDefeated(ADSTREnemyCharacter* Enemy)
{
	ADSTRGameState* State = GetGameState<ADSTRGameState>();
	if (!State || !Enemy)
	{
		return;
	}

	State->SetRemainingEnemies(State->GetRemainingEnemies() - 1);
	if (Enemy->IsBoss())
	{
		State->SetBoss(nullptr);
		SetGateState(EDSTRGateState::Open);
		State->AdvancePhase(EDSTRMatchPhase::Clear);
		State->PushEvent(EDSTRMatchEventKind::Clear);
		UE_LOG(LogDSTR, Log, TEXT("DSTR_CLEAR Players=%d Time=%.2f"),
			GetNumPlayers(), State->GetElapsedMatchSeconds());
		return;
	}

	if (AmbushEnemies.Remove(Enemy) > 0)
	{
		AmbushEnemies.RemoveAll([](const TWeakObjectPtr<ADSTREnemyCharacter>& Item) { return !Item.IsValid(); });
		State->SetAmbushRemaining(AmbushEnemies.Num());
		CheckGateOpen();
	}
	CheckWaveCleared();
}

void ADediServerRPGGameMode::CheckWaveCleared()
{
	ADSTRGameState* State = GetGameState<ADSTRGameState>();
	if (!State || State->GetMatchPhase() != EDSTRMatchPhase::Wave || State->GetRemainingEnemies() > 0)
	{
		return;
	}

	State->PushEvent(EDSTRMatchEventKind::WaveCleared);
	for (ADediServerRPGCharacter* Character : GetPlayerCharacters())
	{
		UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
		if (!Character->IsDowned() && ASC)
		{
			ASC->ApplyGameplayEffectToSelf(
				GetDefault<UDSTREncounterRecoveryEffect>(),
				1.0f,
				ASC->MakeEffectContext());
			UE_LOG(LogDSTR, Log,
				TEXT("DSTR_ENCOUNTER_RECOVERY Player=%s"),
				*Character->GetName());
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADSTRAttackBuffPickup* Pickup = GetWorld()->SpawnActor<ADSTRAttackBuffPickup>(
		ADSTRAttackBuffPickup::StaticClass(),
		EncounterOrigin + FVector(0.0f, 0.0f, 120.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_PICKUP_SPAWN Name=%s"), *GetNameSafe(Pickup));
	BeginAdvance();
}

void ADediServerRPGGameMode::HandlePlayerDowned(ADediServerRPGCharacter* Character)
{
	const APlayerState* PlayerState = Character ? Character->GetPlayerState() : nullptr;
	if (ADSTRGameState* State = GetGameState<ADSTRGameState>())
	{
		State->RecordDowned(PlayerState ? PlayerState->GetPlayerName() : GetNameSafe(Character));
	}
	CheckPartyWiped();
}

void ADediServerRPGGameMode::CheckPartyWiped()
{
	int32 PlayerCount = 0;
	int32 StandingCount = 0;
	for (const ADediServerRPGCharacter* Character : GetPlayerCharacters())
	{
		++PlayerCount;
		StandingCount += Character->IsDowned() ? 0 : 1;
	}
	if (FDSTRDownedRules::IsPartyLost(PlayerCount, StandingCount))
	{
		FailMatch(TEXT("AllPlayersDown"));
	}
}

void ADediServerRPGGameMode::TickBleedOut()
{
	const ADSTRGameState* State = GetGameState<ADSTRGameState>();
	if (!State || !FDSTRStallRules::IsMatchRunning(State->GetMatchPhase()))
	{
		return;
	}
	const float Now = State->GetServerWorldTimeSeconds();
	for (ADediServerRPGCharacter* Character : GetPlayerCharacters())
	{
		const ADSTRPlayerState* PlayerState = Character->GetPlayerState<ADSTRPlayerState>();
		if (PlayerState && FDSTRDownedRules::HasBledOut(PlayerState->GetBleedOutEndServerTime(), Now))
		{
			EliminatePlayer(Character);
		}
	}
}

void ADediServerRPGGameMode::EliminatePlayer(ADediServerRPGCharacter* Character)
{
	ADSTRPlayerState* PlayerState = Character ? Character->GetPlayerState<ADSTRPlayerState>() : nullptr;
	if (!PlayerState || PlayerState->IsEliminated())
	{
		return;
	}
	PlayerState->MarkEliminated();

	ADSTRGameState* State = GetGameState<ADSTRGameState>();
	const FString PlayerName = PlayerState->GetPlayerName();
	UE_LOG(LogDSTR, Warning, TEXT("DSTR_BLEEDOUT Player=%s Seconds=%.1f Time=%.2f"),
		*PlayerName,
		FDSTRDownedRules::BleedOutSeconds,
		State ? State->GetElapsedMatchSeconds() : 0.0f);
	if (State)
	{
		State->PushEvent(EDSTRMatchEventKind::PlayerEliminated, PlayerName);
	}
	CheckPartyWiped();
}

FDSTRMatchProgress ADediServerRPGGameMode::CaptureProgress() const
{
	FDSTRMatchProgress Progress;
	const ADSTRGameState* State = GetGameState<ADSTRGameState>();
	if (!State)
	{
		return Progress;
	}
	const ADSTRBossGate* Gate = BossGate.Get();
	Progress.Phase = static_cast<uint8>(State->GetMatchPhase());
	Progress.GateState = static_cast<uint8>(Gate ? Gate->GetGateState() : EDSTRGateState::Sealed);
	Progress.RemainingEnemies = State->GetRemainingEnemies();
	Progress.AmbushRemaining = State->GetAmbushRemaining();
	Progress.PendingSpawnCount = State->GetPendingSpawnCount();
	Progress.DownCount = State->GetDownCount();
	Progress.ReviveCount = State->GetReviveCount();
	const ADSTREnemyCharacter* Boss = State->GetBoss();
	const UDSTRAttributeSet* BossAttributes = Boss ? Boss->GetAttributeSet() : nullptr;
	Progress.BossHealth = BossAttributes ? FMath::FloorToInt(BossAttributes->GetHealth()) : -1;
	// 넓은 던전에서는 보스방으로 걸어가는 동안 나머지 지표가 전부 고정돼 멈춤으로 오탐된다.
	float NearestToBossRoom = TNumericLimits<float>::Max();
	for (const ADediServerRPGCharacter* Character : GetPlayerCharacters())
	{
		if (Character->IsDowned())
		{
			continue;
		}
		++Progress.PlayersStanding;
		NearestToBossRoom = FMath::Min(NearestToBossRoom, static_cast<float>(
			FVector::Dist(Character->GetActorLocation(), BossRoomLocation)));
	}
	Progress.BossApproachStep = NearestToBossRoom < TNumericLimits<float>::Max()
		? FMath::FloorToInt(NearestToBossRoom / FDSTRStallRules::BossApproachStepSize)
		: 0;
	return Progress;
}

void ADediServerRPGGameMode::TickStallWatchdog(const float DeltaSeconds)
{
	const ADSTRGameState* State = GetGameState<ADSTRGameState>();
	if (!State || !FDSTRStallRules::IsMatchRunning(State->GetMatchPhase()))
	{
		return;
	}

	const FDSTRMatchProgress Current = CaptureProgress();
	if (FDSTRStallRules::HasProgressed(LastProgress, Current))
	{
		LastProgress = Current;
		SecondsSinceProgress = 0.0f;
		return;
	}
	SecondsSinceProgress += DeltaSeconds;
	if (!FDSTRStallRules::ShouldFailForStall(true, SecondsSinceProgress))
	{
		return;
	}
	UE_LOG(LogDSTR, Warning,
		TEXT("DSTR_STALL Phase=%s Seconds=%.1f Hostiles=%d Ambush=%d Pending=%d Standing=%d"),
		*State->GetPhaseDisplayName(),
		SecondsSinceProgress,
		Current.RemainingEnemies,
		Current.AmbushRemaining,
		Current.PendingSpawnCount,
		Current.PlayersStanding);
	FailMatch(TEXT("Stalled"));
}

void ADediServerRPGGameMode::FailMatch(const TCHAR* Reason)
{
	ADSTRGameState* State = GetGameState<ADSTRGameState>();
	if (!State || State->GetMatchPhase() == EDSTRMatchPhase::Clear
		|| State->GetMatchPhase() == EDSTRMatchPhase::Failed)
	{
		return;
	}

	if (State->AdvancePhase(EDSTRMatchPhase::Failed))
	{
		GetWorldTimerManager().ClearTimer(WaveSpawnTimer);
		WaveSpawnIndex = WaveSpawnTotal;
		State->SetPendingSpawnCount(0);
		State->PushEvent(EDSTRMatchEventKind::Failed);
		State->SetBoss(nullptr);
		for (TActorIterator<ADSTREnemyCharacter> It(GetWorld()); It; ++It)
		{
			It->Destroy();
		}
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_FAILED Reason=%s"), Reason);
	}
}

FVector ADediServerRPGGameMode::GetEncounterOrigin() const
{
	// 접속 순서 대신 시작 지점 중심을 고정 기준으로 쓴다.
	TArray<FVector> StartLocations;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		StartLocations.Add(It->GetActorLocation());
	}
	return GetStableEncounterOrigin(StartLocations);
}

FVector ADediServerRPGGameMode::GetStableEncounterOrigin(const TArray<FVector>& PlayerStarts)
{
	return FDSTRSpawnRules::PartyCentre(PlayerStarts);
}
