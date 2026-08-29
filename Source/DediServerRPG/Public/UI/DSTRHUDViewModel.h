#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Game/DSTRGameState.h"

struct FDSTRStatusLineInput
{
	EDSTRMatchPhase Phase = EDSTRMatchPhase::WaitingForPlayers;
	float CountdownRemaining = 0.0f;
	bool bIsHost = false;
	bool bSelfDowned = false;
	bool bTeammateDowned = false;
	bool bBuffActive = false;
	float SecondsSinceBossSpawn = TNumericLimits<float>::Max();
	float BleedOutRemaining = 0.0f;
	bool bSelfEliminated = false;
};

enum class EDSTRObjectiveKind : uint8
{
	None,
	Revive,
	Buff,
	Boss,
	Wave,
	Gate,
	BossIncoming,
	ClearPath,
	EnterChamber
};

struct FDSTRObjectiveInput
{
	EDSTRMatchPhase Phase = EDSTRMatchPhase::WaitingForPlayers;
	bool bSelfDowned = false;
	bool bDownedTeammate = false;
	FString DownedTeammateName;
	bool bReviveInReach = false;
	bool bBuffNearby = false;
	bool bBossAlive = false;
	FString BossName;
	float BossHealth = 0.0f;
	float BossMaxHealth = 0.0f;
	int32 RemainingEnemies = 0;
	int32 LivingEnemyCount = 0;
	int32 PendingSpawnCount = 0;
	EDSTRGateState GateState = EDSTRGateState::Sealed;
	int32 AmbushRemaining = 0;
	float ReviveSecondsLeft = 0.0f;
};

struct DEDISERVERRPG_API FDSTRHUDViewModel
{
	static constexpr const TCHAR* InteractKeyLabel = TEXT("F");

	static float SafeRatio(float Value, float Maximum);
	static FString FormatCooldown(float RemainingSeconds);
	static FString FormatClock(float ElapsedSeconds);
	static FString FormatNetworkRole(ENetMode NetMode, int32 PlayerCount);

	static constexpr int32 MaxPartyNameLength = 12;
	static constexpr int32 MaxPartyRowLength = 32;
	static FString FormatPartyRow(
		int32 Slot, const FString& PlayerName, float Health, bool bDowned, bool bEliminated = false);

	static constexpr float BossIncomingSeconds = 3.0f;
	static constexpr float EventFadeStartSeconds = 6.0f;
	static constexpr float EventFadeEndSeconds = 10.0f;
	static FString FormatCountdown(float RemainingSeconds);
	static FString FormatPhase(EDSTRMatchPhase Phase, bool bCountdownActive, float CountdownRemaining);
	static FString FormatSecondsLeft(float RemainingSeconds);
	static constexpr int32 MaxStatusLineLength = 34;
	static FString FormatStatusLine(const FDSTRStatusLineInput& Input);
	static FString FormatResultSummary(float ElapsedSeconds, int32 Downs, int32 Revives, int32 Alive, int32 Total);
	static constexpr int32 MaxEventLineLength = 33;
	static FString FormatEvent(EDSTRMatchEventKind Kind, const FString& Subject, const FString& Object);
	static float EventOpacity(float AgeSeconds);

	static constexpr float ObjectiveBuffRange = 1200.0f;
	static constexpr int32 MaxObjectiveLength = 41;
	static constexpr float ObjectiveRetargetRatio = 0.6f;
	static constexpr float ObjectiveReviveRange = 250.0f;
	static constexpr float NearTargetPinRange = 500.0f;
	static EDSTRObjectiveKind GetObjectiveKind(const FDSTRObjectiveInput& Input);
	static FString FormatObjective(const FDSTRObjectiveInput& Input);
	static bool ShouldRetarget(float CurrentDistance, float CandidateDistance);
	static bool ShouldPinAtTarget(float DistanceCentimeters);

	static FVector2D ProjectToScreenEdge(
		FVector2D ViewportSize, FVector2D ScreenPos, bool bBehindCamera, float Inset, bool& bOffScreen);
	static float ScreenEdgeAngle(FVector2D ViewportSize, FVector2D EdgePos);
	static FString FormatDistanceMeters(float Centimeters);
	static FVector2D ViewportToDesign(FVector2D ViewportSize, FVector2D DesignSize, FVector2D ScreenPos);
	static FString FormatInteractLabel(bool bReviveTarget, bool bPickupTarget);
};
