#include "UI/DSTRHUD.h"
#include "DSTRLog.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Presentation/DSTRVisualAssetRegistry.h"
#include "TimerManager.h"
#include "UI/DSTRCombatHUDWidget.h"
#include "UI/DSTRLobbyWidget.h"
#include "UI/DSTRMinimapCapture.h"

void ADSTRHUD::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	CombatHUDWidget = CreateWidget<UDSTRCombatHUDWidget>(PC, UDSTRCombatHUDWidget::StaticClass());
	if (CombatHUDWidget)
	{
		CombatHUDWidget->AddToViewport(10);
	}
	LobbyWidget = CreateWidget<UDSTRLobbyWidget>(PC, UDSTRLobbyWidget::StaticClass());
	if (LobbyWidget)
	{
		LobbyWidget->AddToViewport(20);
	}

	if (!IsRunningDedicatedServer()
		&& FDSTRVisualAssetRegistry::ShouldLoadVisualAssets(GetNetMode())
		&& GetWorld())
	{
		MinimapCapture = GetWorld()->SpawnActor<ADSTRMinimapCapture>();
		FDSTRVisualAssetRegistry::PreloadVisualAssets(
			this,
			[WeakThis = TWeakObjectPtr<ADSTRHUD>(this)]()
		{
			if (ADSTRHUD* HUD = WeakThis.Get())
			{
				HUD->CaptureMinimapOrRetry();
			}
		});
	}
}

void ADSTRHUD::CaptureMinimapOrRetry()
{
	if (TryCaptureMinimap())
	{
		return;
	}
	++MinimapCaptureAttempts;
	if (MinimapCaptureAttempts > MaxMinimapCaptureAttempts)
	{
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_MINIMAP_CAPTURE_GAVE_UP Attempts=%d"), MinimapCaptureAttempts - 1);
		return;
	}
	GetWorldTimerManager().SetTimer(
		MinimapCaptureTimerHandle, this, &ADSTRHUD::CaptureMinimapOrRetry, 1.0f, false);
}

bool ADSTRHUD::TryCaptureMinimap()
{
	if (!MinimapCapture || !CombatHUDWidget || !MinimapCapture->CaptureFloorLayout())
	{
		return false;
	}
	CombatHUDWidget->SetMinimapSource(MinimapCapture->GetRenderTarget(), MinimapCapture->GetFrame());
	return true;
}
