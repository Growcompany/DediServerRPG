#include "Player/DSTRPlayerController.h"
#include "DSTRLog.h"

#include "Blueprint/UserWidget.h"
#include "DediServerRPG/DediServerRPGGameMode.h"
#include "Engine/World.h"
#include "Game/DSTRMenuGameMode.h"
#include "Player/DSTRPlayerState.h"
#include "Presentation/DSTRVisualAssetRegistry.h"
#include "TimerManager.h"
#include "UI/DSTRLobbyViewModel.h"
#include "UI/DSTRMainMenuWidget.h"

void ADSTRPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController() || !GetWorld())
	{
		return;
	}
	if (GetWorld()->GetAuthGameMode<ADSTRMenuGameMode>())
	{
		if (UDSTRMainMenuWidget* Menu = CreateWidget<UDSTRMainMenuWidget>(this, UDSTRMainMenuWidget::StaticClass()))
		{
			Menu->AddToViewport(10);
			SetMenuInputMode(Menu);
		}
		return;
	}
	FDSTRVisualAssetRegistry::PreloadVisualAssets(
		this,
		[WeakThis = TWeakObjectPtr<ADSTRPlayerController>(this)]()
	{
		if (ADSTRPlayerController* Controller = WeakThis.Get())
		{
			Controller->ReportPresentationReadyIfLoaded();
		}
	});
	ReportPresentationReadyIfLoaded();
	GetWorldTimerManager().SetTimer(
		PresentationReadyTimer, this, &ADSTRPlayerController::ReportReadyWatchdog,
		FDSTRLobbyViewModel::ReadyPendingWatchdogSeconds, false);
}

void ADSTRPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ReportPresentationReadyIfLoaded();
}

void ADSTRPlayerController::ReportPresentationReadyIfLoaded()
{
	if (bPresentationReadySent || !IsLocalController())
	{
		return;
	}
	if (!PlayerState || !FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(this))
	{
		return;
	}
	bPresentationReadySent = true;
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(PresentationReadyTimer);
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_LOBBY_READY_SENT PlayerId=%d"), PlayerState->GetPlayerId());
	Server_ReportPresentationReady();
}

void ADSTRPlayerController::ReportReadyWatchdog()
{
	if (bPresentationReadySent)
	{
		return;
	}
	UE_LOG(LogDSTR, Warning, TEXT("DSTR_LOBBY_READY_PENDING Waited=%.0f HasPlayerState=%d Loaded=%d"),
		FDSTRLobbyViewModel::ReadyPendingWatchdogSeconds,
		PlayerState ? 1 : 0,
		FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(this) ? 1 : 0);
}

bool ADSTRPlayerController::Server_ReportPresentationReady_Validate()
{
	return true;
}

void ADSTRPlayerController::Server_ReportPresentationReady_Implementation()
{
	ADSTRPlayerState* State = GetPlayerState<ADSTRPlayerState>();
	if (!State || State->IsPresentationReady())
	{
		return;
	}
	State->SetPresentationReady(true);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_LOBBY_READY PlayerId=%d"), State->GetPlayerId());
}

bool ADSTRPlayerController::Server_RequestStartMatch_Validate()
{
	return true;
}

void ADSTRPlayerController::Server_RequestStartMatch_Implementation()
{
	if (ADediServerRPGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADediServerRPGGameMode>() : nullptr)
	{
		GameMode->TryStartMatchByHost(this);
	}
}

void ADSTRPlayerController::ReturnToMainMenu()
{
	ClientTravel(FDSTRLobbyViewModel::MainMenuMapPath(), ETravelType::TRAVEL_Absolute);
}

void ADSTRPlayerController::SetMenuInputMode(UUserWidget* FocusWidget)
{
	FInputModeUIOnly Mode;
	if (FocusWidget)
	{
		FocusWidget->SetIsFocusable(true);
		Mode.SetWidgetToFocus(FocusWidget->TakeWidget());
	}
	SetInputMode(Mode);
	SetShowMouseCursor(true);
}

void ADSTRPlayerController::SetLobbyInputMode()
{
	SetInputMode(FInputModeGameAndUI());
	SetShowMouseCursor(true);
}

void ADSTRPlayerController::SetCombatInputMode()
{
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
}
