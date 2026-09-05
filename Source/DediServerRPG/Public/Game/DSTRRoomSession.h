#pragma once

#include "CoreMinimal.h"

/** A generation identifies one player-created room, even when its server port is reused. */
struct DEDISERVERRPG_API FDSTRRoomRequest
{
	bool bCreate = false;
	bool bValid = true;
	FString RoomName;
	FString RoomId;
	static FDSTRRoomRequest FromOptions(const FString& Options);
};

struct DEDISERVERRPG_API FDSTRRoomSession
{
	static constexpr int32 MaxRooms = 3;
	static constexpr int32 MaxNameLength = 24;

	bool bEnabled = false;
	FString RoomName;
	FString RoomId;

	bool HasRoom() const { return !RoomId.IsEmpty(); }
	FString CheckLogin(const FDSTRRoomRequest& Request) const;
	bool Create(const FDSTRRoomRequest& Request);
	static bool IsValidRoomName(const FString& Name);
	static bool IsValidRoomId(const FString& Id);
	static FString BuildCreateOptions(const FString& Name, const FString& Id);
	static FString BuildJoinOptions(const FString& Id);
};
