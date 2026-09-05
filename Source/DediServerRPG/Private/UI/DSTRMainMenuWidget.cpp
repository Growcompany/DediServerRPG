#include "UI/DSTRMainMenuWidget.h"
#include "DSTRLog.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Game/DSTRGameInstance.h"
#include "Game/DSTRRoomSession.h"
#include "Game/DSTRServerStatusQuery.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Layout/Clipping.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Player/DSTRPlayerController.h"
#include "Styling/SlateTypes.h"
#include "TimerManager.h"
#include "UI/DSTRLobbyViewModel.h"
#include "UI/DSTRUIStyle.h"
#include "UnrealClient.h"

using namespace DSTRUIStyle;

namespace
{
	const FLinearColor InputTextColor(0.04f, 0.07f, 0.11f, 1.0f);
	const FLinearColor ReadyColor(0.18f, 0.85f, 0.48f, 1.0f);
#if !UE_BUILD_SHIPPING
	bool bQuickJoinSmokeConsumed = false;
	bool bCreateRoomSmokeConsumed = false;
	bool bJoinRoomSmokeConsumed = false;
#endif
}

bool UDSTRMainMenuWidget::CanHostLocalServer()
{
#if WITH_EDITOR
	return true;
#else
	return false;
#endif
}

TSharedRef<SWidget> UDSTRMainMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UDSTRMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CancelStatusQueries();
	bJoining = false;
	ServerAddresses = FDSTRServerStatus::GetConfiguredAddresses();
	if (ServerAddresses.Num() > FDSTRRoomSession::MaxRooms)
	{
		ServerAddresses.SetNum(FDSTRRoomSession::MaxRooms);
	}
	StatusQueries.SetNum(ServerAddresses.Num());
	ServerStatuses.SetNum(ServerAddresses.Num());
	NextRefreshAt.Init(0.0, ServerAddresses.Num());
	bCompletedInitialRefresh.Init(false, ServerAddresses.Num());

	if (UDSTRGameInstance* Instance = GetGameInstance<UDSTRGameInstance>())
	{
		if (!Instance->PendingNickname.IsEmpty())
		{
			NicknameBox->SetText(FText::FromString(Instance->PendingNickname));
		}
		const FString Error = FDSTRLobbyViewModel::FormatConnectionError(Instance->ConsumeConnectionError());
		if (!Error.IsEmpty())
		{
			SetStatus(TEXT("CONNECTION FAILED: ") + Error, DangerColor);
		}
	}

	bEvidenceScheduled = false;
	UpdateRoomList();
}

void UDSTRMainMenuWidget::NativeDestruct()
{
	CancelStatusQueries();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LocalServerTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(EvidenceTimerHandle);
	}
	Super::NativeDestruct();
}

void UDSTRMainMenuWidget::CancelStatusQueries()
{
	for (TObjectPtr<UDSTRServerStatusQuery>& Query : StatusQueries)
	{
		if (Query)
		{
			Query->Cancel();
			Query = nullptr;
		}
	}
}

void UDSTRMainMenuWidget::StartStatusQuery(const int32 ServerIndex)
{
	if (bJoining || !ServerAddresses.IsValidIndex(ServerIndex) || !StatusQueries.IsValidIndex(ServerIndex)
		|| StatusQueries[ServerIndex])
	{
		return;
	}
	UDSTRServerStatusQuery* Query = NewObject<UDSTRServerStatusQuery>(this);
	StatusQueries[ServerIndex] = Query;
	Query->Start(GetWorld(), ServerAddresses[ServerIndex]);
}

void UDSTRMainMenuWidget::RefreshServerStatus()
{
	if (bJoining)
	{
		return;
	}
	CancelStatusQueries();
	StatusQueries.SetNum(ServerAddresses.Num());
	for (int32 Index = 0; Index < ServerAddresses.Num(); ++Index)
	{
		StartStatusQuery(Index);
	}
	UpdateRoomList();
}

bool UDSTRMainMenuWidget::IsSlotFresh(const int32 ServerIndex) const
{
	return ServerStatuses.IsValidIndex(ServerIndex)
		&& StatusQueries.IsValidIndex(ServerIndex)
		&& !StatusQueries[ServerIndex]
		&& NextRefreshAt.IsValidIndex(ServerIndex)
		&& NextRefreshAt[ServerIndex] > FPlatformTime::Seconds();
}

