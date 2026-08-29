#include "Game/DSTRMenuGameMode.h"
#include "DSTRLog.h"

#include "GameFramework/HUD.h"
#include "GameFramework/SpectatorPawn.h"
#include "Player/DSTRPlayerController.h"
#include "Presentation/DSTRVisualAssetRegistry.h"

ADSTRMenuGameMode::ADSTRMenuGameMode()
{
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	HUDClass = AHUD::StaticClass();
	PlayerControllerClass = ADSTRPlayerController::StaticClass();
}

void ADSTRMenuGameMode::BeginPlay()
{
	Super::BeginPlay();
	FDSTRVisualAssetRegistry::PreloadVisualAssets(this, []()
	{
		UE_LOG(LogDSTR, Log, TEXT("DSTR_MENU_PRELOAD_COMPLETE"));
	});
}
