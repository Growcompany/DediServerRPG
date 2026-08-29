#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "DSTRGameInstance.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	FString ConsumeConnectionError();

	FString PendingNickname;

private:
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	FString LastConnectionError;
};
