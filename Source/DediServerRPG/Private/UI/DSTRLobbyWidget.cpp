#include "UI/DSTRLobbyWidget.h"
#include "DSTRLog.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Game/DSTRGameState.h"
#include "GameFramework/PlayerState.h"
#include "Player/DSTRPlayerController.h"
#include "Player/DSTRPlayerState.h"
#include "Presentation/DSTRVisualAssetRegistry.h"
#include "UI/DSTRHUDViewModel.h"
#include "UI/DSTRLobbyViewModel.h"

namespace
{
	const FLinearColor PanelColor(0.015f, 0.025f, 0.045f, 0.92f);
	const FLinearColor AccentColor(0.05f, 0.75f, 1.0f, 1.0f);
	const FLinearColor MutedColor(0.62f, 0.72f, 0.82f, 1.0f);
	const FLinearColor ButtonColor(0.08f, 0.32f, 0.50f, 1.0f);
}

TSharedRef<SWidget> UDSTRLobbyWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UDSTRLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ADSTRPlayerController* PC = Cast<ADSTRPlayerController>(GetOwningPlayer()))
	{
		PC->SetLobbyInputMode();
	}
}

UTextBlock* UDSTRLobbyWidget::MakeText(const FString& Text, const int32 Size, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
	Block->SetText(FText::FromString(Text));
	Block->SetColorAndOpacity(FSlateColor(Color));
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	Block->SetFont(Font);
	return Block;
}

UButton* UDSTRLobbyWidget::MakeButton(const FString& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	Button->SetBackgroundColor(ButtonColor);
	UTextBlock* Text = MakeText(Label, 14, FLinearColor::White);
	Text->SetJustification(ETextJustify::Center);
	Button->AddChild(Text);
	return Button;
}

void UDSTRLobbyWidget::BuildWidgetTree()
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

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(20.0f, 16.0f));
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	UTextBlock* Title = MakeText(TEXT("LOBBY"), 22, AccentColor);
	Title->SetJustification(ETextJustify::Center);
	Box->AddChildToVerticalBox(Title);
	HintText = MakeText(TEXT("HOST STARTS THE DUNGEON"), 12, MutedColor);
	HintText->SetJustification(ETextJustify::Center);
	Box->AddChildToVerticalBox(HintText)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 2.0f));
	UTextBlock* SkillKeysText = MakeText(
		TEXT("LMB ATTACK  RMB MAKE WAY  Q FORTIFY  E ASSAULT  R RECKONING"), 10, MutedColor);
	SkillKeysText->SetJustification(ETextJustify::Center);
	Box->AddChildToVerticalBox(SkillKeysText);
	UTextBlock* VerbKeysText = MakeText(TEXT("SPACE JUMP  F INTERACT"), 10, MutedColor);
	VerbKeysText->SetJustification(ETextJustify::Center);
	Box->AddChildToVerticalBox(VerbKeysText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UTextBlock* SlotText = MakeText(FDSTRLobbyViewModel::FormatLobbySlot(Index + 1, TEXT(""), false, false), 14, MutedColor);
		SlotTexts.Add(SlotText);
		Box->AddChildToVerticalBox(SlotText)->SetPadding(FMargin(0.0f, 2.0f));
	}

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	StartButton = MakeButton(TEXT("START"));
	StartButton->OnClicked.AddDynamic(this, &UDSTRLobbyWidget::HandleStartClicked);
	StartButton->SetVisibility(ESlateVisibility::Collapsed);
	UButton* LeaveButton = MakeButton(TEXT("LEAVE"));
	LeaveButton->OnClicked.AddDynamic(this, &UDSTRLobbyWidget::HandleLeaveClicked);
	for (UButton* Button : {StartButton.Get(), LeaveButton})
	{
		UHorizontalBoxSlot* ButtonSlot = Buttons->AddChildToHorizontalBox(Button);
		ButtonSlot->SetPadding(FMargin(6.0f, 0.0f));
		ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	Box->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));
	Panel->SetContent(Box);

	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetPosition(FVector2D(0.0f, -20.0f));
	PanelSlot->SetSize(FVector2D(520.0f, 300.0f));
}

void UDSTRLobbyWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bClosed)
	{
		return;
	}
	const ADSTRGameState* State = GetWorld() ? GetWorld()->GetGameState<ADSTRGameState>() : nullptr;
	if (!State)
	{
		return;
	}
	if (State->GetMatchPhase() != EDSTRMatchPhase::WaitingForPlayers)
	{
		bClosed = true;
		if (ADSTRPlayerController* PC = Cast<ADSTRPlayerController>(GetOwningPlayer()))
		{
			PC->SetCombatInputMode();
		}
		RemoveFromParent();
		return;
	}

	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator < RefreshInterval)
	{
		return;
	}
	RefreshAccumulator = 0.0f;

	const APlayerState* LocalState = GetOwningPlayerState();
	const bool bIsHost = State->IsHost(LocalState);
	for (int32 Index = 0; Index < SlotTexts.Num(); ++Index)
	{
		const APlayerState* Member = State->PlayerArray.IsValidIndex(Index) ? State->PlayerArray[Index].Get() : nullptr;
		const FString Name = Member ? Member->GetPlayerName() : FString();
		const ADSTRPlayerState* MemberState = Cast<ADSTRPlayerState>(Member);
		SlotTexts[Index]->SetText(FText::FromString(FDSTRLobbyViewModel::FormatLobbySlot(
			Index + 1, Name, Member && State->IsHost(Member), MemberState && MemberState->IsPresentationReady())));
		SlotTexts[Index]->SetColorAndOpacity(FSlateColor(
			Member && Member == LocalState ? AccentColor : (Member ? FLinearColor::White : MutedColor)));
	}
	const bool bCountingDown = State->IsCountdownActive();
	const bool bAssetsLoaded = FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(this);
	const bool bAllReady = FDSTRLobbyViewModel::AreAllReady(State->CountPresentationReadyPlayers(), State->PlayerArray.Num());
	HintText->SetText(FText::FromString(bCountingDown
		? FDSTRHUDViewModel::FormatCountdown(State->GetCountdownRemaining())
		: (!bAssetsLoaded ? TEXT("LOADING ASSETS...")
			: (bIsHost
				? (bAllReady ? TEXT("PRESS START WHEN EVERYONE IS IN") : TEXT("WAITING FOR PLAYERS TO LOAD"))
				: TEXT("HOST STARTS THE DUNGEON")))));
	HintText->SetColorAndOpacity(FSlateColor(bCountingDown ? AccentColor : MutedColor));
	StartButton->SetVisibility(
		!bCountingDown && FDSTRLobbyViewModel::CanHostStart(bIsHost, State->GetMatchPhase(), State->PlayerArray.Num(), bAssetsLoaded && bAllReady)
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UDSTRLobbyWidget::HandleStartClicked()
{
	UE_LOG(LogDSTR, Log, TEXT("DSTR_UI_CLICK Button=LobbyStart"));
	if (ADSTRPlayerController* PC = Cast<ADSTRPlayerController>(GetOwningPlayer()))
	{
		PC->Server_RequestStartMatch();
	}
}

void UDSTRLobbyWidget::HandleLeaveClicked()
{
	UE_LOG(LogDSTR, Log, TEXT("DSTR_UI_CLICK Button=LobbyLeave"));
	if (ADSTRPlayerController* PC = Cast<ADSTRPlayerController>(GetOwningPlayer()))
	{
		PC->ReturnToMainMenu();
	}
}
