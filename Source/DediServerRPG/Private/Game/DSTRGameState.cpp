#include "Game/DSTRGameState.h"
#include "DSTRLog.h"

#include "Enemy/DSTREnemyCharacter.h"
#include "World/DSTRBossGate.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Player/DSTRPlayerState.h"
#include "UI/DSTRHUDViewModel.h"

ADSTRGameState::ADSTRGameState()
{
	NetUpdateFrequency = 10.0f;
}

bool ADSTRGameState::IsValidPhaseTransition(const EDSTRMatchPhase From, const EDSTRMatchPhase To)
{
	return (From == EDSTRMatchPhase::WaitingForPlayers && To == EDSTRMatchPhase::Wave)
		|| (From == EDSTRMatchPhase::Wave && To == EDSTRMatchPhase::Advance)
		|| (From == EDSTRMatchPhase::Advance && To == EDSTRMatchPhase::Boss)
		|| (From == EDSTRMatchPhase::Wave && To == EDSTRMatchPhase::Boss)
		|| (From == EDSTRMatchPhase::Boss && To == EDSTRMatchPhase::Clear)
		|| ((From == EDSTRMatchPhase::Wave || From == EDSTRMatchPhase::Advance || From == EDSTRMatchPhase::Boss)
			&& To == EDSTRMatchPhase::Failed);
}

void ADSTRGameState::TrimEvents(TArray<FDSTRMatchEvent>& Events, const int32 MaxEvents)
{
	const int32 Excess = Events.Num() - FMath::Max(0, MaxEvents);
	if (Excess > 0)
	{
		Events.RemoveAt(0, Excess);
	}
}

void ADSTRGameState::StartMatchFlow()
{
	if (!HasAuthority())
	{
		return;
	}
	MatchStartServerTime = GetServerWorldTimeSeconds();
}

bool ADSTRGameState::AdvancePhase(const EDSTRMatchPhase NewPhase)
{
	if (!HasAuthority() || !IsValidPhaseTransition(MatchPhase, NewPhase))
	{
		return false;
	}
	MatchPhase = NewPhase;
	UE_LOG(LogDSTR, Log, TEXT("DSTR_PHASE Phase=%s"), *GetPhaseDisplayName());
	ForceNetUpdate();
	return true;
}

void ADSTRGameState::SetRemainingEnemies(const int32 NewRemainingEnemies)
{
	if (HasAuthority())
	{
		RemainingEnemies = FMath::Max(0, NewRemainingEnemies);
		ForceNetUpdate();
	}
}

void ADSTRGameState::SetWaveSpawnGate(const FVector& Gate)
{
	if (HasAuthority())
	{
		WaveSpawnGate = Gate;
		ForceNetUpdate();
	}
}

void ADSTRGameState::SetPendingSpawnCount(const int32 Count)
{
	if (HasAuthority())
	{
		PendingSpawnCount = FMath::Max(0, Count);
		ForceNetUpdate();
	}
}

void ADSTRGameState::SetBossRoom(const FVector& Room)
{
	if (HasAuthority())
	{
		BossRoomLocation = Room;
		ForceNetUpdate();
	}
}

void ADSTRGameState::SetAdvancePath(const TArray<FVector>& Path)
{
	if (!HasAuthority())
	{
		return;
	}
	AdvancePathPoints.Reset(Path.Num());
	for (const FVector& Point : Path)
	{
		AdvancePathPoints.Add(Point);
	}
	OnRep_AdvancePath();
	ForceNetUpdate();
}

void ADSTRGameState::OnRep_AdvancePath()
{
	AdvancePath.Reset(AdvancePathPoints.Num());
	for (const FVector_NetQuantize& Point : AdvancePathPoints)
	{
		AdvancePath.Add(Point);
	}
}

void ADSTRGameState::SetBossGate(const FVector& Gate)
{
	if (HasAuthority())
	{
		BossGateLocation = Gate;
		ForceNetUpdate();
	}
}

void ADSTRGameState::SetBossGateActor(ADSTRBossGate* Gate)
{
	if (HasAuthority())
	{
		BossGateActor = Gate;
		ForceNetUpdate();
	}
}

void ADSTRGameState::SetAmbushRemaining(const int32 Count)
{
	if (HasAuthority())
	{
		AmbushRemaining = FMath::Max(0, Count);
		ForceNetUpdate();
	}
}

void ADSTRGameState::SetBoss(ADSTREnemyCharacter* NewBoss)
{
	if (HasAuthority())
	{
		Boss = NewBoss;
		if (NewBoss && BossSpawnServerTime <= 0.0f)
		{
			BossSpawnServerTime = GetServerWorldTimeSeconds();
		}
		ForceNetUpdate();
	}
}

void ADSTRGameState::MarkBossIncoming()
{
	if (HasAuthority())
	{
		BossSpawnServerTime = GetServerWorldTimeSeconds();
		ForceNetUpdate();
	}
}

