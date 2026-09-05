#include "Game/DSTRServerStatus.h"
#include "Game/DSTRRoomSession.h"
#include "UI/DSTRLobbyViewModel.h"

#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

namespace
{
	bool ParseSmallNumber(const FString& Text, int32 Max, int32& Out)
	{
		if (Text.IsEmpty() || Text.Len() > 2)
		{
			return false;
		}
		for (TCHAR Character : Text)
		{
			if (Character < TEXT('0') || Character > TEXT('9'))
			{
				return false;
			}
		}
		Out = FCString::Atoi(*Text);
		return Out <= Max;
	}
}

const TCHAR* FDSTRServerStatus::GetLabel() const
{
	switch (Availability)
	{
	case EDSTRServerAvailability::Idle: return TEXT("ONLINE - ROOM AVAILABLE");
	case EDSTRServerAvailability::Available: return TEXT("ONLINE - JOINABLE");
	case EDSTRServerAvailability::Full: return TEXT("ONLINE - FULL");
	case EDSTRServerAvailability::InProgress: return TEXT("ONLINE - IN PROGRESS");
	case EDSTRServerAvailability::NoResponse: return TEXT("NO RESPONSE");
	case EDSTRServerAvailability::Incompatible: return TEXT("UPDATE REQUIRED");
	default: return TEXT("CHECKING SERVER...");
	}
}

const TCHAR* FDSTRServerStatus::GetHint() const
{
	switch (Availability)
	{
	case EDSTRServerAvailability::Idle: return TEXT("Create a named room to start a new party.");
	case EDSTRServerAvailability::Available: return TEXT("Join the lobby. The party leader starts the dungeon.");
	case EDSTRServerAvailability::Full: return TEXT("All four slots are occupied. Try again after a player leaves.");
	case EDSTRServerAvailability::InProgress: return TEXT("This party is playing. A new lobby opens after everyone leaves.");
	case EDSTRServerAvailability::NoResponse: return TEXT("The server may be offline or unreachable. Refresh to retry.");
	case EDSTRServerAvailability::Incompatible: return TEXT("The server and client need matching game versions.");
	default: return TEXT("Requesting the current lobby status...");
	}
}

FString FDSTRServerStatus::CleanRoomName(const FString& Name)
{
	FString Clean;
	for (TCHAR Character : Name)
	{
		if (!FChar::IsControl(Character) && Character != TEXT('|'))
		{
			Clean.AppendChar(Character);
		}
	}
	Clean = Clean.TrimStartAndEnd().Left(32);
	return Clean.IsEmpty() ? TEXT("PUBLIC DUNGEON") : Clean;
}

FString FDSTRServerStatus::Encode() const
{
	if (bManagedRoom)
	{
		return FString::Printf(TEXT("DSTR_STATUS_V2|%d|%d|%d|%d|%s|%s"),
			Players, Capacity, static_cast<int32>(Phase), bCountdown ? 1 : 0, *RoomName, *RoomId);
	}
	return FString::Printf(TEXT("DSTR_STATUS_V1|%d|%d|%d|%d|%s"),
		Players, Capacity, static_cast<int32>(Phase), bCountdown ? 1 : 0, *CleanRoomName(RoomName));
}

bool FDSTRServerStatus::Parse(const FString& Response, FDSTRServerStatus& OutStatus)
{
	// Never leave a previous successful snapshot usable after a malformed reply.
	OutStatus = FDSTRServerStatus();
	OutStatus.Availability = EDSTRServerAvailability::Incompatible;
	if (Response.Len() > 256)
	{
		return false;
	}
	TArray<FString> Fields;
	Response.ParseIntoArray(Fields, TEXT("|"), false);
	const bool bManaged = Fields.Num() == 7 && Fields[0] == TEXT("DSTR_STATUS_V2");
	const bool bLegacy = Fields.Num() == 6 && Fields[0] == TEXT("DSTR_STATUS_V1");
	int32 Players = 0, Capacity = 0, Phase = 0, Countdown = 0;
	if ((!bManaged && !bLegacy)
		|| !ParseSmallNumber(Fields[1], MaxPlayers, Players)
		|| !ParseSmallNumber(Fields[2], MaxPlayers, Capacity) || Capacity < 1 || Players > Capacity
		|| !ParseSmallNumber(Fields[3], static_cast<int32>(EDSTRMatchPhase::Failed), Phase)
		|| !ParseSmallNumber(Fields[4], 1, Countdown))
	{
		return false;
	}
	const bool bIdle = bManaged && Fields[6].IsEmpty();
	if (bIdle ? (!Fields[5].IsEmpty() || Players != 0 || Phase != 0 || Countdown != 0)
		: (Fields[5].IsEmpty() || Fields[5] != CleanRoomName(Fields[5])))
	{
		return false;
	}
	if (bManaged && !bIdle && (!FDSTRRoomSession::IsValidRoomId(Fields[6])
		|| !FDSTRRoomSession::IsValidRoomName(Fields[5])))
	{
		return false;
	}
	OutStatus.Players = Players;
	OutStatus.Capacity = Capacity;
	OutStatus.Phase = static_cast<EDSTRMatchPhase>(Phase);
	OutStatus.bCountdown = Countdown != 0;
	OutStatus.RoomName = Fields[5];
	OutStatus.bManagedRoom = bManaged;
	OutStatus.RoomId = bManaged ? Fields[6] : FString();
	OutStatus.Availability = bIdle ? EDSTRServerAvailability::Idle
		: (OutStatus.Phase != EDSTRMatchPhase::WaitingForPlayers || OutStatus.bCountdown)
		? EDSTRServerAvailability::InProgress
		: (Players >= Capacity ? EDSTRServerAvailability::Full : EDSTRServerAvailability::Available);
	return true;
}

bool FDSTRServerStatus::HasRoom() const
{
	return Availability == EDSTRServerAvailability::Available || Availability == EDSTRServerAvailability::Full
		|| Availability == EDSTRServerAvailability::InProgress;
}

TArray<FString> FDSTRServerStatus::GetConfiguredAddresses()
{
	TArray<FString> Addresses;
	GConfig->GetArray(TEXT("DSTR.Online"), TEXT("RoomServer"), Addresses, GGameIni);
	FString Override;
	if (FParse::Value(FCommandLine::Get(), TEXT("DSTRServers="), Override, false))
	{
		Addresses.Empty();
		Override.ParseIntoArray(Addresses, TEXT(","), true);
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("DSTRServer="), Override))
	{
		Addresses = {Override};
	}
	if (Addresses.IsEmpty())
	{
		Addresses.Add(GetConfiguredAddress());
	}
	TArray<FString> Valid;
	for (const FString& Address : Addresses)
	{
		const FString Trimmed = Address.TrimStartAndEnd();
		if (FDSTRLobbyViewModel::ParseServerAddress(Trimmed).bValid && Valid.Num() < FDSTRRoomSession::MaxRooms)
		{
			Valid.AddUnique(Trimmed);
		}
	}
	return Valid;
}

FString FDSTRServerStatus::GetConfiguredAddress()
{
	FString Address;
	GConfig->GetString(TEXT("DSTR.Online"), TEXT("ServerAddress"), Address, GGameIni);
	// Useful for a local test server; normal players never need an address field.
	FParse::Value(FCommandLine::Get(), TEXT("DSTRServer="), Address);
	return Address.TrimStartAndEnd();
}

FString FDSTRServerStatus::GetConfiguredRoomName()
{
	FString Name;
	GConfig->GetString(TEXT("DSTR.Online"), TEXT("RoomName"), Name, GGameIni);
	return CleanRoomName(Name);
}