int32 UDSTRMainMenuWidget::FindJoinableRoom(const FString& RequestedName) const
{
	for (int32 Index = 0; Index < ServerStatuses.Num(); ++Index)
	{
		const FDSTRServerStatus& Snapshot = ServerStatuses[Index];
		if (IsSlotFresh(Index) && Snapshot.HasRoom() && Snapshot.CanJoin()
			&& (RequestedName.IsEmpty() || Snapshot.RoomName.Equals(RequestedName, ESearchCase::IgnoreCase)))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UDSTRMainMenuWidget::FindCreatableSlot() const
{
	for (int32 Index = 0; Index < ServerStatuses.Num(); ++Index)
	{
		if (IsSlotFresh(Index) && ServerStatuses[Index].CanCreateRoom())
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UDSTRMainMenuWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bJoining)
	{
		if (UDSTRGameInstance* Instance = GetGameInstance<UDSTRGameInstance>())
		{
			const FString Error = FDSTRLobbyViewModel::FormatConnectionError(Instance->ConsumeConnectionError());
			if (!Error.IsEmpty())
			{
				bJoining = false;
				SetStatus(TEXT("CONNECTION FAILED: ") + Error, DangerColor);
				RefreshServerStatus();
			}
		}
		return;
	}

	bool bListChanged = false;
	for (int32 Index = 0; Index < StatusQueries.Num(); ++Index)
	{
		UDSTRServerStatusQuery* Query = StatusQueries[Index];
		if (!Query)
		{
			continue;
		}
		Query->Tick(InDeltaTime);
		if (Query->IsFinished())
		{
			ServerStatuses[Index] = Query->GetResult();
			Query->Cancel();
			StatusQueries[Index] = nullptr;
			NextRefreshAt[Index] = FPlatformTime::Seconds() + FDSTRServerStatus::RefreshSeconds;
			bCompletedInitialRefresh[Index] = true;
			bListChanged = true;
		}
	}

	const double Now = FPlatformTime::Seconds();
	for (int32 Index = 0; Index < ServerAddresses.Num(); ++Index)
	{
		if (!StatusQueries[Index] && Now >= NextRefreshAt[Index])
		{
			StartStatusQuery(Index);
			bListChanged = true;
		}
	}

	if (bListChanged)
	{
		UpdateRoomList();
	}

	bool bInitialRefreshComplete = ServerAddresses.Num() > 0;
	for (const bool bComplete : bCompletedInitialRefresh)
	{
		bInitialRefreshComplete &= bComplete;
	}
#if !UE_BUILD_SHIPPING
	if (bInitialRefreshComplete && !bEvidenceScheduled
		&& FParse::Param(FCommandLine::Get(), TEXT("DSTRVisualEvidence")) && GetWorld())
	{
		bEvidenceScheduled = true;
		GetWorld()->GetTimerManager().SetTimer(
			EvidenceTimerHandle, this, &UDSTRMainMenuWidget::CaptureMenuEvidence, 0.5f, false);
	}
	if (bInitialRefreshComplete)
	{
		RunDevelopmentSmokeCommands();
	}
#endif
}

void UDSTRMainMenuWidget::UpdateRoomList()
{
	int32 VisibleRooms = 0;
	int32 RespondingServers = 0;
	int32 IncompatibleServers = 0;
	int32 NoResponseServers = 0;
	bool bAnyQuery = false;
	const double Now = FPlatformTime::Seconds();

	for (int32 RowIndex = 0; RowIndex < RoomRows.Num(); ++RowIndex)
	{
		const bool bConfigured = ServerStatuses.IsValidIndex(RowIndex);
		const FDSTRServerStatus* Snapshot = bConfigured ? &ServerStatuses[RowIndex] : nullptr;
		const bool bHasRoom = Snapshot && Snapshot->HasRoom();
		RoomRows[RowIndex]->SetVisibility(bHasRoom ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bHasRoom)
		{
			continue;
		}

		++VisibleRooms;
		const bool bQuerying = StatusQueries.IsValidIndex(RowIndex) && StatusQueries[RowIndex];
		const bool bStale = !bQuerying && (!NextRefreshAt.IsValidIndex(RowIndex) || NextRefreshAt[RowIndex] <= Now);
		const FString DisplayRoomName = Snapshot->RoomName.IsEmpty() ? FString(TEXT("CO-OP ROOM")) : Snapshot->RoomName;
		RoomNameTexts[RowIndex]->SetText(FText::FromString(DisplayRoomName));
		RoomNameTexts[RowIndex]->SetToolTipText(FText::FromString(DisplayRoomName));
		PopulationTexts[RowIndex]->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d PLAYERS"), Snapshot->Players, Snapshot->Capacity)));
		const FString StateLabel = bQuerying
			? FString::Printf(TEXT("REFRESHING • LAST: %s"), Snapshot->GetLabel())
			: (bStale ? FString::Printf(TEXT("REFRESH REQUIRED • LAST: %s"), Snapshot->GetLabel()) : FString(Snapshot->GetLabel()));
		RoomStateTexts[RowIndex]->SetText(FText::FromString(StateLabel));
		const bool bCanJoin = !bJoining && !bQuerying && !bStale && Snapshot->CanJoin();
		RoomStateTexts[RowIndex]->SetColorAndOpacity(FSlateColor(
			bCanJoin ? ReadyColor
				: (Snapshot->Availability == EDSTRServerAvailability::NoResponse
					|| Snapshot->Availability == EDSTRServerAvailability::Incompatible ? DangerColor : AccentColor)));
		RoomJoinButtons[RowIndex]->SetIsEnabled(bCanJoin);
	}

	for (int32 Index = 0; Index < ServerStatuses.Num(); ++Index)
	{
		const EDSTRServerAvailability Availability = ServerStatuses[Index].Availability;
		if (Availability == EDSTRServerAvailability::Incompatible)
		{
			++IncompatibleServers;
		}
		else if (Availability == EDSTRServerAvailability::NoResponse)
		{
			++NoResponseServers;
		}
		else if (Availability != EDSTRServerAvailability::Checking)
		{
			++RespondingServers;
		}
		bAnyQuery |= StatusQueries.IsValidIndex(Index) && StatusQueries[Index];
	}

	const bool bInitialRefreshPending = bCompletedInitialRefresh.Contains(false);

	EmptyListText->SetVisibility(VisibleRooms == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	EmptyListText->SetText(FText::FromString(bInitialRefreshPending
		? TEXT("CHECKING FOR ACTIVE ROOMS...")
		: TEXT("NO ACTIVE ROOMS. CREATE ONE WHEN A SERVER SLOT IS AVAILABLE.")));

	if (!bInitialRefreshPending && IncompatibleServers > 0)
	{
		AvailabilityText->SetText(FText::FromString(FString::Printf(
			TEXT("UPDATE REQUIRED • %d VERSION MISMATCH • %d COMPATIBLE • %d NO RESPONSE"),
			IncompatibleServers, RespondingServers, NoResponseServers)));
		AvailabilityText->SetColorAndOpacity(FSlateColor(DangerColor));
	}
	else if (!bInitialRefreshPending && RespondingServers == 0)
	{
		AvailabilityText->SetText(FText::FromString(TEXT("NO RESPONSE • CREATE AND JOIN UNAVAILABLE • UP TO 3 ROOMS")));
		AvailabilityText->SetColorAndOpacity(FSlateColor(DangerColor));
	}
	else
	{
		AvailabilityText->SetText(FText::FromString(FString::Printf(
			TEXT("%d / %d ROOMS • %d / %d SERVERS RESPONDING • UP TO 3 ROOMS"),
			VisibleRooms, FDSTRRoomSession::MaxRooms, RespondingServers, ServerAddresses.Num())));
		AvailabilityText->SetColorAndOpacity(FSlateColor(MutedColor));
	}

	CreateButton->SetIsEnabled(!bJoining && FindCreatableSlot() != INDEX_NONE);
	QuickJoinButton->SetIsEnabled(!bJoining && FindJoinableRoom() != INDEX_NONE);
	RefreshButton->SetIsEnabled(!bJoining && !bAnyQuery);
}

UTextBlock* UDSTRMainMenuWidget::MakeText(const FString& Text, const int32 Size, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
	Block->SetText(FText::FromString(Text));
	Block->SetColorAndOpacity(FSlateColor(Color));
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	Block->SetFont(Font);
	return Block;
}

UButton* UDSTRMainMenuWidget::MakeButton(const FString& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	Button->SetBackgroundColor(ButtonColor);
	UTextBlock* Text = MakeText(Label, 13, FLinearColor::White);
	Text->SetJustification(ETextJustify::Center);
	Button->AddChild(Text);
	return Button;
}

void UDSTRMainMenuWidget::BuildWidgetTree()
{
	UScaleBox* ResponsiveRoot = WidgetTree->ConstructWidget<UScaleBox>();
	ResponsiveRoot->SetStretch(EStretch::ScaleToFit);
	USizeBox* DesignSurface = WidgetTree->ConstructWidget<USizeBox>();
	DesignSurface->SetWidthOverride(1280.0f);
	DesignSurface->SetHeightOverride(720.0f);
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	DesignSurface->SetContent(Root);
	ResponsiveRoot->SetContent(DesignSurface);
	WidgetTree->RootWidget = ResponsiveRoot;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 1.0f));
	UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(Backdrop);
	BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackdropSlot->SetOffsets(FMargin(0.0f));

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(24.0f, 16.0f));
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	UTextBlock* Title = MakeText(TEXT("DEDISERVERRPG"), 28, AccentColor);
	Title->SetJustification(ETextJustify::Center);
	Box->AddChildToVerticalBox(Title);
	UTextBlock* Subtitle = MakeText(TEXT("NAMED CO-OP ROOMS • 1-4 PLAYERS"), 11, MutedColor);
	Subtitle->SetJustification(ETextJustify::Center);
	Box->AddChildToVerticalBox(Subtitle)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

	UHorizontalBox* Inputs = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBox* NicknameColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	NicknameColumn->AddChildToVerticalBox(MakeText(TEXT("NICKNAME"), 10, MutedColor));
	NicknameBox = WidgetTree->ConstructWidget<UEditableTextBox>();
	FString DefaultName = FDSTRLobbyViewModel::ResolveNickname(FString(), FCommandLine::Get(), FPlatformProcess::ComputerName());
	NicknameBox->SetText(FText::FromString(DefaultName.IsEmpty() ? TEXT("Player") : DefaultName));
	NicknameBox->SetForegroundColor(InputTextColor);
	NicknameColumn->AddChildToVerticalBox(NicknameBox)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
	UHorizontalBoxSlot* NicknameSlot = Inputs->AddChildToHorizontalBox(NicknameColumn);
	NicknameSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	NicknameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UVerticalBox* RoomNameColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	RoomNameColumn->AddChildToVerticalBox(MakeText(TEXT("ROOM NAME"), 10, MutedColor));
	RoomNameBox = WidgetTree->ConstructWidget<UEditableTextBox>();
	RoomNameBox->SetText(FText::FromString(TEXT("Friends")));
	RoomNameBox->SetForegroundColor(InputTextColor);
	RoomNameColumn->AddChildToVerticalBox(RoomNameBox)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
	UHorizontalBoxSlot* RoomNameSlot = Inputs->AddChildToHorizontalBox(RoomNameColumn);
	RoomNameSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
	RoomNameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Box->AddChildToVerticalBox(Inputs)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	CreateButton = MakeButton(TEXT("CREATE ROOM"));
	CreateButton->SetIsEnabled(false);
	CreateButton->OnClicked.AddDynamic(this, &UDSTRMainMenuWidget::HandleCreateClicked);
	Box->AddChildToVerticalBox(CreateButton)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 10.0f));

	Box->AddChildToVerticalBox(MakeText(TEXT("ACTIVE ROOMS"), 10, MutedColor));
	for (int32 Index = 0; Index < FDSTRRoomSession::MaxRooms; ++Index)
	{
		UBorder* RoomRow = WidgetTree->ConstructWidget<UBorder>();
		RoomRow->SetBrushColor(FLinearColor(0.025f, 0.05f, 0.08f, 1.0f));
		RoomRow->SetPadding(FMargin(14.0f, 8.0f));
		RoomRow->SetVisibility(ESlateVisibility::Collapsed);
		UVerticalBox* RowBox = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBox* RowHeader = WidgetTree->ConstructWidget<UHorizontalBox>();
		UTextBlock* RoomTitle = MakeText(TEXT("ROOM"), 17, FLinearColor::White);
		RoomTitle->SetAutoWrapText(false);
		RoomTitle->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		RoomTitle->SetClipping(EWidgetClipping::ClipToBounds);
		UHorizontalBoxSlot* TitleSlot = RowHeader->AddChildToHorizontalBox(RoomTitle);
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UButton* DirectJoinButton = MakeButton(TEXT("JOIN"));
		DirectJoinButton->SetIsEnabled(false);
		if (Index == 0)
		{
			DirectJoinButton->OnClicked.AddDynamic(this, &UDSTRMainMenuWidget::HandleRoom0JoinClicked);
		}
		else if (Index == 1)
		{
			DirectJoinButton->OnClicked.AddDynamic(this, &UDSTRMainMenuWidget::HandleRoom1JoinClicked);
		}
		else
		{
			DirectJoinButton->OnClicked.AddDynamic(this, &UDSTRMainMenuWidget::HandleRoom2JoinClicked);
		}
		UHorizontalBoxSlot* JoinSlot = RowHeader->AddChildToHorizontalBox(DirectJoinButton);
		JoinSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		JoinSlot->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));
		RowBox->AddChildToVerticalBox(RowHeader);
		UTextBlock* State = MakeText(TEXT("CHECKING"), 10, AccentColor);
		RowBox->AddChildToVerticalBox(State)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
		UTextBlock* Population = MakeText(TEXT("-- / 4 PLAYERS"), 10, MutedColor);
		RowBox->AddChildToVerticalBox(Population);
		RoomRow->SetContent(RowBox);
		Box->AddChildToVerticalBox(RoomRow)->SetPadding(FMargin(0.0f, 3.0f));
		RoomRows.Add(RoomRow);
		RoomNameTexts.Add(RoomTitle);
		RoomStateTexts.Add(State);
		PopulationTexts.Add(Population);
		RoomJoinButtons.Add(DirectJoinButton);
	}

	EmptyListText = MakeText(TEXT("CHECKING FOR ACTIVE ROOMS..."), 12, MutedColor);
	EmptyListText->SetJustification(ETextJustify::Center);
	EmptyListText->SetAutoWrapText(true);
	Box->AddChildToVerticalBox(EmptyListText)->SetPadding(FMargin(8.0f, 14.0f));
	AvailabilityText = MakeText(TEXT("CHECKING UP TO 3 ROOM SERVERS..."), 10, MutedColor);
	AvailabilityText->SetJustification(ETextJustify::Center);
	AvailabilityText->SetAutoWrapText(true);
	Box->AddChildToVerticalBox(AvailabilityText)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 7.0f));

	if (CanHostLocalServer() && FParse::Param(FCommandLine::Get(), TEXT("DSTRLocalTools")))
	{
		UButton* HostButton = MakeButton(TEXT("LOCAL TEST SERVER"));
		HostButton->OnClicked.AddDynamic(this, &UDSTRMainMenuWidget::HandleHostClicked);
		Box->AddChildToVerticalBox(HostButton)->SetPadding(FMargin(0.0f, 2.0f));
	}
	QuickJoinButton = MakeButton(TEXT("QUICK JOIN"));
	QuickJoinButton->SetIsEnabled(false);
	QuickJoinButton->OnClicked.AddDynamic(this, &UDSTRMainMenuWidget::HandleQuickJoinClicked);
	Box->AddChildToVerticalBox(QuickJoinButton)->SetPadding(FMargin(0.0f, 2.0f));
	RefreshButton = MakeButton(TEXT("REFRESH STATUS"));
	RefreshButton->OnClicked.AddDynamic(this, &UDSTRMainMenuWidget::RefreshServerStatus);
	Box->AddChildToVerticalBox(RefreshButton)->SetPadding(FMargin(0.0f, 2.0f));
	UButton* QuitButton = MakeButton(TEXT("QUIT"));
	QuitButton->OnClicked.AddDynamic(this, &UDSTRMainMenuWidget::HandleQuitClicked);
	Box->AddChildToVerticalBox(QuitButton)->SetPadding(FMargin(0.0f, 2.0f));

	StatusText = MakeText(TEXT(""), 11, MutedColor);
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetAutoWrapText(true);
	Box->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	Panel->SetContent(Box);

	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetPosition(FVector2D::ZeroVector);
	PanelSlot->SetSize(FVector2D(620.0f, 690.0f));
}

