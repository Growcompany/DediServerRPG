#include "Game/DSTRServerStatusQuery.h"

#include "DSTRLog.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Net/DataChannel.h"
#include "UI/DSTRLobbyViewModel.h"

namespace
{
	const TCHAR* StatusDriverPrefix = TEXT("DSTRStatusNetDriver_");
}

bool UDSTRServerStatusQuery::IsStatusDriver(const UNetDriver* Driver)
{
	return Driver && Driver->NetDriverName.ToString().StartsWith(StatusDriverPrefix);
}

void UDSTRServerStatusQuery::Start(UWorld* World, const FString& Address)
{
	Cancel();
	ConnectionError.Reset();
	Result = FDSTRServerStatus();
	bFinished = false;
	StartedAt = FPlatformTime::Seconds();
	QueryWorld = World;
	if (!World || !GEngine || !FDSTRLobbyViewModel::ParseServerAddress(Address).bValid)
	{
		Finish(EDSTRServerAvailability::NoResponse);
		return;
	}

	Initialize(FURL(nullptr, *Address, TRAVEL_Absolute));
	DriverName = FName(*FString::Printf(TEXT("%s%u"), StatusDriverPrefix, GetUniqueID()));
	if (!GEngine->CreateNamedNetDriver(World, DriverName, NAME_GameNetDriver))
	{
		Finish(EDSTRServerAvailability::NoResponse);
		return;
	}
	NetDriver = GEngine->FindNamedNetDriver(World, DriverName);
	NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UDSTRServerStatusQuery::HandleQueryNetworkFailure);
	if (!NetDriver || !NetDriver->InitConnect(this, URL, ConnectionError) || !NetDriver->ServerConnection)
	{
		Finish(EDSTRServerAvailability::NoResponse);
		ReleaseDriver();
		return;
	}
	// Keep the driver worldless, like PendingNetGame, and tick it explicitly.
	// SetWorld would replace our notify handler and attach it to normal gameplay ticks.
	BeginHandshake();
}

void UDSTRServerStatusQuery::Tick(float DeltaSeconds)
{
	if (bFinished)
	{
		ReleaseDriver();
		return;
	}
	if (NetDriver && NetDriver->ServerConnection)
	{
		Super::Tick(DeltaSeconds);
	}
	if (!bFinished && (!ConnectionError.IsEmpty() || !NetDriver
		|| FPlatformTime::Seconds() - StartedAt >= FDSTRServerStatus::QueryTimeoutSeconds))
	{
		Finish(EDSTRServerAvailability::NoResponse);
	}
	if (bFinished)
	{
		// Control messages arrive inside TickDispatch; destroy only after it returns.
		ReleaseDriver();
	}
}

void UDSTRServerStatusQuery::NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch)
{
	if (bFinished || !NetDriver || Connection != NetDriver->ServerConnection)
	{
		return;
	}
	switch (MessageType)
	{
	case NMT_Challenge:
		if (FNetControlMessage<NMT_Challenge>::Receive(Bunch, Connection->Challenge))
		{
			Connection->ClientResponse = TEXT("0");
			FString Request = TEXT("?DSTRStatus=1");
			FString Platform = TEXT("None");
			FNetControlMessage<NMT_Login>::Send(Connection, Connection->ClientResponse, Request, Connection->PlayerId, Platform);
			Connection->FlushNet();
		}
		break;
	case NMT_Failure:
		{
			FString Response;
			if (FNetControlMessage<NMT_Failure>::Receive(Bunch, Response))
			{
				FDSTRServerStatus::Parse(Response, Result);
				Finish(Result.Availability);
			}
		}
		break;
	case NMT_Upgrade:
		{
			uint32 Version = 0;
			uint16 Features = 0;
			if (FNetControlMessage<NMT_Upgrade>::Receive(Bunch, Version, Features))
			{
				Finish(EDSTRServerAvailability::Incompatible);
			}
		}
		break;
	case NMT_Welcome:
		{
			FString Map, Game, Redirect;
			if (FNetControlMessage<NMT_Welcome>::Receive(Bunch, Map, Game, Redirect))
			{
				// An older server accepted Login without understanding our status option.
				// Do not load its map or send Join.
				Finish(EDSTRServerAvailability::Incompatible);
			}
		}
		break;
	default:
		break; // The control channel discards unhandled payloads, including CloseReason.
	}
}

void UDSTRServerStatusQuery::Finish(EDSTRServerAvailability Availability)
{
	Result.Availability = Availability;
	bFinished = true;
	UE_LOG(LogDSTR, Log, TEXT("DSTR_SERVER_STATUS State=%s Players=%d Capacity=%d Room=%s Id=%s"),
		Result.GetLabel(), Result.Players, Result.Capacity, *Result.RoomName, *Result.RoomId);
}

void UDSTRServerStatusQuery::HandleQueryNetworkFailure(UWorld*, UNetDriver* Driver,
	ENetworkFailure::Type FailureType, const FString&)
{
	if (!bFinished && Driver == NetDriver)
	{
		// The stateless handshake can reject an outdated client before NMT_Upgrade.
		Finish(FailureType == ENetworkFailure::OutdatedClient || FailureType == ENetworkFailure::OutdatedServer
			? EDSTRServerAvailability::Incompatible : EDSTRServerAvailability::NoResponse);
	}
}

void UDSTRServerStatusQuery::ReleaseDriver()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}
	NetworkFailureHandle.Reset();
	if (!NetDriver)
	{
		return;
	}
	if (GEngine && QueryWorld.IsValid() && GEngine->GetWorldContextFromWorld(QueryWorld.Get())
		&& GEngine->FindNamedNetDriver(QueryWorld.Get(), DriverName) == NetDriver)
	{
		GEngine->DestroyNamedNetDriver(QueryWorld.Get(), DriverName);
	}
	else
	{
		NetDriver->Shutdown();
		NetDriver->LowLevelDestroy();
	}
	NetDriver = nullptr;
}

void UDSTRServerStatusQuery::Cancel()
{
	bFinished = true;
	ReleaseDriver();
	QueryWorld.Reset();
}

void UDSTRServerStatusQuery::BeginDestroy()
{
	Cancel();
	Super::BeginDestroy();
}
