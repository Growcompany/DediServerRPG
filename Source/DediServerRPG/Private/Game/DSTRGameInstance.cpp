#include "Game/DSTRGameInstance.h"
#include "DSTRLog.h"

#include "Engine/Engine.h"

void UDSTRGameInstance::Init()
{
	Super::Init();
	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UDSTRGameInstance::HandleNetworkFailure);
		GEngine->OnTravelFailure().AddUObject(this, &UDSTRGameInstance::HandleTravelFailure);
	}
}

FString UDSTRGameInstance::ConsumeConnectionError()
{
	FString Error = LastConnectionError;
	LastConnectionError.Empty();
	return Error;
}

void UDSTRGameInstance::HandleNetworkFailure(UWorld*, UNetDriver*, const ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	LastConnectionError = ErrorString.IsEmpty() ? ENetworkFailure::ToString(FailureType) : ErrorString;
	UE_LOG(LogDSTR, Warning, TEXT("DSTR_CONNECTION_FAILED Reason=%s"), *LastConnectionError);
}

void UDSTRGameInstance::HandleTravelFailure(UWorld*, const ETravelFailure::Type FailureType, const FString& ErrorString)
{
	LastConnectionError = ErrorString.IsEmpty() ? ETravelFailure::ToString(FailureType) : ErrorString;
	UE_LOG(LogDSTR, Warning, TEXT("DSTR_CONNECTION_FAILED Reason=%s"), *LastConnectionError);
}
