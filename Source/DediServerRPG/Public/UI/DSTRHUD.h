#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DSTRHUD.generated.h"

UCLASS()
class DEDISERVERRPG_API ADSTRHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

public:
	static constexpr int32 MaxMinimapCaptureAttempts = 10;

private:
	void CaptureMinimapOrRetry();
	bool TryCaptureMinimap();

	UPROPERTY(Transient)
	TObjectPtr<class UDSTRCombatHUDWidget> CombatHUDWidget;
	UPROPERTY(Transient)
	TObjectPtr<class UDSTRLobbyWidget> LobbyWidget;
	UPROPERTY(Transient)
	TObjectPtr<class ADSTRMinimapCapture> MinimapCapture;
	FTimerHandle MinimapCaptureTimerHandle;
	int32 MinimapCaptureAttempts = 0;
};
