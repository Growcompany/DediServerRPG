#include "Player/DSTRLocalPlayer.h"

#include "Game/DSTRGameInstance.h"
#include "Misc/CommandLine.h"
#include "UI/DSTRLobbyViewModel.h"

FString UDSTRLocalPlayer::GetNickname() const
{
	const UDSTRGameInstance* Instance = GetGameInstance() ? Cast<UDSTRGameInstance>(GetGameInstance()) : nullptr;
	return FDSTRLobbyViewModel::ResolveNickname(
		Instance ? Instance->PendingNickname : FString(),
		FCommandLine::Get(),
		Super::GetNickname());
}
