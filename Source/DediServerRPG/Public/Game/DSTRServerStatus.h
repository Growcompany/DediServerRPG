#pragma once

#include "CoreMinimal.h"
#include "Game/DSTRGameState.h"

enum class EDSTRServerAvailability : uint8
{
	Checking,
	Idle,
	Available,
	Full,
	InProgress,
	NoResponse,
	Incompatible
};

/** Small, public snapshot. A timeout cannot distinguish an offline server from an unreachable one. */
struct DEDISERVERRPG_API FDSTRServerStatus
{
	static constexpr int32 MaxPlayers = 4;
	static constexpr double QueryTimeoutSeconds = 6.0;
	static constexpr double RefreshSeconds = 10.0;

	EDSTRServerAvailability Availability = EDSTRServerAvailability::Checking;
	int32 Players = 0;
	int32 Capacity = MaxPlayers;
	EDSTRMatchPhase Phase = EDSTRMatchPhase::WaitingForPlayers;
	bool bCountdown = false;
	FString RoomName;
	bool bManagedRoom = false;
	FString RoomId;

	bool CanJoin() const { return Availability == EDSTRServerAvailability::Available; }
	bool CanCreateRoom() const { return Availability == EDSTRServerAvailability::Idle; }
	bool HasRoom() const;
	const TCHAR* GetLabel() const;
	const TCHAR* GetHint() const;
	FString Encode() const;
	static bool Parse(const FString& Response, FDSTRServerStatus& OutStatus);
	static FString GetConfiguredAddress();
	static TArray<FString> GetConfiguredAddresses();
	static FString GetConfiguredRoomName();
	static FString CleanRoomName(const FString& Name);
};
