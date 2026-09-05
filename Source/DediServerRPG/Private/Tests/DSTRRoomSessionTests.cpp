#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Game/DSTRRoomSession.h"
#include "Game/DSTRServerStatus.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRRoomCreationRaceTest,
	"DSTR.Rooms.CreatorsCannotOverwriteEachOther", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDSTRRoomCreationRaceTest::RunTest(const FString&)
{
	FDSTRRoomSession Session;
	Session.bEnabled = true;
	const FString FirstId = TEXT("11111111111111111111111111111111");
	const FString SecondId = TEXT("22222222222222222222222222222222");
	const FDSTRRoomRequest First = FDSTRRoomRequest::FromOptions(FDSTRRoomSession::BuildCreateOptions(TEXT("Room A"), FirstId));
	const FDSTRRoomRequest Second = FDSTRRoomRequest::FromOptions(FDSTRRoomSession::BuildCreateOptions(TEXT("Room B"), SecondId));
	TestTrue(TEXT("First creator can begin loading an idle slot"), Session.CheckLogin(First).IsEmpty());
	TestTrue(TEXT("Second creator can also begin loading before first Join"), Session.CheckLogin(Second).IsEmpty());
	TestFalse(TEXT("Read-only prechecks create no phantom room"), Session.HasRoom());
	TestTrue(TEXT("First successful Join claims the room"), Session.Create(First));
	TestEqual(TEXT("Final admission rejects the competing creation"), Session.CheckLogin(Second), FString(TEXT("RoomUnavailable")));
	TestFalse(TEXT("A competing creator cannot overwrite the first room"), Session.Create(Second));
	TestEqual(TEXT("Room name is retained"), Session.RoomName, FString(TEXT("Room A")));
	TestEqual(TEXT("Generation is retained"), Session.RoomId, FirstId);
	const FDSTRRoomRequest Friend = FDSTRRoomRequest::FromOptions(FDSTRRoomSession::BuildJoinOptions(FirstId));
	TestTrue(TEXT("A friend selecting this generation is admitted"), Session.CheckLogin(Friend).IsEmpty());
	TestEqual(TEXT("A different generation is rejected"), Session.CheckLogin(
		FDSTRRoomRequest::FromOptions(FDSTRRoomSession::BuildJoinOptions(SecondId))), FString(TEXT("RoomChanged")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRRoomRequestValidationTest,
	"DSTR.Rooms.NameAndGenerationValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDSTRRoomRequestValidationTest::RunTest(const FString&)
{
	const FString Id = TEXT("11111111111111111111111111111111");
	const FString KoreanName = TEXT("친구들과 던전");
	const FDSTRRoomRequest Korean = FDSTRRoomRequest::FromOptions(FDSTRRoomSession::BuildCreateOptions(KoreanName, Id));
	TestTrue(TEXT("Korean room name roundtrips through URL options"), Korean.bValid);
	TestEqual(TEXT("Unicode name is preserved"), Korean.RoomName, KoreanName);
	const TArray<FString> BadNames = {TEXT(""), TEXT("  "), TEXT(" Room"), TEXT("Room "), TEXT("Bad|Room"), TEXT("Bad\nRoom"), FString::ChrN(25, TEXT('A'))};
	for (const FString& Name : BadNames)
	{
		TestFalse(TEXT("Bad name cannot create a room"), FDSTRRoomRequest::FromOptions(FDSTRRoomSession::BuildCreateOptions(Name, Id)).bValid);
	}
	for (const FString& BadId : {FString(), FString(TEXT("zero")), FString::ChrN(32, TEXT('0')), FString::ChrN(32, TEXT('z'))})
	{
		TestFalse(TEXT("A valid nonzero GUID is required"), FDSTRRoomRequest::FromOptions(FDSTRRoomSession::BuildCreateOptions(TEXT("Room"), BadId)).bValid);
	}
	TestFalse(TEXT("Malformed encoded name is rejected"), FDSTRRoomRequest::FromOptions(
		TEXT("?DSTRCreateRoom=1?DSTRRoomName64=invalid!?DSTRRoomId=") + Id).bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRManagedRoomProtocolTest,
	"DSTR.Rooms.StatusDistinguishesIdleAndCreated", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDSTRManagedRoomProtocolTest::RunTest(const FString&)
{
	FDSTRServerStatus Status;
	TestTrue(TEXT("Idle slot response parses"), FDSTRServerStatus::Parse(TEXT("DSTR_STATUS_V2|0|4|0|0||"), Status));
	TestTrue(TEXT("Fresh idle status allows room creation"), Status.CanCreateRoom());
	TestFalse(TEXT("Idle capacity is not a listed room"), Status.HasRoom());
	TestFalse(TEXT("Idle capacity cannot be joined as a room"), Status.CanJoin());
	TestTrue(TEXT("Created room parses"), FDSTRServerStatus::Parse(
		TEXT("DSTR_STATUS_V2|1|4|0|0|Room A|11111111111111111111111111111111"), Status));
	TestTrue(TEXT("Created room is visible and joinable"), Status.HasRoom() && Status.CanJoin());
	TestFalse(TEXT("A created room cannot be overwritten"), Status.CanCreateRoom());
	const TArray<FString> BadReplies = {
		TEXT("DSTR_STATUS_V2|1|4|0|0||"), TEXT("DSTR_STATUS_V2|0|4|1|0||"),
		TEXT("DSTR_STATUS_V2|0|4|0|1||"), TEXT("DSTR_STATUS_V2|0|4|0|0|Fake room|"),
		TEXT("DSTR_STATUS_V2|1|4|0|0|Room A|invalid"), TEXT("DSTR_STATUS_V2|1|4|0|0||11111111111111111111111111111111")
	};
	for (const FString& Reply : BadReplies)
	{
		TestFalse(TEXT("Contradictory or malformed managed status is rejected"), FDSTRServerStatus::Parse(Reply, Status));
		TestFalse(TEXT("Invalid replies enable no creation or joining"), Status.CanCreateRoom() || Status.CanJoin());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRRoomEndpointListTest,
	"DSTR.Rooms.MultipleEndpointOverride", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDSTRRoomEndpointListTest::RunTest(const FString&)
{
	const FString Original = FCommandLine::Get();
	FCommandLine::Set(TEXT("-DSTRServers=127.0.0.1:7981,127.0.0.1:7982,127.0.0.1:7983 -OtherFlag"));
	const TArray<FString> Addresses = FDSTRServerStatus::GetConfiguredAddresses();
	FCommandLine::Set(*Original);
	TestEqual(TEXT("Unquoted comma-separated override retains all three rooms"), Addresses.Num(), 3);
	TestTrue(TEXT("Last endpoint is preserved"), Addresses.Contains(TEXT("127.0.0.1:7983")));
	return true;
}

#endif
