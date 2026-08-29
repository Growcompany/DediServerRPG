#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/GameStateBase.h"
#include "DSTRGameState.generated.h"

UENUM(BlueprintType)
enum class EDSTRMatchPhase : uint8
{
	WaitingForPlayers,
	Wave,
	Advance,
	Boss,
	Clear,
	Failed
};

UENUM()
enum class EDSTRGateState : uint8
{
	Sealed,
	Open,
	Locked
};

UENUM()
enum class EDSTRMatchEventKind : uint8
{
	MatchStart,
	PlayerDowned,
	PlayerRevived,
	WaveCleared,
	BossSpawned,
	BuffPicked,
	Clear,
	Failed,
	WaveIncoming,
	BossIncoming,
	BossAwakened,
	GateOpened,
	PlayerEliminated
};

USTRUCT()
struct FDSTRMatchEvent
{
	GENERATED_BODY()

	UPROPERTY()
	EDSTRMatchEventKind Kind = EDSTRMatchEventKind::MatchStart;

	UPROPERTY()
	FString Subject;

	UPROPERTY()
	FString Object;

	UPROPERTY()
	float ServerTime = 0.0f;
};

class ADSTRBossGate;
class ADSTREnemyCharacter;

UCLASS()
class DEDISERVERRPG_API ADSTRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxRecentEvents = 6;
	static constexpr float StartCountdownSeconds = 3.0f;

	ADSTRGameState();

	static bool IsValidPhaseTransition(EDSTRMatchPhase From, EDSTRMatchPhase To);
	static void TrimEvents(TArray<FDSTRMatchEvent>& Events, int32 MaxEvents = MaxRecentEvents);

	void StartMatchFlow();
	bool AdvancePhase(EDSTRMatchPhase NewPhase);
	void SetRemainingEnemies(int32 NewRemainingEnemies);
	void SetBoss(ADSTREnemyCharacter* NewBoss);
	void MarkBossIncoming();
	void SetWaveSpawnGate(const FVector& Gate);
	void SetPendingSpawnCount(int32 Count);
	void SetBossRoom(const FVector& Room);
	void SetAdvancePath(const TArray<FVector>& Path);
	void SetBossGate(const FVector& Gate);
	void SetBossGateActor(ADSTRBossGate* Gate);
	void SetAmbushRemaining(int32 Count);
	void SetStartCountdown(float Seconds);
	void ClearStartCountdown();
	void RecordDowned(const FString& PlayerName);
	void RecordRevived(const FString& ReviverName, const FString& TargetName);
	void PushEvent(EDSTRMatchEventKind Kind, const FString& Subject = FString(), const FString& Object = FString());

	EDSTRMatchPhase GetMatchPhase() const { return MatchPhase; }
	int32 GetRemainingEnemies() const { return RemainingEnemies; }
	FVector GetWaveSpawnGate() const { return WaveSpawnGate; }
	int32 GetPendingSpawnCount() const { return PendingSpawnCount; }
	FVector GetBossRoom() const { return BossRoomLocation; }
	const TArray<FVector>& GetAdvancePath() const { return AdvancePath; }
	FVector GetBossGate() const { return BossGateLocation; }
	ADSTRBossGate* GetBossGateActor() const { return BossGateActor; }
	int32 GetAmbushRemaining() const { return AmbushRemaining; }
	ADSTREnemyCharacter* GetBoss() const { return Boss; }
	float GetElapsedMatchSeconds() const;
	float GetSecondsSinceBossSpawn() const;
	bool IsCountdownActive() const { return CountdownEndServerTime > 0.0f; }
	float GetCountdownRemaining() const;
	int32 GetDownCount() const { return DownCount; }
	int32 GetReviveCount() const { return ReviveCount; }
	const TArray<FDSTRMatchEvent>& GetRecentEvents() const { return RecentEvents; }
	FString GetPhaseDisplayName() const;

	int32 GetHostPlayerId() const { return HostPlayerId; }
	bool IsHost(const APlayerState* PlayerState) const;
	int32 CountPresentationReadyPlayers() const;
	void SetHostPlayerId(int32 NewHostPlayerId);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase)
	EDSTRMatchPhase MatchPhase = EDSTRMatchPhase::WaitingForPlayers;

	UFUNCTION()
	void OnRep_MatchPhase();

	UPROPERTY(Replicated)
	int32 RemainingEnemies = 0;

	UPROPERTY(Replicated)
	FVector_NetQuantize WaveSpawnGate = FVector::ZeroVector;

	UPROPERTY(Replicated)
	int32 PendingSpawnCount = 0;

	UPROPERTY(Replicated)
	FVector_NetQuantize BossRoomLocation = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_AdvancePath)
	TArray<FVector_NetQuantize> AdvancePathPoints;

	UFUNCTION()
	void OnRep_AdvancePath();

	TArray<FVector> AdvancePath;

	UPROPERTY(Replicated)
	FVector_NetQuantize BossGateLocation = FVector::ZeroVector;

	UPROPERTY(Replicated)
	TObjectPtr<ADSTRBossGate> BossGateActor;

	UPROPERTY(Replicated)
	int32 AmbushRemaining = 0;

	UPROPERTY(Replicated)
	TObjectPtr<ADSTREnemyCharacter> Boss;

	UPROPERTY(Replicated)
	float MatchStartServerTime = 0.0f;

	UPROPERTY(Replicated)
	float BossSpawnServerTime = 0.0f;

	UPROPERTY(Replicated)
	float CountdownEndServerTime = 0.0f;

	UPROPERTY(Replicated)
	int32 DownCount = 0;

	UPROPERTY(Replicated)
	int32 ReviveCount = 0;

	UPROPERTY(Replicated)
	TArray<FDSTRMatchEvent> RecentEvents;

	UPROPERTY(Replicated)
	int32 HostPlayerId = -1;
};
