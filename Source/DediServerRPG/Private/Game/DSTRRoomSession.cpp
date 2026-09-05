#include "Game/DSTRRoomSession.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/Base64.h"

namespace
{
	FString EncodeRoomName(const FString& Name)
	{
		const FTCHARToUTF8 Utf8(*Name);
		return FBase64::Encode(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}

	bool DecodeRoomName(const FString& Encoded, FString& Name)
	{
		TArray<uint8> Bytes;
		if (Encoded.IsEmpty() || Encoded.Len() > 128 || !FBase64::Decode(Encoded, Bytes) || Bytes.IsEmpty())
		{
			return false;
		}
		const FUTF8ToTCHAR Decoded(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		Name = FString(Decoded.Length(), Decoded.Get());
		return EncodeRoomName(Name) == Encoded;
	}
}

FDSTRRoomRequest FDSTRRoomRequest::FromOptions(const FString& Options)
{
	FDSTRRoomRequest Request;
	Request.bCreate = UGameplayStatics::HasOption(Options, TEXT("DSTRCreateRoom"));
	Request.RoomId = UGameplayStatics::ParseOption(Options, TEXT("DSTRRoomId"));
	if (Request.bCreate)
	{
		const FString EncodedName = UGameplayStatics::ParseOption(Options, TEXT("DSTRRoomName64"));
		Request.bValid = UGameplayStatics::ParseOption(Options, TEXT("DSTRCreateRoom")) == TEXT("1")
			&& DecodeRoomName(EncodedName, Request.RoomName)
			&& FDSTRRoomSession::IsValidRoomName(Request.RoomName)
			&& FDSTRRoomSession::IsValidRoomId(Request.RoomId);
	}
	else
	{
		Request.bValid = Request.RoomId.IsEmpty() || FDSTRRoomSession::IsValidRoomId(Request.RoomId);
	}
	return Request;
}

bool FDSTRRoomSession::IsValidRoomName(const FString& Name)
{
	if (Name.IsEmpty() || Name.Len() > MaxNameLength || Name != Name.TrimStartAndEnd())
	{
		return false;
	}
	for (TCHAR Character : Name)
	{
		if (FChar::IsControl(Character) || Character == TEXT('|'))
		{
			return false;
		}
	}
	return true;
}

bool FDSTRRoomSession::IsValidRoomId(const FString& Id)
{
	FGuid Parsed;
	return Id.Len() == 32 && FGuid::ParseExact(Id, EGuidFormats::Digits, Parsed) && Parsed.IsValid();
}

FString FDSTRRoomSession::BuildCreateOptions(const FString& Name, const FString& Id)
{
	return FString::Printf(TEXT("?DSTRCreateRoom=1?DSTRRoomName64=%s?DSTRRoomId=%s"), *EncodeRoomName(Name), *Id);
}

FString FDSTRRoomSession::BuildJoinOptions(const FString& Id)
{
	return Id.IsEmpty() ? FString() : TEXT("?DSTRRoomId=") + Id;
}

FString FDSTRRoomSession::CheckLogin(const FDSTRRoomRequest& Request) const
{
	if (!bEnabled)
	{
		return Request.bCreate ? TEXT("RoomCreationUnsupported") : FString();
	}
	if (!Request.bValid)
	{
		return TEXT("InvalidRoomRequest");
	}
	if (Request.bCreate)
	{
		return HasRoom() ? TEXT("RoomUnavailable") : FString();
	}
	if (!HasRoom())
	{
		return TEXT("RoomNotFound");
	}
	return Request.RoomId == RoomId ? FString() : TEXT("RoomChanged");
}

bool FDSTRRoomSession::Create(const FDSTRRoomRequest& Request)
{
	if (!bEnabled || !Request.bCreate || !CheckLogin(Request).IsEmpty())
	{
		return false;
	}
	RoomName = Request.RoomName;
	RoomId = Request.RoomId;
	return true;
}
