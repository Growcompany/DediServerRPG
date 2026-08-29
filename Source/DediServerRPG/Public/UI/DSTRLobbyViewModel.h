#pragma once

#include "CoreMinimal.h"
#include "Game/DSTRGameState.h"

struct FDSTRServerAddress
{
	bool bValid = false;
	FString Host;
	int32 Port = 0;
};

struct DEDISERVERRPG_API FDSTRLobbyViewModel
{
	static constexpr int32 DefaultPort = 7777;
	static constexpr int32 MaxNicknameLength = 12;
	static constexpr int32 MaxLobbySlotLength = 32;
	static constexpr float BotLobbySettleSeconds = 2.0f;
	static constexpr float BotLobbyMinSeconds = 10.0f;
	static constexpr float ReadyPendingWatchdogSeconds = 10.0f;

	static const TCHAR* MainMenuMapPath() { return TEXT("/Game/DediServerRPG/Maps/DSTR_MainMenu"); }
	static const TCHAR* DungeonMapPath() { return TEXT("/Game/DediServerRPG/Maps/DSTR_DungeonArena"); }

	static FDSTRServerAddress ParseServerAddress(const FString& Input);
	static FString CleanNickname(const FString& Nickname);
	static FString ResolveNickname(const FString& PendingNickname, const FString& CommandLine, const FString& PlatformName);
	static FString BuildJoinUrl(const FDSTRServerAddress& Address, const FString& Nickname);
	static FString FormatLobbySlot(int32 Slot, const FString& Name, bool bHost, bool bReady);
	static bool CanHostStart(bool bIsHost, EDSTRMatchPhase Phase, int32 PlayerCount, bool bAssetsLoaded);
	static bool AreAllReady(int32 ReadyCount, int32 PlayerCount);
	static const TCHAR* GetStartRejectionReason(
		bool bIsHost, EDSTRMatchPhase Phase, int32 PlayerCount, int32 ReadyCount, bool bCountdownActive);
	static bool ShouldBotStart(bool bIsHost, EDSTRMatchPhase Phase, float TimeInLobby, float TimeSinceLastJoin, bool bAssetsLoaded);
	static FString BuildServerLaunchArguments(const FString& ProjectFile, const FString& Map, int32 Port);
	static bool IsPortFree(int32 Port);
	static FString FormatConnectionError(const FString& EngineReason);
};
