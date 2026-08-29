#include "UI/DSTRHUDViewModel.h"

float FDSTRHUDViewModel::SafeRatio(const float Value, const float Maximum)
{
	return Maximum > KINDA_SMALL_NUMBER
		? FMath::Clamp(Value / Maximum, 0.0f, 1.0f)
		: 0.0f;
}

FString FDSTRHUDViewModel::FormatCooldown(const float RemainingSeconds)
{
	return RemainingSeconds > KINDA_SMALL_NUMBER
		? FString::Printf(TEXT("%.1fs"), RemainingSeconds)
		: FString(TEXT("READY"));
}

FString FDSTRHUDViewModel::FormatClock(const float ElapsedSeconds)
{
	const int32 TotalSeconds = FMath::Max(0, FMath::FloorToInt(ElapsedSeconds));
	return FString::Printf(TEXT("%02d:%02d"), TotalSeconds / 60, TotalSeconds % 60);
}

FString FDSTRHUDViewModel::FormatNetworkRole(
	const ENetMode NetMode,
	const int32 PlayerCount)
{
	switch (NetMode)
	{
	case NM_Client:
		return FString::Printf(
			TEXT("DEDICATED SERVER  /  CLIENT %d/4"),
			FMath::Clamp(PlayerCount, 1, 4));
	case NM_ListenServer:
		return TEXT("LISTEN SERVER TEST");
	case NM_DedicatedServer:
		return TEXT("DEDICATED SERVER");
	default:
		return TEXT("STANDALONE PREVIEW");
	}
}

FString FDSTRHUDViewModel::FormatPartyRow(
	const int32 Slot,
	const FString& PlayerName,
	const float Health,
	const bool bDowned,
	const bool bEliminated)
{
	const FString DisplayName = (PlayerName.IsEmpty() ? FString(TEXT("-")) : PlayerName)
		.Left(MaxPartyNameLength)
		.RightPad(MaxPartyNameLength);
	const TCHAR* Condition = bDowned ? (bEliminated ? TEXT("LOST") : TEXT("DOWN")) : TEXT("ACTIVE");
	return FString::Printf(
		TEXT("%d %s HP %3.0f %s"),
		Slot,
		*DisplayName,
		FMath::Max(0.0f, Health),
		Condition);
}

FString FDSTRHUDViewModel::FormatSecondsLeft(const float RemainingSeconds)
{
	return FString::Printf(TEXT("%dS LEFT"), FMath::Max(1, FMath::CeilToInt(RemainingSeconds)));
}

FString FDSTRHUDViewModel::FormatCountdown(const float RemainingSeconds)
{
	return FString::Printf(TEXT("STARTING IN %d"), FMath::Max(1, FMath::CeilToInt(RemainingSeconds)));
}

FString FDSTRHUDViewModel::FormatPhase(
	const EDSTRMatchPhase Phase,
	const bool bCountdownActive,
	const float CountdownRemaining)
{
	switch (Phase)
	{
	case EDSTRMatchPhase::WaitingForPlayers:
		return bCountdownActive ? FormatCountdown(CountdownRemaining) : TEXT("LOBBY - WAITING FOR HOST");
	case EDSTRMatchPhase::Wave: return TEXT("WAVE 1 - CLEAR THE ENEMIES");
	case EDSTRMatchPhase::Advance: return TEXT("ADVANCE - REACH THE BOSS CHAMBER");
	case EDSTRMatchPhase::Boss: return TEXT("BOSS - COOPERATE AND SURVIVE");
	case EDSTRMatchPhase::Clear: return TEXT("DUNGEON CLEAR");
	case EDSTRMatchPhase::Failed: return TEXT("PARTY DEFEATED");
	default: return TEXT("UNKNOWN");
	}
}

FString FDSTRHUDViewModel::FormatStatusLine(const FDSTRStatusLineInput& Input)
{
	if (Input.CountdownRemaining > 0.0f)
	{
		return FormatCountdown(Input.CountdownRemaining);
	}
	if (Input.Phase == EDSTRMatchPhase::WaitingForPlayers)
	{
		return Input.bIsHost ? TEXT("PRESS START WHEN EVERYONE IS IN") : TEXT("HOST STARTS THE DUNGEON");
	}
	if (Input.Phase == EDSTRMatchPhase::Clear || Input.Phase == EDSTRMatchPhase::Failed)
	{
		return FString();
	}
	if (Input.bSelfDowned)
	{
		if (Input.bSelfEliminated)
		{
			return TEXT("YOU ARE OUT - THE PARTY FIGHTS ON");
		}
		return Input.BleedOutRemaining > 0.0f
			? FString::Printf(TEXT("YOU ARE DOWN - %s"), *FormatSecondsLeft(Input.BleedOutRemaining))
			: FString(TEXT("YOU ARE DOWN - WAIT FOR A TEAMMATE"));
	}
	if (Input.bTeammateDowned)
	{
		return FString::Printf(TEXT("PARTY MEMBER DOWN - PRESS %s NEARBY"), InteractKeyLabel);
	}
	if (Input.SecondsSinceBossSpawn < BossIncomingSeconds)
	{
		return TEXT("BOSS INCOMING");
	}
	if (Input.bBuffActive)
	{
		return TEXT("ATTACK BUFF ACTIVE");
	}
	return FString();
}