void UDSTRMainMenuWidget::SetStatus(const FString& Text, const FLinearColor& Color)
{
	StatusText->SetText(FText::FromString(Text));
	StatusText->SetColorAndOpacity(FSlateColor(Color));
}

void UDSTRMainMenuWidget::TravelToAddress(const FString& AddressText, const FString& RoomOptions)
{
	const FDSTRServerAddress Address = FDSTRLobbyViewModel::ParseServerAddress(AddressText);
	if (!Address.bValid)
	{
		SetStatus(TEXT("INVALID SERVER CONFIGURATION"), DangerColor);
		return;
	}
	const FString Nickname = NicknameBox->GetText().ToString();
	if (UDSTRGameInstance* Instance = GetGameInstance<UDSTRGameInstance>())
	{
		Instance->PendingNickname = Nickname;
	}
	const FString Url = FDSTRLobbyViewModel::BuildJoinUrl(Address, Nickname) + RoomOptions;
	UE_LOG(LogDSTR, Log, TEXT("DSTR_MENU_JOIN Url=%s"), *Url);
	SetStatus(TEXT("CONNECTING..."), AccentColor);
	if (APlayerController* PC = GetOwningPlayer())
	{
		bJoining = true;
		CancelStatusQueries();
		UpdateRoomList();
		PC->ClientTravel(Url, ETravelType::TRAVEL_Absolute);
	}
}

