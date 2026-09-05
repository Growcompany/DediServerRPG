#pragma once

#include "CoreMinimal.h"
#include "Engine/PendingNetGame.h"
#include "Game/DSTRServerStatus.h"
#include "DSTRServerStatusQuery.generated.h"

/** An isolated pre-login handshake on the game port. Never registered as a pending world travel. */
UCLASS(Transient)
class DEDISERVERRPG_API UDSTRServerStatusQuery : public UPendingNetGame
{
	GENERATED_BODY()

public:
	void Start(UWorld* World, const FString& Address);
	void Cancel();
	bool IsFinished() const { return bFinished; }
	const FDSTRServerStatus& GetResult() const { return Result; }
	static bool IsStatusDriver(const UNetDriver* Driver);

	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch) override;
	virtual void SendJoin() override {} // A status query must never allocate a player.
	virtual void BeginDestroy() override;

private:
	void ReleaseDriver();
	void Finish(EDSTRServerAvailability Availability);
	void HandleQueryNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type FailureType, const FString& Error);
	TWeakObjectPtr<UWorld> QueryWorld;
	FName DriverName;
	FDelegateHandle NetworkFailureHandle;
	FDSTRServerStatus Result;
	double StartedAt = 0.0;
	bool bFinished = true;
};