void ADSTRGameState::SetStartCountdown(const float Seconds)
{
	if (HasAuthority())
	{
		CountdownEndServerTime = GetServerWorldTimeSeconds() + FMath::Max(0.0f, Seconds);
		ForceNetUpdate();
	}
}

void ADSTRGameState::ClearStartCountdown()
{
	if (HasAuthority())
	{
		CountdownEndServerTime = 0.0f;
		ForceNetUpdate();
	}
}

void ADSTRGameState::RecordDowned(const FString& PlayerName)
{
	if (HasAuthority())
	{
		++DownCount;
		PushEvent(EDSTRMatchEventKind::PlayerDowned, PlayerName);
	}
}

void ADSTRGameState::RecordRevived(const FString& ReviverName, const FString& TargetName)
{
	if (HasAuthority())
	{
		++ReviveCount;
		PushEvent(EDSTRMatchEventKind::PlayerRevived, ReviverName, TargetName);
	}
}

void ADSTRGameState::PushEvent(const EDSTRMatchEventKind Kind, const FString& Subject, const FString& Object)
{
	if (!HasAuthority())
	{
		return;
	}
	FDSTRMatchEvent Event;
	Event.Kind = Kind;
	Event.Subject = Subject;
	Event.Object = Object;
	Event.ServerTime = GetServerWorldTimeSeconds();
	RecentEvents.Add(Event);
	TrimEvents(RecentEvents);
	ForceNetUpdate();
}

float ADSTRGameState::GetElapsedMatchSeconds() const
{
	return MatchStartServerTime > 0.0f
		? FMath::Max(0.0f, GetServerWorldTimeSeconds() - MatchStartServerTime)
		: 0.0f;
}

float ADSTRGameState::GetSecondsSinceBossSpawn() const
{
	return BossSpawnServerTime > 0.0f
		? FMath::Max(0.0f, GetServerWorldTimeSeconds() - BossSpawnServerTime)
		: TNumericLimits<float>::Max();
}

float ADSTRGameState::GetCountdownRemaining() const
{
	return IsCountdownActive()
		? FMath::Max(0.0f, CountdownEndServerTime - GetServerWorldTimeSeconds())
		: 0.0f;
}

FString ADSTRGameState::GetPhaseDisplayName() const
{
	return FDSTRHUDViewModel::FormatPhase(MatchPhase, IsCountdownActive(), GetCountdownRemaining());
}

void ADSTRGameState::OnRep_MatchPhase()
{
	UE_LOG(LogDSTR, Log, TEXT("DSTR_CLIENT_PHASE Phase=%s"), *GetPhaseDisplayName());
}

bool ADSTRGameState::IsHost(const APlayerState* PlayerState) const
{
	return PlayerState && HostPlayerId >= 0 && PlayerState->GetPlayerId() == HostPlayerId;
}

int32 ADSTRGameState::CountPresentationReadyPlayers() const
{
	int32 ReadyCount = 0;
	for (const APlayerState* Member : PlayerArray)
	{
		const ADSTRPlayerState* State = Cast<ADSTRPlayerState>(Member);
		if (State && State->IsPresentationReady())
		{
			++ReadyCount;
		}
	}
	return ReadyCount;
}

void ADSTRGameState::SetHostPlayerId(const int32 NewHostPlayerId)
{
	if (HasAuthority())
	{
		HostPlayerId = NewHostPlayerId;
		UE_LOG(LogDSTR, Log, TEXT("DSTR_LOBBY_HOST PlayerId=%d"), HostPlayerId);
		ForceNetUpdate();
	}
}

void ADSTRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADSTRGameState, MatchPhase);
	DOREPLIFETIME(ADSTRGameState, RemainingEnemies);
	DOREPLIFETIME(ADSTRGameState, WaveSpawnGate);
	DOREPLIFETIME(ADSTRGameState, PendingSpawnCount);
	DOREPLIFETIME(ADSTRGameState, BossRoomLocation);
	DOREPLIFETIME(ADSTRGameState, AdvancePathPoints);
	DOREPLIFETIME(ADSTRGameState, BossGateLocation);
	DOREPLIFETIME(ADSTRGameState, BossGateActor);
	DOREPLIFETIME(ADSTRGameState, AmbushRemaining);
	DOREPLIFETIME(ADSTRGameState, Boss);
	DOREPLIFETIME(ADSTRGameState, MatchStartServerTime);
	DOREPLIFETIME(ADSTRGameState, BossSpawnServerTime);
	DOREPLIFETIME(ADSTRGameState, CountdownEndServerTime);
	DOREPLIFETIME(ADSTRGameState, DownCount);
	DOREPLIFETIME(ADSTRGameState, ReviveCount);
	DOREPLIFETIME(ADSTRGameState, RecentEvents);
	DOREPLIFETIME(ADSTRGameState, HostPlayerId);
}