FString FDSTRHUDViewModel::FormatResultSummary(
	const float ElapsedSeconds, const int32 Downs, const int32 Revives, const int32 Alive, const int32 Total)
{
	return FString::Printf(TEXT("TIME %s   |   DOWNS %d   |   REVIVES %d   |   PARTY %d/%d"),
		*FormatClock(ElapsedSeconds), FMath::Max(0, Downs), FMath::Max(0, Revives),
		FMath::Clamp(Alive, 0, FMath::Max(0, Total)), FMath::Max(0, Total));
}

FString FDSTRHUDViewModel::FormatEvent(const EDSTRMatchEventKind Kind, const FString& Subject, const FString& Object)
{
	const FString Who = Subject.IsEmpty() ? FString(TEXT("SOMEONE")) : Subject.Left(MaxPartyNameLength).ToUpper();
	const FString Whom = Object.IsEmpty() ? FString(TEXT("A TEAMMATE")) : Object.Left(MaxPartyNameLength).ToUpper();
	switch (Kind)
	{
	case EDSTRMatchEventKind::MatchStart: return TEXT("MATCH START");
	case EDSTRMatchEventKind::PlayerDowned: return Who + TEXT(" IS DOWN");
	case EDSTRMatchEventKind::PlayerEliminated: return Who + TEXT(" BLED OUT");
	case EDSTRMatchEventKind::PlayerRevived: return Who + TEXT(" REVIVED ") + Whom;
	case EDSTRMatchEventKind::WaveCleared: return TEXT("WAVE CLEARED");
	case EDSTRMatchEventKind::BossSpawned: return TEXT("BOSS ") + Who + TEXT(" APPEARED");
	case EDSTRMatchEventKind::BuffPicked: return Who + TEXT(" TOOK ATTACK BUFF");
	case EDSTRMatchEventKind::WaveIncoming: return TEXT("ENEMIES INCOMING");
	case EDSTRMatchEventKind::BossIncoming: return TEXT("BOSS INCOMING");
	case EDSTRMatchEventKind::BossAwakened: return TEXT("THE BOSS AWAKENS");
	case EDSTRMatchEventKind::GateOpened: return TEXT("THE GATE OPENS");
	case EDSTRMatchEventKind::Clear: return TEXT("DUNGEON CLEAR");
	case EDSTRMatchEventKind::Failed: return TEXT("PARTY DEFEATED");
	default: return FString();
	}
}

float FDSTRHUDViewModel::EventOpacity(const float AgeSeconds)
{
	if (AgeSeconds <= EventFadeStartSeconds)
	{
		return 1.0f;
	}
	const float Alpha = FMath::Clamp(
		(AgeSeconds - EventFadeStartSeconds) / (EventFadeEndSeconds - EventFadeStartSeconds), 0.0f, 1.0f);
	return FMath::Lerp(1.0f, 0.15f, Alpha);
}

EDSTRObjectiveKind FDSTRHUDViewModel::GetObjectiveKind(const FDSTRObjectiveInput& Input)
{
	if (Input.Phase == EDSTRMatchPhase::WaitingForPlayers
		|| Input.Phase == EDSTRMatchPhase::Clear
		|| Input.Phase == EDSTRMatchPhase::Failed)
	{
		return EDSTRObjectiveKind::None;
	}
	if (Input.bSelfDowned)
	{
		return EDSTRObjectiveKind::None;
	}
	if (Input.bDownedTeammate)
	{
		return EDSTRObjectiveKind::Revive;
	}
	if (Input.bBuffNearby && Input.LivingEnemyCount <= 0)
	{
		return EDSTRObjectiveKind::Buff;
	}
	if (Input.bBossAlive)
	{
		return EDSTRObjectiveKind::Boss;
	}
	if (Input.Phase == EDSTRMatchPhase::Boss)
	{
		return EDSTRObjectiveKind::BossIncoming;
	}
	if (Input.Phase == EDSTRMatchPhase::Advance)
	{
		return Input.GateState == EDSTRGateState::Sealed
			? EDSTRObjectiveKind::ClearPath
			: EDSTRObjectiveKind::EnterChamber;
	}
	if (Input.Phase == EDSTRMatchPhase::Wave && Input.PendingSpawnCount > 0)
	{
		return EDSTRObjectiveKind::Gate;
	}
	return Input.RemainingEnemies > 0 ? EDSTRObjectiveKind::Wave : EDSTRObjectiveKind::None;
}