void UDSTRMainMenuWidget::HandleCreateClicked()
{
	if (bJoining)
	{
		return;
	}
	const FString RoomName = RoomNameBox->GetText().ToString().TrimStartAndEnd();
	if (!FDSTRRoomSession::IsValidRoomName(RoomName))
	{
		SetStatus(FString::Printf(TEXT("ROOM NAME MUST BE 1-%d CHARACTERS"), FDSTRRoomSession::MaxNameLength), DangerColor);
		return;
	}
	const int32 ServerIndex = FindCreatableSlot();
	if (!ServerAddresses.IsValidIndex(ServerIndex))
	{
		SetStatus(TEXT("NO FRESH SERVER SLOT. REFRESH AND TRY AGAIN."), DangerColor);
		UpdateRoomList();
		return;
	}
	const FString RoomId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	TravelToAddress(ServerAddresses[ServerIndex], FDSTRRoomSession::BuildCreateOptions(RoomName, RoomId));
}

void UDSTRMainMenuWidget::JoinRoomByIndex(const int32 ServerIndex)
{
	if (bJoining || !ServerAddresses.IsValidIndex(ServerIndex) || !ServerStatuses.IsValidIndex(ServerIndex)
		|| !IsSlotFresh(ServerIndex) || !ServerStatuses[ServerIndex].HasRoom() || !ServerStatuses[ServerIndex].CanJoin())
	{
		SetStatus(TEXT("ROOM STATUS CHANGED. REFRESH AND TRY AGAIN."), DangerColor);
		UpdateRoomList();
		return;
	}
	TravelToAddress(ServerAddresses[ServerIndex], FDSTRRoomSession::BuildJoinOptions(ServerStatuses[ServerIndex].RoomId));
}

