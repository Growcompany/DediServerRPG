#include "UI/DSTRLobbyViewModel.h"

#include "SocketSubsystem.h"
#include "Sockets.h"
#include "IPAddress.h"

FDSTRServerAddress FDSTRLobbyViewModel::ParseServerAddress(const FString& Input)
{
	FDSTRServerAddress Result;
	const FString Trimmed = Input.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return Result;
	}

	FString Host = Trimmed;
	int32 Port = DefaultPort;
	int32 ColonIndex = INDEX_NONE;
	if (Trimmed.FindLastChar(TEXT(':'), ColonIndex))
	{
		Host = Trimmed.Left(ColonIndex);
		const FString PortText = Trimmed.Mid(ColonIndex + 1);
		if (PortText.IsEmpty() || !PortText.IsNumeric())
		{
			return Result;
		}
		Port = FCString::Atoi(*PortText);
	}

	for (const TCHAR Char : Host)
	{
		if (!FChar::IsAlnum(Char) && Char != TEXT('.') && Char != TEXT('-'))
		{
			return Result;
		}
	}
	if (Host.IsEmpty() || Port < 1 || Port > 65535)
	{
		return Result;
	}

	Result.bValid = true;
	Result.Host = Host;
	Result.Port = Port;
	return Result;
}

FString FDSTRLobbyViewModel::CleanNickname(const FString& Nickname)
{
	FString Clean;
	for (const TCHAR Char : Nickname)
	{
		if (FChar::IsAlnum(Char) || Char == TEXT('_') || Char == TEXT('-'))
		{
			Clean.AppendChar(Char);
		}
	}
	return Clean.Left(MaxNicknameLength);
}

FString FDSTRLobbyViewModel::ResolveNickname(const FString& PendingNickname, const FString& CommandLine, const FString& PlatformName)
{
	const FString Pending = CleanNickname(PendingNickname);
	if (!Pending.IsEmpty())
	{
		return Pending;
	}
	FString FromCommandLine;
	if (FParse::Value(*CommandLine, TEXT("Name="), FromCommandLine))
	{
		int32 OptionEnd = INDEX_NONE;
		if (FromCommandLine.FindChar(TEXT('?'), OptionEnd))
		{
			FromCommandLine.LeftInline(OptionEnd);
		}
		const FString Clean = CleanNickname(FromCommandLine);
		if (!Clean.IsEmpty())
		{
			return Clean;
		}
	}
	return CleanNickname(PlatformName);
}

FString FDSTRLobbyViewModel::BuildJoinUrl(const FDSTRServerAddress& Address, const FString& Nickname)
{
	const FString Clean = CleanNickname(Nickname);
	FString Url = FString::Printf(TEXT("%s:%d"), *Address.Host, Address.Port);
	if (!Clean.IsEmpty())
	{
		Url += TEXT("?Name=") + Clean;
	}
	return Url;
}

FString FDSTRLobbyViewModel::FormatLobbySlot(const int32 Slot, const FString& Name, const bool bHost, const bool bReady)
{
	if (Name.IsEmpty())
	{
		return FString::Printf(TEXT("%d  -- EMPTY SLOT --"), Slot);
	}
	return FString::Printf(TEXT("%d  %s  %s  %s"),
		Slot,
		*Name.Left(MaxNicknameLength).RightPad(MaxNicknameLength),
		bHost ? TEXT("HOST") : TEXT("    "),
		bReady ? TEXT("READY") : TEXT("")).TrimEnd();
}

bool FDSTRLobbyViewModel::CanHostStart(const bool bIsHost, const EDSTRMatchPhase Phase, const int32 PlayerCount, const bool bAssetsLoaded)
{
	return bIsHost && Phase == EDSTRMatchPhase::WaitingForPlayers && PlayerCount >= 1 && bAssetsLoaded;
}

bool FDSTRLobbyViewModel::AreAllReady(const int32 ReadyCount, const int32 PlayerCount)
{
	return PlayerCount >= 1 && ReadyCount >= PlayerCount;
}

const TCHAR* FDSTRLobbyViewModel::GetStartRejectionReason(
	const bool bIsHost, const EDSTRMatchPhase Phase, const int32 PlayerCount, const int32 ReadyCount, const bool bCountdownActive)
{
	if (!bIsHost)
	{
		return TEXT("NotHost");
	}
	if (bCountdownActive)
	{
		return TEXT("Countdown");
	}
	if (Phase != EDSTRMatchPhase::WaitingForPlayers || PlayerCount < 1)
	{
		return TEXT("Phase");
	}
	if (!AreAllReady(ReadyCount, PlayerCount))
	{
		return TEXT("NotReady");
	}
	return nullptr;
}

bool FDSTRLobbyViewModel::ShouldBotStart(
	const bool bIsHost, const EDSTRMatchPhase Phase,
	const float TimeInLobby, const float TimeSinceLastJoin, const bool bAssetsLoaded)
{
	return CanHostStart(bIsHost, Phase, 1, bAssetsLoaded)
		&& TimeInLobby >= BotLobbyMinSeconds
		&& TimeSinceLastJoin >= BotLobbySettleSeconds;
}

FString FDSTRLobbyViewModel::BuildServerLaunchArguments(const FString& ProjectFile, const FString& Map, const int32 Port)
{
	return FString::Printf(TEXT("\"%s\" %s -server -log -port=%d"), *ProjectFile, *Map, Port);
}

bool FDSTRLobbyViewModel::IsPortFree(const int32 Port)
{
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Sockets || Port < 1 || Port > 65535)
	{
		return true;
	}
	FSocket* Probe = Sockets->CreateSocket(NAME_DGram, TEXT("DSTRPortProbe"), FNetworkProtocolTypes::IPv4);
	if (!Probe)
	{
		return true;
	}
	const TSharedRef<FInternetAddr> Addr = Sockets->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
	Addr->SetAnyAddress();
	Addr->SetPort(Port);
	const bool bBound = Probe->Bind(*Addr);
	Probe->Close();
	Sockets->DestroySocket(Probe);
	return bBound;
}

FString FDSTRLobbyViewModel::FormatConnectionError(const FString& EngineReason)
{
	const FString Trimmed = EngineReason.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return FString();
	}
	if (Trimmed.Contains(TEXT("TIMED OUT"), ESearchCase::IgnoreCase))
	{
		return TEXT("CONNECTION TIMED OUT");
	}
	if (Trimmed.Contains(TEXT("ServerFull"), ESearchCase::IgnoreCase))
	{
		return TEXT("SERVER FULL");
	}
	if (Trimmed.Contains(TEXT("HOST CLOSED"), ESearchCase::IgnoreCase) || Trimmed.Contains(TEXT("ConnectionLost"), ESearchCase::IgnoreCase))
	{
		return TEXT("SERVER CLOSED THE CONNECTION");
	}
	int32 CutIndex = INDEX_NONE;
	const FString Head = Trimmed.FindChar(TEXT('.'), CutIndex) && CutIndex > 0 ? Trimmed.Left(CutIndex) : Trimmed;
	return Head.Left(48).ToUpper();
}
