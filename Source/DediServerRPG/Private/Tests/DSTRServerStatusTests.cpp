#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "DediServerRPG/DediServerRPGGameMode.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Game/DSTRGameState.h"
#include "Game/DSTRServerStatus.h"
#include "Game/DSTRServerStatusQuery.h"
#include "GameFramework/PlayerController.h"
#include "Player/DSTRPlayerState.h"

namespace
{
	struct FStatusTestWorld
	{
		UWorld* World;
		ADediServerRPGGameMode* Mode;

		FStatusTestWorld(bool bManagedRoom = false)
		{
			World = UWorld::CreateWorld(EWorldType::PIE, false);
			GEngine->CreateNewWorldContext(EWorldType::PIE).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(World));
			World->SetGameMode(FURL(nullptr, TEXT("/Game/DSTRStatusTest?game=/Script/DediServerRPG.DediServerRPGGameMode"), TRAVEL_Absolute));
			Mode = World->GetAuthGameMode<ADediServerRPGGameMode>();
			FString Error;
			Mode->InitGame(TEXT("DSTRStatusTest"), bManagedRoom ? TEXT("?DSTRRoomServer=1") : FString(), Error);
			World->SetPlayInEditorInitialNetMode(NM_DedicatedServer);
			Mode->PreInitializeComponents();
		}