void UDSTRMainMenuWidget::HandleQuickJoinClicked()
{
	const int32 ServerIndex = FindJoinableRoom();
	if (ServerIndex == INDEX_NONE)
	{
		SetStatus(TEXT("NO JOINABLE ROOM IS AVAILABLE."), DangerColor);
		UpdateRoomList();
		return;
	}
	JoinRoomByIndex(ServerIndex);
}

void UDSTRMainMenuWidget::HandleRoom0JoinClicked()
{
	JoinRoomByIndex(0);
}

void UDSTRMainMenuWidget::HandleRoom1JoinClicked()
{
	JoinRoomByIndex(1);
}

void UDSTRMainMenuWidget::HandleRoom2JoinClicked()
{
	JoinRoomByIndex(2);
}

void UDSTRMainMenuWidget::RunDevelopmentSmokeCommands()
{
#if !UE_BUILD_SHIPPING
	FString RequestedRoom;
	if (!bCreateRoomSmokeConsumed
		&& FParse::Value(FCommandLine::Get(), TEXT("DSTRCreateRoomSmoke="), RequestedRoom)
		&& FindCreatableSlot() != INDEX_NONE)
	{
		bCreateRoomSmokeConsumed = true;
		RoomNameBox->SetText(FText::FromString(RequestedRoom));
		HandleCreateClicked();
		return;
	}
	if (!bJoinRoomSmokeConsumed
		&& FParse::Value(FCommandLine::Get(), TEXT("DSTRJoinRoomSmoke="), RequestedRoom))
	{
		const int32 ServerIndex = FindJoinableRoom(RequestedRoom.TrimStartAndEnd());
		if (ServerIndex != INDEX_NONE)
		{
			bJoinRoomSmokeConsumed = true;
			JoinRoomByIndex(ServerIndex);
			return;
		}
	}
	if (!bQuickJoinSmokeConsumed && FParse::Param(FCommandLine::Get(), TEXT("DSTRQuickJoinSmoke"))
		&& FindJoinableRoom() != INDEX_NONE)
	{
		bQuickJoinSmokeConsumed = true;
		HandleQuickJoinClicked();
	}
#endif
}

