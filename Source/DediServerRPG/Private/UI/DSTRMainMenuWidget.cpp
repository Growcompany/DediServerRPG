#include "UI/DSTRMainMenuWidget.h"
#include "DSTRLog.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Game/DSTRGameInstance.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Player/DSTRPlayerController.h"
#include "TimerManager.h"
#include "UI/DSTRLobbyViewModel.h"
#include "UnrealClient.h"

namespace
{
	const FLinearColor PanelColor(0.015f, 0.025f, 0.045f, 0.92f);
	const FLinearColor AccentColor(0.05f, 0.75f, 1.0f, 1.0f);
	const FLinearColor MutedColor(0.62f, 0.72f, 0.82f, 1.0f);
	const FLinearColor DangerColor(1.0f, 0.18f, 0.14f, 1.0f);
	const FLinearColor ButtonColor(0.08f, 0.32f, 0.50f, 1.0f);
	const FLinearColor InputTextColor(0.04f, 0.07f, 0.11f, 1.0f);
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
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("DSTRVisualEvidence")) && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			EvidenceTimerHandle, this, &UDSTRMainMenuWidget::CaptureMenuEvidence, 3.0f, false);
	}
#endif
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
	UTextBlock* Text = MakeText(Label, 14, FLinearColor::White);
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
	Panel->SetPadding(FMargin(28.0f, 20.0f));
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	UTextBlock* Title = MakeText(TEXT("DEDISERVERRPG"), 30, AccentColor);
	Title->SetJustification(ETextJustify::Center);
	Box->AddChildToVerticalBox(Title);
	UTextBlock* Subtitle = MakeText(TEXT("DEDICATED SERVER CO-OP"), 12, MutedColor);
	Subtitle->SetJustification(ETextJustify::Center);
	Box->AddChildToVerticalBox(Subtitle)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 18.0f));

	Box->AddChildToVerticalBox(MakeText(TEXT("NICKNAME"), 11, MutedColor));
	NicknameBox = WidgetTree->ConstructWidget<UEditableTextBox>();
	FString DefaultName = FDSTRLobbyViewModel::ResolveNickname(FString(), FCommandLine::Get(), FPlatformProcess::ComputerName());
	if (DefaultName.IsEmpty())
	{
		DefaultName = TEXT("Player");
	}
	NicknameBox->SetText(FText::FromString(DefaultName));
	NicknameBox->SetForegroundColor(InputTextColor);
	Box->AddChildToVerticalBox(NicknameBox)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 10.0f));

	Box->AddChildToVerticalBox(MakeText(TEXT("SERVER ADDRESS"), 11, MutedColor));
	AddressBox = WidgetTree->ConstructWidget<UEditableTextBox>();
	AddressBox->SetText(FText::FromString(FString::Printf(TEXT("127.0.0.1:%d"), FDSTRLobbyViewModel::DefaultPort)));
	AddressBox->SetForegroundColor(InputTextColor);
	Box->AddChildToVerticalBox(AddressBox)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 14.0f));

	if (CanHostLocalServer())
	{
		UButton* HostButton = MakeButton(TEXT("HOST LOCAL SERVER"));
		HostButton->OnClicked.AddDynamic(this, &UDSTRMainMenuWidget::HandleHostClicked);
		Box->AddChildToVerticalBox(HostButton)->SetPadding(FMargin(0.0f, 4.0f));
	}
	UButton* JoinButton = MakeButton(TEXT("JOIN"));
	JoinButton->OnClicked.AddDynamic(this, &UDSTRMainMenuWidget::HandleJoinClicked);
	Box->AddChildToVerticalBox(JoinButton)->SetPadding(FMargin(0.0f, 4.0f));
	UButton* QuitButton = MakeButton(TEXT("QUIT"));
	QuitButton->OnClicked.AddDynamic(this, &UDSTRMainMenuWidget::HandleQuitClicked);
	Box->AddChildToVerticalBox(QuitButton)->SetPadding(FMargin(0.0f, 4.0f));

	StatusText = MakeText(TEXT(""), 12, MutedColor);
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetAutoWrapText(true);
	Box->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));
	Panel->SetContent(Box);

	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetPosition(FVector2D::ZeroVector);
	PanelSlot->SetSize(FVector2D(480.0f, 400.0f));
}

void UDSTRMainMenuWidget::SetStatus(const FString& Text, const FLinearColor& Color)
{
	StatusText->SetText(FText::FromString(Text));
	StatusText->SetColorAndOpacity(FSlateColor(Color));
}

void UDSTRMainMenuWidget::JoinAddress(const FString& AddressText)
{
	const FDSTRServerAddress Address = FDSTRLobbyViewModel::ParseServerAddress(AddressText);
	if (!Address.bValid)
	{
		SetStatus(TEXT("INVALID ADDRESS"), DangerColor);
		return;
	}
	const FString Nickname = NicknameBox->GetText().ToString();
	if (UDSTRGameInstance* Instance = GetGameInstance<UDSTRGameInstance>())
	{
		Instance->PendingNickname = Nickname;
	}
	const FString Url = FDSTRLobbyViewModel::BuildJoinUrl(Address, Nickname);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_MENU_JOIN Url=%s"), *Url);
	SetStatus(TEXT("CONNECTING..."), AccentColor);
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->ClientTravel(Url, ETravelType::TRAVEL_Absolute);
	}
}

void UDSTRMainMenuWidget::HandleJoinClicked()
{
	JoinAddress(AddressBox->GetText().ToString());
}

void UDSTRMainMenuWidget::HandleHostClicked()
{
	const int32 Port = FDSTRLobbyViewModel::DefaultPort;
	AddressBox->SetText(FText::FromString(FString::Printf(TEXT("127.0.0.1:%d"), Port)));
	if (!FDSTRLobbyViewModel::IsPortFree(Port))
	{
		UE_LOG(LogDSTR, Log, TEXT("DSTR_MENU_HOST_SKIPPED Port=%d"), Port);
		SetStatus(FString::Printf(TEXT("SERVER ALREADY ON %d - JOINING"), Port), AccentColor);
		JoinAddress(AddressBox->GetText().ToString());
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
	JoinAddress(AddressBox->GetText().ToString());
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