		~FStatusTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRManagedRoomAdmissionTest,
	"DSTR.Rooms.IdleSlotRequiresRoomCreation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDSTRManagedRoomAdmissionTest::RunTest(const FString&)
{
	FStatusTestWorld Fixture(true);
	FString Response;
	Fixture.Mode->PreLogin(TEXT("?DSTRStatus=1"), TEXT("127.0.0.1"), FUniqueNetIdRepl(), Response);
	TestEqual(TEXT("An idle managed server reports free capacity without a fake room name"),
		Response, FString(TEXT("DSTR_STATUS_V2|0|4|0|0||")));
	Fixture.Mode->PreLogin(TEXT("?Name=Guest"), TEXT("127.0.0.1"), FUniqueNetIdRepl(), Response);
	TestEqual(TEXT("A raw join cannot claim an uncreated room"), Response, FString(TEXT("RoomNotFound")));
	Fixture.Mode->PreLogin(TEXT("?DSTRCreateRoom=1?DSTRRoomName64=QQ==?DSTRRoomId=11111111111111111111111111111111"),
		TEXT("127.0.0.1"), FUniqueNetIdRepl(), Response);
	TestTrue(TEXT("Valid creator can begin connecting"), Response.IsEmpty());
	Fixture.Mode->PreLogin(TEXT("?DSTRStatus=1"), TEXT("127.0.0.1"), FUniqueNetIdRepl(), Response);
	TestEqual(TEXT("PreLogin alone does not create a phantom room"), Response, FString(TEXT("DSTR_STATUS_V2|0|4|0|0||")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRReadOnlyStatusRequestTest,
	"DSTR.Server.StatusRequestDoesNotJoin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDSTRReadOnlyStatusRequestTest::RunTest(const FString&)
{
	FStatusTestWorld Fixture;
	FString Response;
	Fixture.Mode->PreLogin(TEXT("?DSTRStatus=1"), TEXT("127.0.0.1"), FUniqueNetIdRepl(), Response);
	TestTrue(TEXT("Status ends the handshake with an explicit protocol reply"), Response.StartsWith(TEXT("DSTR_STATUS_V1|0|4|0|0|")));
	TestEqual(TEXT("Query creates no controller"), Fixture.World->GetNumPlayerControllers(), 0);
	TestEqual(TEXT("Query never appoints a leader"), Fixture.World->GetGameState<ADSTRGameState>()->GetHostPlayerId(), -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRRunningMatchStatusRequestTest,
	"DSTR.Server.RunningMatchReportsStatusAndRejectsJoin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDSTRRunningMatchStatusRequestTest::RunTest(const FString&)
{
	FStatusTestWorld Fixture;
	Fixture.World->GetGameState<ADSTRGameState>()->AdvancePhase(EDSTRMatchPhase::Wave);
	FString Response;
	Fixture.Mode->PreLogin(TEXT("?DSTRStatus=1"), TEXT("127.0.0.1"), FUniqueNetIdRepl(), Response);
	TestTrue(TEXT("A running match still answers status requests"), Response.StartsWith(TEXT("DSTR_STATUS_V1|0|4|1|0|")));
	Fixture.Mode->PreLogin(TEXT("?Name=LateJoin"), TEXT("127.0.0.1"), FUniqueNetIdRepl(), Response);
	TestEqual(TEXT("Gameplay admission rechecks the match phase"), Response, FString(TEXT("MatchInProgress")));
	APlayerController* LatePlayer = Fixture.Mode->Login(nullptr, ROLE_AutonomousProxy, FString(),
		TEXT("?Name=LateJoin"), FUniqueNetIdRepl(), Response);
	TestNull(TEXT("An earlier accepted handshake cannot bypass the new match phase"), LatePlayer);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRFullLobbyStatusRequestTest,
	"DSTR.Server.FullLobbyStillReportsStatus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDSTRFullLobbyStatusRequestTest::RunTest(const FString&)
{
	FStatusTestWorld Fixture;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		APlayerController* PC = Fixture.World->SpawnActor<APlayerController>();
		Fixture.World->AddController(PC);
		PC->PlayerState = Fixture.World->SpawnActor<ADSTRPlayerState>();
	}
	TestEqual(TEXT("Fixture has four admitted players"), Fixture.Mode->GetNumPlayers(), 4);
	FString Response;
	Fixture.Mode->PreLogin(TEXT("?DSTRStatus=1"), TEXT("127.0.0.1"), FUniqueNetIdRepl(), Response);
	FDSTRServerStatus Status;
	TestTrue(TEXT("A full server still provides a valid status response"), FDSTRServerStatus::Parse(Response, Status));
	TestEqual(TEXT("All slots are reported"), Status.Players, 4);
	TestFalse(TEXT("A full lobby cannot be joined"), Status.CanJoin());
	Fixture.Mode->PreLogin(TEXT("?Name=Fifth"), TEXT("127.0.0.1"), FUniqueNetIdRepl(), Response);
	TestEqual(TEXT("Normal admission rejects a fifth player"), Response, FString(TEXT("ServerFull")));
	TestNull(TEXT("The final Join stage rechecks capacity"), Fixture.Mode->Login(nullptr, ROLE_AutonomousProxy,
		FString(), TEXT("?Name=Fifth"), FUniqueNetIdRepl(), Response));
	TestEqual(TEXT("Status never creates a fifth controller"), Fixture.World->GetNumPlayerControllers(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRCountdownStatusRequestTest,
	"DSTR.Server.CountdownClosesAdmission", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDSTRCountdownStatusRequestTest::RunTest(const FString&)
{
	FStatusTestWorld Fixture;
	ADSTRGameState* State = Fixture.World->GetGameState<ADSTRGameState>();
	State->SetStartCountdown(3.0f);
	FString Response;
	Fixture.Mode->PreLogin(TEXT("?DSTRStatus=1"), TEXT("127.0.0.1"), FUniqueNetIdRepl(), Response);
	FDSTRServerStatus Status;
	TestTrue(TEXT("Countdown response parses"), FDSTRServerStatus::Parse(Response, Status));
	TestTrue(TEXT("Countdown is reported"), Status.bCountdown);
	TestFalse(TEXT("Countdown cannot be joined"), Status.CanJoin());
	TestTrue(TEXT("Status query leaves countdown active"), State->IsCountdownActive());
	Fixture.Mode->PreLogin(TEXT("?Name=LateJoin"), TEXT("127.0.0.1"), FUniqueNetIdRepl(), Response);
	TestEqual(TEXT("Countdown rejects gameplay admission"), Response, FString(TEXT("MatchInProgress")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRStatusProtocolTest,
	"DSTR.Server.StatusProtocolRejectsInvalidReplies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDSTRStatusProtocolTest::RunTest(const FString&)
{
	FDSTRServerStatus Status;
	TestTrue(TEXT("Empty available lobby parses"), FDSTRServerStatus::Parse(TEXT("DSTR_STATUS_V1|0|4|0|0|PUBLIC DUNGEON"), Status));
	TestTrue(TEXT("An available lobby allows joining"), Status.CanJoin());
	const TArray<FString> Invalid = {
		TEXT("ServerFull"), TEXT("DSTR_STATUS_V2|0|4|0|0|Room"),
		TEXT("DSTR_STATUS_V1|-1|4|0|0|Room"), TEXT("DSTR_STATUS_V1|1.0|4|0|0|Room"),
		TEXT("DSTR_STATUS_V1|5|4|0|0|Room"), TEXT("DSTR_STATUS_V1|0|0|0|0|Room"),
		TEXT("DSTR_STATUS_V1|0|4|99|0|Room"), TEXT("DSTR_STATUS_V1|0|4|0|2|Room"),
		TEXT("DSTR_STATUS_V1|0|4|0|0|"), TEXT("DSTR_STATUS_V1|0|4|0|0|Room|Extra"),
		TEXT("DSTR_STATUS_V1|0|4|0|0|Room\nInjected"), FString::ChrN(257, TEXT('x'))
	};
	for (const FString& Reply : Invalid)
	{
		Status.Availability = EDSTRServerAvailability::Available;
		TestFalse(TEXT("Invalid reply is rejected"), FDSTRServerStatus::Parse(Reply, Status));
		TestFalse(TEXT("Invalid reply clears previously available state"), Status.CanJoin());
	}
	for (int32 Phase = 1; Phase <= static_cast<int32>(EDSTRMatchPhase::Failed); ++Phase)
	{
		TestTrue(TEXT("All known active/finished phases parse"), FDSTRServerStatus::Parse(
			FString::Printf(TEXT("DSTR_STATUS_V1|2|4|%d|0|Room"), Phase), Status));
		TestFalse(TEXT("Mid-match and completed parties cannot accept new players"), Status.CanJoin());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRQueryIsolationTest,
	"DSTR.Server.QueryVersionFailureAndCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDSTRQueryIsolationTest::RunTest(const FString&)
{
	FStatusTestWorld Fixture;
	UNetDriver* OriginalGameDriver = Fixture.World->GetNetDriver();
	UDSTRServerStatusQuery* Query = NewObject<UDSTRServerStatusQuery>(Fixture.World);
	Query->Start(Fixture.World, TEXT("127.0.0.1:7999"));
	if (!TestNotNull(TEXT("Query owns a separate driver"), Query->NetDriver.Get()))
	{
		return false;
	}
	const FName QueryDriverName = Query->NetDriver->NetDriverName;
	GEngine->OnNetworkFailure().Broadcast(Fixture.World, Query->NetDriver,
		ENetworkFailure::OutdatedClient, TEXT("Status handshake version test"));
	TestTrue(TEXT("Early version rejection completes the query"), Query->IsFinished());
	TestTrue(TEXT("An explicit version failure is not reported as offline"),
		Query->GetResult().Availability == EDSTRServerAvailability::Incompatible);
	Query->Tick(0.0f);
	TestNull(TEXT("Finished query removes its named driver outside dispatch"), GEngine->FindNamedNetDriver(Fixture.World, QueryDriverName));
	TestTrue(TEXT("The gameplay driver is untouched"), Fixture.World->GetNetDriver() == OriginalGameDriver);
	Query->Start(Fixture.World, TEXT("127.0.0.1:7999"));
	TestFalse(TEXT("A cancelled or failed query can be reused"), Query->IsFinished());
	Query->Cancel();
	Query->Cancel();
	TestNull(TEXT("Repeated cancellation leaves no driver"), GEngine->FindNamedNetDriver(Fixture.World, QueryDriverName));
	TestEqual(TEXT("A status handshake never adds a controller"), Fixture.World->GetNumPlayerControllers(), 0);
	return true;
}

#endif
