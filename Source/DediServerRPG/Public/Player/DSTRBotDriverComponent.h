#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DSTRBotDriverComponent.generated.h"

class ADediServerRPGCharacter;

UCLASS()
class DEDISERVERRPG_API UDSTRBotDriverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDSTRBotDriverComponent();

	static bool ShouldAttach(const FString& CommandLine, bool bLocallyControlled);

	static constexpr float AimDegreesPerStep = 6.0f;
	static constexpr float AimCommitDegrees = 20.0f;

protected:
	virtual void BeginPlay() override;

private:
	void RunStep();
	bool TryLobbyStart(ADediServerRPGCharacter& Character);
	float AimAt(ADediServerRPGCharacter& Character, const FVector& ToTarget, bool bImmediate);

	FTimerHandle StepTimerHandle;
	bool bAnnounced = false;
	double LobbyEnterTime = -1.0;
	double LastPlayerCountChangeTime = -1.0;
	int32 LastKnownPlayerCount = -1;
	double LastBotStartRequestTime = -1.0;
	static constexpr float StepSeconds = 0.15f;
	static constexpr float BotStartRetrySeconds = 2.0f;
	static constexpr float BotFortifyHealthRatio = 0.5f;
	static constexpr float TelegraphEscapeRange = 550.0f;
};
