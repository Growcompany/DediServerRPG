#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "DediServerRPG/DediServerRPGGameMode.h"
#include "Engine/Engine.h"
#include "Engine/Player.h"
#include "Engine/World.h"
#include "Game/DSTRGameState.h"
#include "GameFramework/PlayerController.h"
#include "Player/DSTRPlayerState.h"
#include "TimerManager.h"

namespace
{
	struct FServerLifecycleWorld
	{
		UWorld* World;
		ADediServerRPGGameMode* Mode;

		FServerLifecycleWorld()
		{
			World = UWorld::CreateWorld(EWorldType::PIE, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::PIE);
			Context.SetCurrentWorld(World);
			World->SetPlayInEditorInitialNetMode(NM_DedicatedServer);
			World->URL.Map = TEXT("/Game/DediServerRPG/Maps/DSTR_DungeonArena");
			Context.LastURL = World->URL;
			Mode = World->SpawnActor<ADediServerRPGGameMode>();
			Mode->PreInitializeComponents();
		}

		~FServerLifecycleWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		APlayerController* Join()
		{
			APlayerController* PC = World->SpawnActor<APlayerController>();
			World->AddController(PC);
			PC->Player = NewObject<UPlayer>(World);
			PC->PlayerState = World->SpawnActor<ADSTRPlayerState>();
			PC->PlayerState->SetIsOnlyASpectator(true);
			Mode->PostLogin(PC);
			return PC;
		}

		void Leave(APlayerController* PC)
		{
			Mode->Logout(PC);
			PC->Destroy();
		}

		void Advance()
		{
			for (int32 Index = 0; Index < 120; ++Index)
			{
				++GFrameCounter;
				World->GetTimerManager().Tick(1.0f / 60.0f);
			}
		}
	};

	constexpr uint32 LifecycleTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRNewArrivalDuringEmptyResetTest,
	"DSTR.Server.NewArrivalCannotCancelEmptyReset", LifecycleTestFlags)

bool FDSTRNewArrivalDuringEmptyResetTest::RunTest(const FString&)
{
	FServerLifecycleWorld Fixture;
	TestEqual(TEXT("Fixture runs dedicated server rules"), Fixture.World->GetNetMode(), NM_DedicatedServer);
	APlayerController* First = Fixture.Join();
	TestEqual(TEXT("One controller joined"), Fixture.World->GetNumPlayerControllers(), 1);
	Fixture.World->GetGameState<ADSTRGameState>()->SetStartCountdown(3.0f);
	Fixture.Leave(First);
	Fixture.Join(); // Same frame as the previous party leaving, before the reset timer.
	TestEqual(TEXT("Only the new controller remains"), Fixture.World->GetNumPlayerControllers(), 1);
	Fixture.Advance();
	TestFalse(TEXT("New arrival is sent to a fresh map instead of inheriting the abandoned countdown"), Fixture.World->NextURL.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRRemainingPlayerKeepsSessionTest,
	"DSTR.Server.RemainingPlayerKeepsSession", LifecycleTestFlags)

bool FDSTRRemainingPlayerKeepsSessionTest::RunTest(const FString&)
{
	FServerLifecycleWorld Fixture;
	APlayerController* First = Fixture.Join();
	Fixture.Join();
	Fixture.World->GetGameState<ADSTRGameState>()->SetStartCountdown(3.0f);
	Fixture.Leave(First);
	Fixture.Advance();
	TestTrue(TEXT("An occupied session must not reload when its leader leaves"), Fixture.World->NextURL.IsEmpty());
	return true;
}

#endif
