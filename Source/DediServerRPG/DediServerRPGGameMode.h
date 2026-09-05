// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/DSTRDungeonRules.h"
#include "Game/DSTRStallRules.h"
#include "Game/DSTRServerStatus.h"
#include "Game/DSTRRoomSession.h"
#include "GameFramework/GameModeBase.h"
#include "DediServerRPGGameMode.generated.h"

class ADediServerRPGCharacter;
class ADSTRBossGate;
class ADSTREnemyCharacter;
class UNavigationSystemV1;
enum class EDSTRGateState : uint8;

UCLASS(minimalapi)
class ADediServerRPGGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADediServerRPGGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PreLogin(
		const FString& Options,
		const FString& Address,
		const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;
	virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal,
		const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static bool ShouldAutoStart(const FString& CommandLine);
	bool TryStartMatchByHost(APlayerController* Requester);
	static float GetEncounterStartDelay(int32 PlayerCount);
	static float GetCaptureTimeDilation(const FString& CommandLine);
	static int32 GetWaveEnemyCount(int32 PlayerCount);
	static TConstArrayView<FVector> GetWaveSpawnOffsets();
	static FVector GetBossSpawnOffset();
	static bool IsReviveSmokeEnabled(const FString& CommandLine);
	static bool IsStallSmokeEnabled(const FString& CommandLine);
	static bool CanAcceptPlayerCount(int32 CurrentPlayerCount) { return CurrentPlayerCount < FDSTRServerStatus::MaxPlayers; }
	static FVector GetStableEncounterOrigin(const TArray<FVector>& PlayerStarts);
	void HandlePlayerDowned(ADediServerRPGCharacter* Character);
	FVector GetMatchOrigin() const { return EncounterOrigin; }

private:
	FString GetAdmissionError(const FString& Options);
	FDSTRRoomSession RoomSession;
	void ResetEmptyDedicatedServer();
	void BeginStartCountdown();
	void TryAutoStart();
	void StartEncounter();
	void SpawnWave();
	void SpawnNextWaveEnemy();
	bool ResolveBossRoom();
	bool ResolveBossRoomFromMarker(UWorld* World, UNavigationSystemV1* Navigation);
	bool ResolveBossRoomByScan(UWorld* World, UNavigationSystemV1* Navigation);
	TArray<FVector> CollectFloorTileCentres(TArray<FVector>& OutTiles) const;
	bool PlaceDormantBoss();
	void BeginAdvance();
	bool PlaceBossGate(const struct FDSTRBossGatePlacement& Placement);
	void SetGateState(EDSTRGateState NewState);
	void CheckGateOpen();
	void CheckGateBypass();
	void EnforceArenaLock(float DeltaSeconds);
	void HandleGateEntered(APawn* EnteringPawn);
	TArray<ADediServerRPGCharacter*> GetPlayerCharacters() const;
	void HandleEnemyDefeated(ADSTREnemyCharacter* Enemy);
	void HandleEnemyAwakened(ADSTREnemyCharacter* Enemy);
	void CheckWaveCleared();
	void RunNavProbe();
	void TriggerReviveSmoke();
	void TickBleedOut();
	void EliminatePlayer(ADediServerRPGCharacter* Character);
	void CheckPartyWiped();
	FDSTRMatchProgress CaptureProgress() const;
	void TickStallWatchdog(float DeltaSeconds);
	void FailMatch(const TCHAR* Reason);
	FVector GetEncounterOrigin() const;
	TArray<FVector> CollectDoorSpawnPoints() const;
	TArray<FVector> GetLivingPlayerLocations() const;
	ADSTREnemyCharacter* SpawnEnemyAt(const FVector& Location, const FRotator& Rotation);

	bool bEncounterStarted = false;
	FVector EncounterOrigin = FVector::ZeroVector;
	FTimerHandle EncounterStartTimer;
	FTimerHandle AutoStartTimer;
	FTimerHandle ReviveSmokeTimer;
	FTimerHandle WaveSpawnTimer;
	FTimerHandle NavProbeTimer;
	int32 NavProbeAttempts = 0;
	float EncounterNavWaitSeconds = 0.0f;
	TArray<FVector> DoorSpawnPoints;
	FVector BossRoomLocation = FVector::ZeroVector;
	float ArenaRadius = FDSTRDungeonRules::BossRoomRadius;
	float ArenaHalfHeight = FDSTRDungeonRules::BossRoomHalfHeight;
	TArray<FVector> AdvancePath;
	TWeakObjectPtr<ADSTREnemyCharacter> DormantBoss;
	TWeakObjectPtr<ADSTRBossGate> BossGate;
	TArray<TWeakObjectPtr<ADSTREnemyCharacter>> AmbushEnemies;
	TMap<TWeakObjectPtr<ADediServerRPGCharacter>, float> SecondsOutsideArena;
	TSet<TWeakObjectPtr<ADediServerRPGCharacter>> ReportedGateBypass;
	FVector WaveSpawnGateLocation = FVector::ZeroVector;
	bool bWaveSpawnFromDoor = false;
	int32 WaveSpawnIndex = 0;
	int32 WaveSpawnTotal = 0;
	int32 WaveSpawnedCount = 0;
	bool bReviveSmokeScheduled = false;
	FDSTRMatchProgress LastProgress;
	float SecondsSinceProgress = 0.0f;
};