FString FDSTRHUDViewModel::FormatObjective(const FDSTRObjectiveInput& Input)
{
	switch (GetObjectiveKind(Input))
	{
	case EDSTRObjectiveKind::Revive:
	{
		const FString Who = Input.DownedTeammateName.IsEmpty()
			? FString(TEXT("A TEAMMATE"))
			: Input.DownedTeammateName.Left(MaxPartyNameLength).ToUpper();
		const FString Clock = Input.ReviveSecondsLeft > 0.0f
			? FString::Printf(TEXT(" %dS"), FMath::Max(1, FMath::CeilToInt(Input.ReviveSecondsLeft)))
			: FString();
		return Input.bReviveInReach
			? TEXT("REVIVE ") + Who + FString::Printf(TEXT(" (%s)"), InteractKeyLabel) + Clock
			: TEXT("REACH ") + Who + Clock;
	}
	case EDSTRObjectiveKind::Buff:
		return FString::Printf(TEXT("PICK UP ATTACK BUFF (%s)"), InteractKeyLabel);
	case EDSTRObjectiveKind::Boss:
		return FString::Printf(
			TEXT("SURVIVE: %s %.0f/%.0f"),
			*(Input.BossName.IsEmpty() ? FString(TEXT("BOSS")) : Input.BossName.Left(MaxPartyNameLength).ToUpper()),
			FMath::Max(0.0f, Input.BossHealth),
			FMath::Max(0.0f, Input.BossMaxHealth));
	case EDSTRObjectiveKind::BossIncoming:
		return TEXT("BOSS INCOMING - REGROUP");
	case EDSTRObjectiveKind::ClearPath:
		return FString::Printf(
			TEXT("CLEAR THE PATH TO OPEN THE GATE (%d LEFT)"), FMath::Max(0, Input.AmbushRemaining));
	case EDSTRObjectiveKind::EnterChamber:
		return TEXT("ENTER THE BOSS CHAMBER");
	case EDSTRObjectiveKind::Gate:
		return FString::Printf(TEXT("GO TO THE GATE - %d INCOMING"), Input.PendingSpawnCount);
	case EDSTRObjectiveKind::Wave:
		return FString::Printf(TEXT("CLEAR %d HOSTILES"), Input.RemainingEnemies);
	default:
		return FString();
	}
}

bool FDSTRHUDViewModel::ShouldRetarget(const float CurrentDistance, const float CandidateDistance)
{
	return CandidateDistance < CurrentDistance * ObjectiveRetargetRatio;
}

bool FDSTRHUDViewModel::ShouldPinAtTarget(const float DistanceCentimeters)
{
	return DistanceCentimeters < NearTargetPinRange;
}

FVector2D FDSTRHUDViewModel::ProjectToScreenEdge(
	const FVector2D ViewportSize,
	const FVector2D ScreenPos,
	const bool bBehindCamera,
	const float Inset,
	bool& bOffScreen)
{
	const double MaxX = FMath::Max(static_cast<double>(Inset), ViewportSize.X - Inset);
	const double MaxY = FMath::Max(static_cast<double>(Inset), ViewportSize.Y - Inset);
	if (bBehindCamera)
	{
		bOffScreen = true;
		return FVector2D(FMath::Clamp(ViewportSize.X - ScreenPos.X, static_cast<double>(Inset), MaxX), MaxY);
	}

	bOffScreen = ScreenPos.X < Inset || ScreenPos.X > MaxX || ScreenPos.Y < Inset || ScreenPos.Y > MaxY;
	return FVector2D(
		FMath::Clamp(ScreenPos.X, static_cast<double>(Inset), MaxX),
		FMath::Clamp(ScreenPos.Y, static_cast<double>(Inset), MaxY));
}

float FDSTRHUDViewModel::ScreenEdgeAngle(const FVector2D ViewportSize, const FVector2D EdgePos)
{
	const FVector2D Delta = EdgePos - ViewportSize * 0.5;
	if (Delta.IsNearlyZero())
	{
		return 0.0f;
	}
	return FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(Delta.X, -Delta.Y)));
}

FString FDSTRHUDViewModel::FormatDistanceMeters(const float Centimeters)
{
	return FString::Printf(TEXT("%dm"), FMath::Max(0, FMath::RoundToInt(Centimeters / 100.0f)));
}

FVector2D FDSTRHUDViewModel::ViewportToDesign(
	const FVector2D ViewportSize,
	const FVector2D DesignSize,
	const FVector2D ScreenPos)
{
	if (ViewportSize.X <= 0.0 || ViewportSize.Y <= 0.0 || DesignSize.X <= 0.0 || DesignSize.Y <= 0.0)
	{
		return ScreenPos;
	}
	const double Scale = FMath::Min(ViewportSize.X / DesignSize.X, ViewportSize.Y / DesignSize.Y);
	const FVector2D Bars = (ViewportSize - DesignSize * Scale) * 0.5;
	return (ScreenPos - Bars) / Scale;
}

FString FDSTRHUDViewModel::FormatInteractLabel(const bool bReviveTarget, const bool bPickupTarget)
{
	if (bReviveTarget)
	{
		return TEXT("REVIVE");
	}
	return bPickupTarget ? TEXT("PICK UP") : TEXT("INTERACT");
}
