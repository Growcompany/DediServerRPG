#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DSTRPlayerController.generated.h"

class UUserWidget;

UCLASS()
class DEDISERVERRPG_API ADSTRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestStartMatch();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ReportPresentationReady();

	void ReportPresentationReadyIfLoaded();

	virtual void OnRep_PlayerState() override;

	void ReturnToMainMenu();
	void SetMenuInputMode(UUserWidget* FocusWidget);
	void SetLobbyInputMode();
	void SetCombatInputMode();

protected:
	virtual void BeginPlay() override;

private:
	void ReportReadyWatchdog();

	FTimerHandle PresentationReadyTimer;
	bool bPresentationReadySent = false;
};