void UDSTRMainMenuWidget::HandleHostClicked()
{
	const int32 Port = FDSTRLobbyViewModel::DefaultPort;
	LocalServerAddress = FString::Printf(TEXT("127.0.0.1:%d"), Port);
	if (!FDSTRLobbyViewModel::IsPortFree(Port))
	{
		UE_LOG(LogDSTR, Log, TEXT("DSTR_MENU_HOST_SKIPPED Port=%d"), Port);
		SetStatus(TEXT("LOCAL SERVER FOUND - JOINING"), AccentColor);
		TravelToAddress(LocalServerAddress, FString());
		return;
	}

	const FString Arguments = FDSTRLobbyViewModel::BuildServerLaunchArguments(
		FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()),
		FDSTRLobbyViewModel::DungeonMapPath(),
		Port);
	uint32 ProcessId = 0;
	FProcHandle Handle = FPlatformProcess::CreateProc(
		FPlatformProcess::ExecutablePath(), *Arguments, true, false, false, &ProcessId, 0, nullptr, nullptr);
	if (!Handle.IsValid())
	{
		SetStatus(TEXT("SERVER LAUNCH FAILED"), DangerColor);
		return;
	}
	FPlatformProcess::CloseProc(Handle);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_MENU_HOST_LAUNCH Pid=%u Args=%s"), ProcessId, *Arguments);
	SetStatus(FString::Printf(TEXT("SERVER STARTING (%.0fs)..."), LocalServerFirstJoinSeconds), AccentColor);
	LocalServerJoinAttempts = 0;
	GetWorld()->GetTimerManager().SetTimer(
		LocalServerTimerHandle, this, &UDSTRMainMenuWidget::JoinLocalServer, LocalServerFirstJoinSeconds, false);
}

void UDSTRMainMenuWidget::JoinLocalServer()
{
	++LocalServerJoinAttempts;
	TravelToAddress(LocalServerAddress, FString());
	if (LocalServerJoinAttempts <= LocalServerMaxRetries && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			LocalServerTimerHandle, this, &UDSTRMainMenuWidget::RetryLocalServerJoin, LocalServerRetrySeconds, false);
	}
}

void UDSTRMainMenuWidget::RetryLocalServerJoin()
{
	UDSTRGameInstance* Instance = GetGameInstance<UDSTRGameInstance>();
	const FString Error = Instance ? Instance->ConsumeConnectionError() : FString();
	if (Error.IsEmpty())
	{
		return;
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_MENU_HOST_RETRY Attempt=%d Reason=%s"), LocalServerJoinAttempts, *Error);
	SetStatus(FString::Printf(TEXT("RETRYING %d/%d..."), LocalServerJoinAttempts, LocalServerMaxRetries), AccentColor);
	JoinLocalServer();
}

void UDSTRMainMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UDSTRMainMenuWidget::CaptureMenuEvidence()
{
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots/Windows"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory, TEXT("DSTR_MenuEvidence.png")), true, false);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_MENU_EVIDENCE_REQUESTED"));
}
