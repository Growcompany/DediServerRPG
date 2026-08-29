#include "UI/DSTRCombatHUDWidget.h"
#include "DSTRLog.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DSTRAttributeSet.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Game/DSTRActorRegistry.h"
#include "Game/DSTRDungeonRules.h"
#include "Game/DSTRGameState.h"
#include "GameplayEffect.h"
#include "Player/DSTRPlayerController.h"
#include "Player/DSTRPlayerState.h"
#include "UI/DSTRHUDViewModel.h"
#include "UI/DSTRMinimapViewModel.h"
#include "World/DSTRAttackBuffPickup.h"
#include "World/DSTRBossGate.h"

namespace
{
	const FLinearColor PanelColor(0.015f, 0.025f, 0.045f, 0.88f);
	const FLinearColor AccentColor(0.05f, 0.75f, 1.0f, 1.0f);
	const FLinearColor MutedColor(0.62f, 0.72f, 0.82f, 1.0f);
	const FLinearColor ReadyColor(0.15f, 0.95f, 0.72f, 1.0f);
	const FLinearColor DangerColor(1.0f, 0.18f, 0.14f, 1.0f);
	const FLinearColor AllyMarkerColor(0.3f, 0.6f, 1.0f, 1.0f);
	const FLinearColor EnemyMarkerColor(0.9f, 0.2f, 0.2f, 1.0f);
	const FLinearColor BuffMarkerColor(1.0f, 0.85f, 0.2f, 1.0f);
	const FLinearColor DoorMarkerColor(1.0f, 0.55f, 0.1f, 1.0f);
	const FLinearColor BossRoomMarkerColor(0.62f, 0.04f, 0.06f, 1.0f);
	const FLinearColor RouteMarkerColor(0.78f, 0.84f, 0.94f, 1.0f);
	const FLinearColor GateSealedMarkerColor(0.24f, 0.52f, 1.0f, 1.0f);
	const FLinearColor GateOpenMarkerColor(0.60f, 0.88f, 1.0f, 1.0f);
	const FLinearColor GateLockedMarkerColor(0.95f, 0.18f, 0.14f, 1.0f);
	constexpr float GateSegmentSpacing = 200.0f;

	FSlateBrush MakeTeardropBrush(const float Size, const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.ImageSize = FVector2D(Size, Size);
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(0.0, Size * 0.5, Size * 0.5, Size * 0.5);
		return Brush;
	}

	ADSTRAttackBuffPickup* FindNearestBuff(
		const TArray<TObjectPtr<ADSTRAttackBuffPickup>>& Pickups,
		const FVector& From,
		const float MaxRange)
	{
		ADSTRAttackBuffPickup* Best = nullptr;
		float BestSquared = FMath::Square(MaxRange);
		for (ADSTRAttackBuffPickup* Pickup : Pickups)
		{
			const float DistanceSquared = FVector::DistSquared(From, Pickup->GetActorLocation());
			if (DistanceSquared <= BestSquared)
			{
				BestSquared = DistanceSquared;
				Best = Pickup;
			}
		}
		return Best;
	}

	ADediServerRPGCharacter* FindNearestDownedTeammate(
		const TArray<TObjectPtr<ADediServerRPGCharacter>>& Party,
		const AActor* Self)
	{
		ADediServerRPGCharacter* Best = nullptr;
		float BestSquared = TNumericLimits<float>::Max();
		const FVector From = Self ? Self->GetActorLocation() : FVector::ZeroVector;
		for (ADediServerRPGCharacter* Member : Party)
		{
			const float DistanceSquared = FVector::DistSquared(From, Member->GetActorLocation());
			if (Member != Self && Member->IsDowned() && !Member->IsEliminated() && DistanceSquared < BestSquared)
			{
				BestSquared = DistanceSquared;
				Best = Member;
			}
		}
		return Best;
	}

	ADSTREnemyCharacter* FindNearestMinion(
		const TArray<TObjectPtr<ADSTREnemyCharacter>>& LivingEnemies,
		const FVector& From)
	{
		ADSTREnemyCharacter* Best = nullptr;
		float BestSquared = TNumericLimits<float>::Max();
		for (ADSTREnemyCharacter* Enemy : LivingEnemies)
		{
			const float DistanceSquared = FVector::DistSquared(From, Enemy->GetActorLocation());
			if (!Enemy->IsBoss() && DistanceSquared < BestSquared)
			{
				BestSquared = DistanceSquared;
				Best = Enemy;
			}
		}
		return Best;
	}
}

void UDSTRCombatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshHUD();
}

TSharedRef<SWidget> UDSTRCombatHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UDSTRCombatHUDWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.1f)
	{
		RefreshAccumulator = 0.0f;
		RefreshHUD();
	}
}

UTextBlock* UDSTRCombatHUDWidget::MakeText(
	const FString& Text,
	const int32 Size,
	const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
	Block->SetText(FText::FromString(Text));
	Block->SetColorAndOpacity(FSlateColor(Color));
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	Block->SetFont(Font);
	return Block;
}

UBorder* UDSTRCombatHUDWidget::MakePanel(const FLinearColor& Color)
{
	UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
	Border->SetBrushColor(Color);
	Border->SetPadding(FMargin(12.0f, 8.0f));
	return Border;
}

void UDSTRCombatHUDWidget::AddPanelToCanvas(
	UCanvasPanel* Canvas,
	UBorder* Panel,
	const FVector2D& Anchor,
	const FVector2D& Position,
	const FVector2D& Size,
	const FVector2D& Alignment)
{
	UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Panel);
	CanvasSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
	CanvasSlot->SetAlignment(Alignment);
	CanvasSlot->SetPosition(Position);
	CanvasSlot->SetSize(Size);
}

void UDSTRCombatHUDWidget::BuildWidgetTree()
{
	UScaleBox* ResponsiveRoot = WidgetTree->ConstructWidget<UScaleBox>();
	ResponsiveRoot->SetStretch(EStretch::ScaleToFit);
	ResponsiveRoot->SetStretchDirection(EStretchDirection::Both);
	USizeBox* DesignSurface = WidgetTree->ConstructWidget<USizeBox>();
	DesignSurface->SetWidthOverride(1280.0f);
	DesignSurface->SetHeightOverride(720.0f);
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	DesignSurface->SetContent(Root);
	ResponsiveRoot->SetContent(DesignSurface);
	WidgetTree->RootWidget = ResponsiveRoot;

	UBorder* PhasePanel = MakePanel(PanelColor);
	UVerticalBox* PhaseBox = WidgetTree->ConstructWidget<UVerticalBox>();
	PhaseText = MakeText(TEXT("WAITING FOR PLAYERS"), 18, AccentColor);
	PhaseText->SetJustification(ETextJustify::Center);
	RunInfoText = MakeText(TEXT("Synchronizing with dedicated server"), 12, MutedColor);
	RunInfoText->SetJustification(ETextJustify::Center);
	PhaseBox->AddChildToVerticalBox(PhaseText);
	PhaseBox->AddChildToVerticalBox(RunInfoText);
	PhasePanel->SetContent(PhaseBox);
	AddPanelToCanvas(Root, PhasePanel, FVector2D(0.5f, 0.0f), FVector2D(0.0f, 20.0f), FVector2D(440.0f, 60.0f), FVector2D(0.5f, 0.0f));

	ObjectivePanel = MakePanel(FLinearColor(0.02f, 0.10f, 0.16f, 0.85f));
	ObjectivePanel->SetPadding(FMargin(12.0f, 4.0f));
	ObjectiveText = MakeText(TEXT(""), 13, ReadyColor);
	ObjectiveText->SetJustification(ETextJustify::Center);
	ObjectivePanel->SetContent(ObjectiveText);
	ObjectivePanel->SetClipping(EWidgetClipping::ClipToBounds);
	AddPanelToCanvas(Root, ObjectivePanel, FVector2D(0.5f, 0.0f), FVector2D(0.0f, 88.0f), FVector2D(440.0f, 32.0f), FVector2D(0.5f, 0.0f));
	ObjectivePanelSlot = Cast<UCanvasPanelSlot>(ObjectivePanel->Slot);
	ObjectivePanel->SetVisibility(ESlateVisibility::Collapsed);

	UBorder* NetworkPanel = MakePanel(FLinearColor(0.02f, 0.10f, 0.16f, 0.9f));
	NetworkText = MakeText(TEXT("DEDICATED CLIENT"), 11, AccentColor);
	NetworkText->SetJustification(ETextJustify::Center);
	NetworkPanel->SetContent(NetworkText);
	AddPanelToCanvas(Root, NetworkPanel, FVector2D(1.0f, 0.0f), FVector2D(-20.0f, 20.0f), FVector2D(250.0f, 34.0f), FVector2D(1.0f, 0.0f));

	UBorder* PartyPanel = MakePanel(PanelColor);
	UVerticalBox* PartyBox = WidgetTree->ConstructWidget<UVerticalBox>();
	PartyBox->AddChildToVerticalBox(MakeText(TEXT("CO-OP PARTY"), 14, AccentColor));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UTextBlock* Row = MakeText(TEXT("-- EMPTY SLOT --"), 12, MutedColor);
		PartyRows.Add(Row);
		PartyBox->AddChildToVerticalBox(Row);
	}
	PartyPanel->SetContent(PartyBox);
	PartyPanel->SetClipping(EWidgetClipping::ClipToBounds);
	AddPanelToCanvas(Root, PartyPanel, FVector2D(0.0f, 0.0f), FVector2D(20.0f, 20.0f), FVector2D(290.0f, 118.0f));

	BossPanel = MakePanel(FLinearColor(0.12f, 0.01f, 0.015f, 0.9f));
	UVerticalBox* BossBox = WidgetTree->ConstructWidget<UVerticalBox>();
	BossText = MakeText(TEXT("SEVAROG"), 13, DangerColor);
	BossText->SetJustification(ETextJustify::Center);
	BossBar = WidgetTree->ConstructWidget<UProgressBar>();
	BossBar->SetFillColorAndOpacity(DangerColor);
	BossBox->AddChildToVerticalBox(BossText);
	BossBox->AddChildToVerticalBox(BossBar);
	BossPanel->SetContent(BossBox);
	AddPanelToCanvas(Root, BossPanel, FVector2D(0.5f, 0.0f), FVector2D(0.0f, 88.0f), FVector2D(440.0f, 46.0f), FVector2D(0.5f, 0.0f));
	BossPanel->SetVisibility(ESlateVisibility::Collapsed);

	UBorder* StatsPanel = MakePanel(PanelColor);
	UVerticalBox* StatsBox = WidgetTree->ConstructWidget<UVerticalBox>();
	StatsBox->AddChildToVerticalBox(MakeText(TEXT("COMBAT STATUS"), 13, AccentColor));
	HealthText = MakeText(TEXT("HP 100 / 100"), 12, FLinearColor::White);
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>();
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.1f, 0.9f, 0.35f, 1.0f));
	StaminaText = MakeText(TEXT("STAMINA 100 / 100"), 11, FLinearColor::White);
	StaminaBar = WidgetTree->ConstructWidget<UProgressBar>();
	StaminaBar->SetFillColorAndOpacity(FLinearColor(0.1f, 0.65f, 1.0f, 1.0f));
	StatsBox->AddChildToVerticalBox(HealthText);
	StatsBox->AddChildToVerticalBox(HealthBar);
	StatsBox->AddChildToVerticalBox(StaminaText);
	StatsBox->AddChildToVerticalBox(StaminaBar);
	StatsPanel->SetContent(StatsBox);
	AddPanelToCanvas(Root, StatsPanel, FVector2D(0.0f, 1.0f), FVector2D(20.0f, -20.0f), FVector2D(280.0f, 104.0f), FVector2D(0.0f, 1.0f));

	UBorder* AbilityPanel = MakePanel(PanelColor);
	UHorizontalBox* AbilityBox = WidgetTree->ConstructWidget<UHorizontalBox>();
	const TCHAR* Keys[] = {TEXT("LMB"), TEXT("RMB"), TEXT("Q"), TEXT("E"), TEXT("R")};
	const TCHAR* Names[] = {
		TEXT("BASIC"), TEXT("MAKE WAY"), TEXT("FORTIFY"), TEXT("ASSAULT"), TEXT("RECKON")};
	for (int32 Index = 0; Index < AbilityCardCount; ++Index)
	{
		UBorder* Card = MakePanel(FLinearColor(0.025f, 0.06f, 0.09f, 0.95f));
		UVerticalBox* CardBox = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* Key = MakeText(Keys[Index], 12, AccentColor);
		Key->SetJustification(ETextJustify::Center);
		UTextBlock* Name = MakeText(Names[Index], 10, FLinearColor::White);
		Name->SetJustification(ETextJustify::Center);
		UTextBlock* Cooldown = MakeText(TEXT("READY"), 10, ReadyColor);
		Cooldown->SetJustification(ETextJustify::Center);
		UProgressBar* CooldownBar = WidgetTree->ConstructWidget<UProgressBar>();
		CooldownBar->SetFillColorAndOpacity(ReadyColor);
		CooldownBar->SetPercent(1.0f);
		AbilityCooldownTexts.Add(Cooldown);
		AbilityNameTexts.Add(Name);
		AbilityCooldownBars.Add(CooldownBar);
		CardBox->AddChildToVerticalBox(Key);
		CardBox->AddChildToVerticalBox(Name);
		CardBox->AddChildToVerticalBox(Cooldown);
		CardBox->AddChildToVerticalBox(CooldownBar);
		Card->SetContent(CardBox);
		UHorizontalBoxSlot* CardSlot = AbilityBox->AddChildToHorizontalBox(Card);
		CardSlot->SetPadding(FMargin(3.0f, 0.0f));
		CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	AbilityPanel->SetContent(AbilityBox);
	AddPanelToCanvas(Root, AbilityPanel, FVector2D(0.5f, 1.0f), FVector2D(AbilityPanelOffsetX, -20.0f), FVector2D(AbilityPanelWidth, 92.0f), FVector2D(0.5f, 1.0f));

	UBorder* HintPanel = MakePanel(PanelColor);
	HintPanel->SetPadding(FMargin(8.0f, 6.0f));
	UVerticalBox* HintBox = WidgetTree->ConstructWidget<UVerticalBox>();
	InteractHintText = MakeText(
		FString(FDSTRHUDViewModel::InteractKeyLabel) + TEXT(" INTERACT"), 12, MutedColor);
	HintBox->AddChildToVerticalBox(InteractHintText);
	HintPanel->SetContent(HintBox);
	AddPanelToCanvas(Root, HintPanel, FVector2D(0.5f, 1.0f), FVector2D(AbilityHintOffsetX, -20.0f),
		FVector2D(AbilityHintWidth, 62.0f), FVector2D(0.0f, 1.0f));

	ResultPanel = MakePanel(FLinearColor(0.015f, 0.025f, 0.045f, 0.94f));
	ResultText = MakeText(TEXT("DUNGEON CLEAR"), 32, FLinearColor(1.0f, 0.8f, 0.1f, 1.0f));
	ResultText->SetJustification(ETextJustify::Center);
	UVerticalBox* ResultBox = WidgetTree->ConstructWidget<UVerticalBox>();
	ResultBox->AddChildToVerticalBox(ResultText);
	MainMenuButton = WidgetTree->ConstructWidget<UButton>();
	MainMenuButton->SetBackgroundColor(FLinearColor(0.08f, 0.32f, 0.50f, 1.0f));
	UTextBlock* MainMenuLabel = MakeText(TEXT("MAIN MENU"), 13, FLinearColor::White);
	MainMenuLabel->SetJustification(ETextJustify::Center);
	MainMenuButton->AddChild(MainMenuLabel);
	MainMenuButton->OnClicked.AddDynamic(this, &UDSTRCombatHUDWidget::HandleMainMenuClicked);
	SummaryText = MakeText(TEXT(""), 12, MutedColor);
	SummaryText->SetJustification(ETextJustify::Center);
	ResultBox->AddChildToVerticalBox(SummaryText)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
	ResultBox->AddChildToVerticalBox(MainMenuButton)->SetPadding(FMargin(120.0f, 8.0f, 120.0f, 0.0f));
	ResultPanel->SetContent(ResultBox);
	AddPanelToCanvas(Root, ResultPanel, FVector2D(0.5f, 0.45f), FVector2D::ZeroVector, FVector2D(480.0f, 150.0f), FVector2D(0.5f, 0.5f));
	ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	ResultText->SetRenderOpacity(0.0f);

	StatusPanel = MakePanel(FLinearColor(0.015f, 0.025f, 0.045f, 0.8f));
	StatusText = MakeText(TEXT(""), 14, AccentColor);
	StatusText->SetJustification(ETextJustify::Center);
	StatusPanel->SetContent(StatusText);
	AddPanelToCanvas(Root, StatusPanel, FVector2D(0.5f, 1.0f), FVector2D(0.0f, -128.0f), FVector2D(560.0f, 36.0f), FVector2D(0.5f, 1.0f));
	StatusPanel->SetVisibility(ESlateVisibility::Collapsed);

	RevivePanel = MakePanel(FLinearColor(0.015f, 0.025f, 0.045f, 0.8f));
	UVerticalBox* ReviveBox = WidgetTree->ConstructWidget<UVerticalBox>();
	UTextBlock* ReviveLabel = MakeText(TEXT("REVIVING"), 11, ReadyColor);
	ReviveLabel->SetJustification(ETextJustify::Center);
	ReviveBar = WidgetTree->ConstructWidget<UProgressBar>();
	ReviveBar->SetFillColorAndOpacity(ReadyColor);
	ReviveBox->AddChildToVerticalBox(ReviveLabel);
	ReviveBox->AddChildToVerticalBox(ReviveBar);
	RevivePanel->SetContent(ReviveBox);
	AddPanelToCanvas(Root, RevivePanel, FVector2D(0.5f, 1.0f), FVector2D(0.0f, -176.0f), FVector2D(300.0f, 44.0f), FVector2D(0.5f, 1.0f));
	RevivePanel->SetVisibility(ESlateVisibility::Collapsed);

	EventPanel = MakePanel(FLinearColor(0.02f, 0.10f, 0.16f, 0.75f));
	UVerticalBox* EventBox = WidgetTree->ConstructWidget<UVerticalBox>();
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UTextBlock* Row = MakeText(TEXT(""), 10, FLinearColor::White);
		EventRows.Add(Row);
		EventBox->AddChildToVerticalBox(Row);
	}
	EventPanel->SetContent(EventBox);
	EventPanel->SetClipping(EWidgetClipping::ClipToBounds);
	AddPanelToCanvas(Root, EventPanel, FVector2D(1.0f, 0.0f), FVector2D(-20.0f, 64.0f), FVector2D(300.0f, 86.0f), FVector2D(1.0f, 0.0f));
	EventPanel->SetVisibility(ESlateVisibility::Collapsed);

	MinimapPanel = MakePanel(FLinearColor(0.02f, 0.10f, 0.16f, 0.85f));
	MinimapPanel->SetPadding(FMargin(0.0f));
	UCanvasPanel* MinimapCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	MinimapImage = WidgetTree->ConstructWidget<UImage>();
	UCanvasPanelSlot* MinimapImageSlot = MinimapCanvas->AddChildToCanvas(MinimapImage);
	MinimapImageSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	MinimapImageSlot->SetOffsets(FMargin(0.0f));
	for (int32 Index = 0; Index < MaxMinimapMarkers; ++Index)
	{
		UImage* Marker = WidgetTree->ConstructWidget<UImage>();
		Marker->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* MarkerSlot = MinimapCanvas->AddChildToCanvas(Marker);
		MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		MinimapMarkers.Add(Marker);
		MinimapMarkerSlots.Add(MarkerSlot);
	}
	MinimapPanel->SetContent(MinimapCanvas);
	MinimapPanel->SetClipping(EWidgetClipping::ClipToBounds);
	AddPanelToCanvas(Root, MinimapPanel, FVector2D(1.0f, 0.0f), FVector2D(-20.0f, 166.0f),
		FVector2D(FDSTRMinimapViewModel::PanelSize, FDSTRMinimapViewModel::PanelSize), FVector2D(1.0f, 0.0f));
	MinimapPanel->SetVisibility(ESlateVisibility::Collapsed);

	DirectionArrow = WidgetTree->ConstructWidget<UImage>();
	DirectionArrow->SetBrush(MakeTeardropBrush(DirectionArrowSize, ReadyColor));
	DirectionArrow->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	DirectionArrowSlot = Root->AddChildToCanvas(DirectionArrow);
	DirectionArrowSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	DirectionArrowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	DirectionArrowSlot->SetSize(FVector2D(DirectionArrowSize, DirectionArrowSize));
	DirectionArrowSlot->SetZOrder(6);
	DirectionArrow->SetVisibility(ESlateVisibility::Collapsed);

	DirectionDistanceText = MakeText(TEXT(""), 11, ReadyColor);
	DirectionDistanceText->SetJustification(ETextJustify::Center);
	DirectionDistanceSlot = Root->AddChildToCanvas(DirectionDistanceText);
	DirectionDistanceSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	DirectionDistanceSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	DirectionDistanceSlot->SetAutoSize(true);
	DirectionDistanceSlot->SetZOrder(6);
	DirectionDistanceText->SetVisibility(ESlateVisibility::Collapsed);

	HeadMarker = WidgetTree->ConstructWidget<UImage>();
	HeadMarker->SetBrush(MakeTeardropBrush(HeadMarkerSize, ReadyColor));
	HeadMarker->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	HeadMarker->SetRenderTransformAngle(TeardropTipOffsetDegrees + 180.0f);
	HeadMarkerSlot = Root->AddChildToCanvas(HeadMarker);
	HeadMarkerSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	HeadMarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	HeadMarkerSlot->SetSize(FVector2D(HeadMarkerSize, HeadMarkerSize));
	HeadMarkerSlot->SetZOrder(6);
	HeadMarker->SetVisibility(ESlateVisibility::Collapsed);
}

void UDSTRCombatHUDWidget::SetMinimapSource(
	UTextureRenderTarget2D* InRenderTarget,
	const FDSTRMinimapFrame& InFrame)
{
	MinimapRenderTarget = InRenderTarget;
	MinimapFrame = InFrame;
	bMinimapBrushApplied = false;
}

void UDSTRCombatHUDWidget::PlaceMinimapMarker(
	int32& MarkerIndex,
	const FVector& World,
	const float Size,
	const FLinearColor& Color,
	const int32 ZOrder,
	const FVector2D& PanelOffset,
	const float Opacity)
{
	if (!MinimapMarkers.IsValidIndex(MarkerIndex) || !MinimapMarkerSlots.IsValidIndex(MarkerIndex))
	{
		return;
	}

	bool bClamped = false;
	const FVector2D Panel = FDSTRMinimapViewModel::WorldToPanel(MinimapFrame, World, bClamped) + PanelOffset;
	const float Inset = FDSTRMinimapViewModel::MarkerInset;

	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.ImageSize = FVector2D(Size, Size);
	Brush.TintColor = FSlateColor(Color);
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;

	UImage* Marker = MinimapMarkers[MarkerIndex];
	Marker->SetBrush(Brush);
	Marker->SetRenderOpacity(Opacity);
	Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
	MinimapMarkerSlots[MarkerIndex]->SetSize(FVector2D(Size, Size));
	MinimapMarkerSlots[MarkerIndex]->SetPosition(FVector2D(
		FMath::Clamp(Panel.X, Inset, MinimapFrame.PanelSize - Inset),
		FMath::Clamp(Panel.Y, Inset, MinimapFrame.PanelSize - Inset)));
	MinimapMarkerSlots[MarkerIndex]->SetZOrder(ZOrder);
	++MarkerIndex;
}

void UDSTRCombatHUDWidget::RefreshMinimap(const ADSTRGameState* State, APlayerController* PC)
{
	UWorld* World = GetWorld();
	const EDSTRMatchPhase Phase = State ? State->GetMatchPhase() : EDSTRMatchPhase::WaitingForPlayers;
	const bool bShow = MinimapPanel && MinimapRenderTarget && World && PC
		&& Phase != EDSTRMatchPhase::WaitingForPlayers
		&& Phase != EDSTRMatchPhase::Clear
		&& Phase != EDSTRMatchPhase::Failed;
	if (MinimapPanel)
	{
		MinimapPanel->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!bShow)
	{
		return;
	}

	if (!bMinimapBrushApplied && MinimapImage)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(MinimapRenderTarget);
		Brush.ImageSize = FVector2D(FDSTRMinimapViewModel::PanelSize, FDSTRMinimapViewModel::PanelSize);
		MinimapImage->SetBrush(Brush);
		bMinimapBrushApplied = true;
	}

	int32 MarkerIndex = 0;
	const APawn* LocalPawn = PC->GetPawn();
	if (LocalPawn)
	{
		const float Heading = FDSTRMinimapViewModel::HeadingToArrowAngle(
			static_cast<float>(PC->GetControlRotation().Yaw));
		const FVector2D Nose(
			FMath::Sin(FMath::DegreesToRadians(Heading)) * MinimapNoseDistance,
			-FMath::Cos(FMath::DegreesToRadians(Heading)) * MinimapNoseDistance);
		PlaceMinimapMarker(MarkerIndex, LocalPawn->GetActorLocation(), 12.0f, AccentColor, 4);
		PlaceMinimapMarker(MarkerIndex, LocalPawn->GetActorLocation(), 6.0f, AccentColor, 5, Nose);
	}

	const AActor* Objective = ObjectiveTargetActor.Get();
	const float PulseAlpha = FDSTRMinimapViewModel::PulseOpacity(bMinimapPulseOn);

	const bool bShowRoute = Phase == EDSTRMatchPhase::Advance || Phase == EDSTRMatchPhase::Boss;
	if (bShowRoute && !State->GetBossRoom().IsNearlyZero())
	{
		PlaceMinimapMarker(MarkerIndex, State->GetBossRoom(), 12.0f, BossRoomMarkerColor, 2,
			FVector2D::ZeroVector, Phase == EDSTRMatchPhase::Advance ? PulseAlpha : 1.0f);
	}

	if (bShowRoute && BossGate)
	{
		const EDSTRGateState GateState = BossGate->GetGateState();
		const FLinearColor GateColor = GateState == EDSTRGateState::Locked
			? GateLockedMarkerColor
			: (GateState == EDSTRGateState::Open ? GateOpenMarkerColor : GateSealedMarkerColor);
		const FVector Across = FVector::CrossProduct(FVector::UpVector, BossGate->GetGateForward()).GetSafeNormal2D();
		for (int32 Step = -1; Step <= 1; ++Step)
		{
			PlaceMinimapMarker(
				MarkerIndex,
				BossGate->GetActorLocation() + Across * (GateSegmentSpacing * Step),
				6.0f,
				GateColor,
				3,
				FVector2D::ZeroVector,
				GateState == EDSTRGateState::Sealed ? PulseAlpha : 1.0f);
		}
	}

	float WaveWarningAge = TNumericLimits<float>::Max();
	for (const FDSTRMatchEvent& Event : State->GetRecentEvents())
	{
		if (Event.Kind == EDSTRMatchEventKind::WaveIncoming)
		{
			WaveWarningAge = State->GetServerWorldTimeSeconds() - Event.ServerTime;
		}
	}
	if (State->GetPendingSpawnCount() > 0 || WaveWarningAge < FDSTRHUDViewModel::EventFadeEndSeconds)
	{
		PlaceMinimapMarker(MarkerIndex, State->GetWaveSpawnGate(), 9.0f, DoorMarkerColor, 1,
			FVector2D::ZeroVector, bObjectiveIsGate ? PulseAlpha : 1.0f);
	}

	for (const ADediServerRPGCharacter* Member : PartyCharacters)
	{
		if (Member != LocalPawn)
		{
			PlaceMinimapMarker(MarkerIndex, Member->GetActorLocation(), 8.0f, AllyMarkerColor, 3,
				FVector2D::ZeroVector, Member == Objective ? PulseAlpha : 1.0f);
		}
	}
	for (const ADSTREnemyCharacter* Enemy : LivingEnemies)
	{
		PlaceMinimapMarker(MarkerIndex, Enemy->GetActorLocation(), Enemy->IsBoss() ? 12.0f : 8.0f, EnemyMarkerColor, 2,
			FVector2D::ZeroVector, Enemy == Objective ? PulseAlpha : 1.0f);
	}
	for (const ADSTRAttackBuffPickup* Pickup : ActivePickups)
	{
		PlaceMinimapMarker(MarkerIndex, Pickup->GetActorLocation(), 8.0f, BuffMarkerColor, 1,
			FVector2D::ZeroVector, Pickup == Objective ? PulseAlpha : 1.0f);
	}
	if (bShowRoute)
	{
		for (const FVector& Point : State->GetAdvancePath())
		{
			PlaceMinimapMarker(MarkerIndex, Point, 4.0f, RouteMarkerColor, 0, FVector2D::ZeroVector, 0.45f);
		}
	}
	for (int32 Index = MarkerIndex; Index < MinimapMarkers.Num(); ++Index)
	{
		MinimapMarkers[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UDSTRCombatHUDWidget::CollectSceneActors()
{
	LivingEnemies.Reset();
	PartyCharacters.Reset();
	ActivePickups.Reset();
	AdvanceHostiles.Reset();
	BossGate = nullptr;
	const UWorld* World = GetWorld();
	const UDSTRActorRegistry* Registry = UDSTRActorRegistry::Get(World);
	if (!World || !Registry)
	{
		return;
	}
	for (const TWeakObjectPtr<ADSTREnemyCharacter>& Handle : Registry->GetEnemies())
	{
		ADSTREnemyCharacter* Enemy = Handle.Get();
		if (!Enemy || Enemy->IsCombatantDead())
		{
			continue;
		}
		if (!Enemy->IsDormant())
		{
			LivingEnemies.Add(Enemy);
		}
		if (!Enemy->IsBoss())
		{
			AdvanceHostiles.Add(Enemy);
		}
	}
	if (const ADSTRGameState* State = World->GetGameState<ADSTRGameState>())
	{
		BossGate = State->GetBossGateActor();
	}
	for (const TWeakObjectPtr<ADediServerRPGCharacter>& Handle : Registry->GetHeroes())
	{
		if (ADediServerRPGCharacter* Member = Handle.Get())
		{
			PartyCharacters.Add(Member);
		}
	}
	for (const TWeakObjectPtr<ADSTRAttackBuffPickup>& Handle : Registry->GetPickups())
	{
		ADSTRAttackBuffPickup* Pickup = Handle.Get();
		if (Pickup && !Pickup->IsConsumed())
		{
			ActivePickups.Add(Pickup);
		}
	}
}

void UDSTRCombatHUDWidget::RefreshObjective(
	const ADSTRGameState* State,
	const ADediServerRPGCharacter* LocalCharacter,
	const bool bTeammateDowned,
	const FString& DownedTeammateName,
	const bool bBossPanelVisible)
{
	UWorld* World = GetWorld();
	if (!ObjectivePanel || !ObjectiveText || !World)
	{
		return;
	}

	const ADSTREnemyCharacter* Boss = State ? State->GetBoss() : nullptr;
	const UDSTRAttributeSet* BossAttributes = Boss ? Boss->GetAttributeSet() : nullptr;
	const FVector From = LocalCharacter ? LocalCharacter->GetActorLocation() : FVector::ZeroVector;
	ADSTRAttackBuffPickup* NearbyBuff = LocalCharacter
		? FindNearestBuff(ActivePickups, From, FDSTRHUDViewModel::ObjectiveBuffRange) : nullptr;
	const ADediServerRPGCharacter* DownedTeammate = LocalCharacter && bTeammateDowned
		? FindNearestDownedTeammate(PartyCharacters, LocalCharacter) : nullptr;

	FDSTRObjectiveInput Input;
	Input.Phase = State ? State->GetMatchPhase() : EDSTRMatchPhase::WaitingForPlayers;
	Input.bSelfDowned = LocalCharacter && LocalCharacter->IsDowned();
	Input.bDownedTeammate = bTeammateDowned;
	Input.DownedTeammateName = DownedTeammateName;
	Input.bReviveInReach = DownedTeammate
		&& FVector::Dist(From, DownedTeammate->GetActorLocation()) <= FDSTRHUDViewModel::ObjectiveReviveRange;
	Input.ReviveSecondsLeft = DownedTeammate ? DownedTeammate->GetBleedOutRemaining() : 0.0f;
	Input.bBuffNearby = NearbyBuff != nullptr;
	Input.bBossAlive = Boss && !Boss->IsCombatantDead() && BossAttributes != nullptr;
	Input.BossName = TEXT("SEVAROG");
	Input.BossHealth = BossAttributes ? BossAttributes->GetHealth() : 0.0f;
	Input.BossMaxHealth = BossAttributes ? BossAttributes->GetMaxHealth() : 0.0f;
	Input.RemainingEnemies = State ? State->GetRemainingEnemies() : 0;
	Input.LivingEnemyCount = LivingEnemies.Num();
	Input.PendingSpawnCount = State ? State->GetPendingSpawnCount() : 0;
	Input.GateState = BossGate ? BossGate->GetGateState() : EDSTRGateState::Sealed;
	Input.AmbushRemaining = State ? State->GetAmbushRemaining() : 0;

	const EDSTRObjectiveKind Kind = FDSTRHUDViewModel::GetObjectiveKind(Input);
	const FString Line = FDSTRHUDViewModel::FormatObjective(Input);
	ObjectiveText->SetText(FText::FromString(Line));
	ObjectiveText->SetColorAndOpacity(FSlateColor(Kind == EDSTRObjectiveKind::Wave ? MutedColor : ReadyColor));
	ObjectivePanel->SetVisibility(Line.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (ObjectivePanelSlot)
	{
		ObjectivePanelSlot->SetPosition(FVector2D(0.0f, bBossPanelVisible ? 140.0f : 88.0f));
	}

	bObjectiveIsGate = IsGateKind(Kind);
	ObjectiveTargetActor = ResolveObjectiveTarget(Kind, State, LocalCharacter, NearbyBuff);
	if (const AActor* Target = ObjectiveTargetActor.Get())
	{
		ObjectiveTargetLocation = Target->GetActorLocation();
		ObjectiveTargetName = Target->GetName();
	}
	else if (Kind == EDSTRObjectiveKind::ClearPath && State)
	{
		ObjectiveTargetLocation = FDSTRDungeonRules::NextWaypoint(
			State->GetAdvancePath(), From, State->GetBossRoom());
		ObjectiveTargetName = TEXT("BOSS CHAMBER");
	}
	else if (Kind == EDSTRObjectiveKind::EnterChamber && State)
	{
		ObjectiveTargetLocation = State->GetBossGate();
		ObjectiveTargetName = TEXT("BOSS GATE");
	}
	else if (bObjectiveIsGate && State)
	{
		ObjectiveTargetLocation = State->GetWaveSpawnGate();
		ObjectiveTargetName = TEXT("GATE");
	}
	else
	{
		ObjectiveTargetName.Reset();
	}
}

AActor* UDSTRCombatHUDWidget::ResolveObjectiveTarget(
	const EDSTRObjectiveKind Kind,
	const ADSTRGameState* State,
	const AActor* LocalPawn,
	ADSTRAttackBuffPickup* NearbyBuff)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	const FVector From = LocalPawn ? LocalPawn->GetActorLocation() : FVector::ZeroVector;
	switch (Kind)
	{
	case EDSTRObjectiveKind::Revive:
		return FindNearestDownedTeammate(PartyCharacters, LocalPawn);
	case EDSTRObjectiveKind::Buff:
		return NearbyBuff;
	case EDSTRObjectiveKind::Boss:
		return State ? State->GetBoss() : nullptr;
	case EDSTRObjectiveKind::ClearPath:
		return FindNearestMinion(AdvanceHostiles, From);
	case EDSTRObjectiveKind::Wave:
	{
		ADSTREnemyCharacter* Minion = PickWaveMarkerTarget(From);
		return Minion ? Cast<AActor>(Minion) : (State ? Cast<AActor>(State->GetBoss()) : nullptr);
	}
	default:
		return nullptr;
	}
}

ADSTREnemyCharacter* UDSTRCombatHUDWidget::PickWaveMarkerTarget(const FVector& From)
{
	ADSTREnemyCharacter* Current = WaveMarkerTarget.Get();
	if (Current && (Current->IsCombatantDead() || !LivingEnemies.Contains(Current)))
	{
		Current = nullptr;
	}
	ADSTREnemyCharacter* Nearest = FindNearestMinion(LivingEnemies, From);
	if (Current && Nearest && Nearest != Current
		&& !FDSTRHUDViewModel::ShouldRetarget(
			static_cast<float>(FVector::Dist(From, Current->GetActorLocation())),
			static_cast<float>(FVector::Dist(From, Nearest->GetActorLocation()))))
	{
		Nearest = Current;
	}
	WaveMarkerTarget = Nearest;
	return Nearest;
}

void UDSTRCombatHUDWidget::RefreshDirectionMarker(APlayerController* PC)
{
	const AActor* Target = ObjectiveTargetActor.Get();
	const APawn* LocalPawn = PC ? PC->GetPawn() : nullptr;
	const bool bUsable = !ObjectiveTargetName.IsEmpty() && LocalPawn && PC->IsLocalController()
		&& DirectionArrow && DirectionDistanceText && HeadMarker;
	if (!bUsable)
	{
		if (DirectionArrow)
		{
			DirectionArrow->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (DirectionDistanceText)
		{
			DirectionDistanceText->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (HeadMarker)
		{
			HeadMarker->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const float MarkerRise = Target ? Target->GetSimpleCollisionHalfHeight() : GateMarkerHeight;
	const FVector HeadLocation = ObjectiveTargetLocation + FVector(0.0f, 0.0f, MarkerRise + 25.0f);
	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
	const bool bBehind = FVector::DotProduct(CameraRotation.Vector(), HeadLocation - CameraLocation) <= 0.0f;

	FVector2D ScreenPos = FVector2D::ZeroVector;
	const bool bProjected = PC->ProjectWorldLocationToScreen(HeadLocation, ScreenPos, false);

	const FVector2D DesignSize(DesignWidth, DesignHeight);
	FVector2D ViewportSize = DesignSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	const FVector2D Design = FDSTRHUDViewModel::ViewportToDesign(ViewportSize, DesignSize, ScreenPos);

	bool bOffScreen = true;
	const FVector2D Edge = FDSTRHUDViewModel::ProjectToScreenEdge(
		DesignSize, Design, bBehind || !bProjected, ScreenEdgeInset, bOffScreen);
	const float TargetDistance = static_cast<float>(
		FVector::Dist(LocalPawn->GetActorLocation(), ObjectiveTargetLocation));
	bOffScreen = bOffScreen && !FDSTRHUDViewModel::ShouldPinAtTarget(TargetDistance);

	DirectionArrow->SetVisibility(bOffScreen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	DirectionDistanceText->SetVisibility(bOffScreen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	HeadMarker->SetVisibility(bOffScreen ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	if (bOffScreen)
	{
		const FVector2D Safe(
			FMath::Clamp(Edge.X, static_cast<double>(DirectionMarkerInset), DesignWidth - DirectionMarkerInset),
			FMath::Clamp(Edge.Y, static_cast<double>(DirectionMarkerInset), DesignHeight - DirectionMarkerInset));
		const float Angle = FDSTRHUDViewModel::ScreenEdgeAngle(DesignSize, Safe);
		DirectionArrow->SetRenderTransformAngle(Angle + TeardropTipOffsetDegrees);
		DirectionArrowSlot->SetPosition(Safe);
		const FVector2D Inward = (DesignSize * 0.5 - Safe).GetSafeNormal() * 30.0;
		DirectionDistanceSlot->SetPosition(Safe + Inward);
		DirectionDistanceText->SetText(FText::FromString(FDSTRHUDViewModel::FormatDistanceMeters(TargetDistance)));
	}
	else
	{
		HeadMarkerSlot->SetPosition(FVector2D(
			FMath::Clamp(Design.X, 20.0, DesignWidth - 20.0),
			FMath::Clamp(Design.Y, static_cast<double>(HeadMarkerTopLimit), DesignHeight - 40.0)));
	}

	if (bOffScreen != bMarkerOffScreen || ObjectiveTargetName != LastMarkerTargetName)
	{
		bMarkerOffScreen = bOffScreen;
		LastMarkerTargetName = ObjectiveTargetName;
		UE_LOG(LogDSTR, Log, TEXT("DSTR_OBJECTIVE_MARKER Target=%s OffScreen=%d Behind=%d Dist=%.0f"),
			*ObjectiveTargetName,
			bOffScreen ? 1 : 0,
			bBehind ? 1 : 0,
			TargetDistance);
	}
}

void UDSTRCombatHUDWidget::RefreshInteractLabel(const ADediServerRPGCharacter* LocalCharacter)
{
	if (!InteractHintText)
	{
		return;
	}
	const FVector From = LocalCharacter ? LocalCharacter->GetActorLocation() : FVector::ZeroVector;
	const float ReachSquared = FMath::Square(ADediServerRPGCharacter::InteractRange);
	bool bRevive = false;
	for (const ADediServerRPGCharacter* Member : PartyCharacters)
	{
		bRevive = bRevive || (Member->CanBeRevivedBy(LocalCharacter)
			&& FVector::DistSquared(From, Member->GetActorLocation()) <= ReachSquared);
	}
	bool bPickup = false;
	for (const ADSTRAttackBuffPickup* Pickup : ActivePickups)
	{
		bPickup = bPickup || (!bRevive && LocalCharacter && !LocalCharacter->IsDowned()
			&& FVector::DistSquared(From, Pickup->GetActorLocation()) <= ReachSquared);
	}
	InteractHintText->SetText(FText::FromString(
		FString(FDSTRHUDViewModel::InteractKeyLabel) + TEXT(" ")
			+ FDSTRHUDViewModel::FormatInteractLabel(bRevive, bPickup)));
	InteractHintText->SetColorAndOpacity(FSlateColor(bRevive || bPickup ? AccentColor : MutedColor));
}

void UDSTRCombatHUDWidget::RefreshAbilityCard(
	const int32 Index,
	const UAbilitySystemComponent* ASC,
	const FGameplayTag& CooldownTag)
{
	if (!AbilityCooldownTexts.IsValidIndex(Index) || !AbilityCooldownBars.IsValidIndex(Index))
	{
		return;
	}

	float Remaining = 0.0f;
	float Duration = 0.0f;
	if (ASC && CooldownTag.IsValid())
	{
		FGameplayTagContainer Tags(CooldownTag);
		const TArray<TPair<float, float>> Times = ASC->GetActiveEffectsTimeRemainingAndDuration(
			FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Tags));
		if (!Times.IsEmpty())
		{
			Remaining = Times[0].Key;
			Duration = Times[0].Value;
		}
	}

	const bool bReady = Remaining <= KINDA_SMALL_NUMBER;
	AbilityCooldownTexts[Index]->SetText(FText::FromString(FDSTRHUDViewModel::FormatCooldown(Remaining)));
	AbilityCooldownTexts[Index]->SetColorAndOpacity(FSlateColor(bReady ? ReadyColor : FLinearColor(1.0f, 0.62f, 0.12f, 1.0f)));
	AbilityCooldownBars[Index]->SetPercent(bReady ? 1.0f : 1.0f - FDSTRHUDViewModel::SafeRatio(Remaining, Duration));
}

void UDSTRCombatHUDWidget::RefreshHUD()
{
	APlayerController* PC = GetOwningPlayer();
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return;
	}

	const ADSTRGameState* State = World->GetGameState<ADSTRGameState>();
	const ADSTRPlayerState* LocalState = PC->GetPlayerState<ADSTRPlayerState>();
	const UDSTRAttributeSet* Attributes = LocalState ? LocalState->GetAttributeSet() : nullptr;
	const UAbilitySystemComponent* ASC = LocalState ? LocalState->GetAbilitySystemComponent() : nullptr;

	if (State)
	{
		PhaseText->SetText(FText::FromString(State->GetPhaseDisplayName()));
		RunInfoText->SetText(FText::FromString(FString::Printf(
			TEXT("RUN %s   |   HOSTILES %d"),
			*FDSTRHUDViewModel::FormatClock(State->GetElapsedMatchSeconds()),
			State->GetRemainingEnemies())));
	}

	NetworkText->SetText(FText::FromString(FDSTRHUDViewModel::FormatNetworkRole(
		World->GetNetMode(), State ? State->PlayerArray.Num() : 1)));

	if (Attributes)
	{
		HealthText->SetText(FText::FromString(FString::Printf(
			TEXT("HP  %.0f / %.0f"), Attributes->GetHealth(), Attributes->GetMaxHealth())));
		HealthBar->SetPercent(FDSTRHUDViewModel::SafeRatio(Attributes->GetHealth(), Attributes->GetMaxHealth()));
		StaminaText->SetText(FText::FromString(FString::Printf(
			TEXT("STAMINA  %.0f / %.0f"), Attributes->GetStamina(), Attributes->GetMaxStamina())));
		StaminaBar->SetPercent(FDSTRHUDViewModel::SafeRatio(Attributes->GetStamina(), Attributes->GetMaxStamina()));
	}

	for (int32 Index = 0; Index < PartyRows.Num(); ++Index)
	{
		PartyRows[Index]->SetText(FText::FromString(TEXT("-- EMPTY SLOT --")));
		PartyRows[Index]->SetColorAndOpacity(FSlateColor(MutedColor));
	}
	bool bTeammateDowned = false;
	FString DownedTeammateName;
	int32 AliveCount = 0;
	if (State)
	{
		int32 RowIndex = 0;
		for (APlayerState* PlayerState : State->PlayerArray)
		{
			if (!PartyRows.IsValidIndex(RowIndex))
			{
				break;
			}
			const ADSTRPlayerState* DSTRState = Cast<ADSTRPlayerState>(PlayerState);
			const UDSTRAttributeSet* TeamAttributes = DSTRState ? DSTRState->GetAttributeSet() : nullptr;
			const ADediServerRPGCharacter* Character = DSTRState
				? Cast<ADediServerRPGCharacter>(DSTRState->GetPawn()) : nullptr;
			const bool bDowned = Character && Character->IsDowned();
			const bool bRescuable = bDowned && !Character->IsEliminated() && PlayerState != LocalState;
			if (bRescuable && DownedTeammateName.IsEmpty())
			{
				DownedTeammateName = PlayerState->GetPlayerName();
			}
			bTeammateDowned |= bRescuable;
			AliveCount += bDowned ? 0 : 1;
			PartyRows[RowIndex]->SetText(FText::FromString(FDSTRHUDViewModel::FormatPartyRow(
				RowIndex + 1,
				PlayerState->GetPlayerName(),
				TeamAttributes ? TeamAttributes->GetHealth() : 0.0f,
				bDowned,
				Character && Character->IsEliminated())));
			PartyRows[RowIndex]->SetColorAndOpacity(FSlateColor(bDowned ? DangerColor : FLinearColor::White));
			++RowIndex;
		}
	}

	const ADSTREnemyCharacter* Boss = State ? State->GetBoss() : nullptr;
	const UDSTRAttributeSet* BossAttributes = Boss ? Boss->GetAttributeSet() : nullptr;
	const bool bShowBoss = Boss && !Boss->IsCombatantDead() && BossAttributes;
	BossPanel->SetVisibility(bShowBoss ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShowBoss)
	{
		BossText->SetText(FText::FromString(FString::Printf(
			TEXT("SEVAROG   %.0f / %.0f"),
			BossAttributes->GetHealth(), BossAttributes->GetMaxHealth())));
		BossBar->SetPercent(FDSTRHUDViewModel::SafeRatio(BossAttributes->GetHealth(), BossAttributes->GetMaxHealth()));
	}

	RefreshAbilityCard(0, ASC, DSTRGameplayTags::Effect_Cooldown_Attack.GetTag());
	RefreshAbilityCard(1, ASC, DSTRGameplayTags::Effect_Cooldown_MakeWay.GetTag());
	RefreshAbilityCard(2, ASC, DSTRGameplayTags::Effect_Cooldown_Fortify.GetTag());
	RefreshAbilityCard(3, ASC, DSTRGameplayTags::Effect_Cooldown_Charge.GetTag());
	RefreshAbilityCard(4, ASC, DSTRGameplayTags::Effect_Cooldown_Reckoning.GetTag());

	const ADediServerRPGCharacter* LocalCharacter = LocalState
		? Cast<ADediServerRPGCharacter>(LocalState->GetPawn()) : nullptr;
	CollectSceneActors();
	RefreshObjective(State, LocalCharacter, bTeammateDowned, DownedTeammateName, bShowBoss);
	RefreshDirectionMarker(PC);
	RefreshInteractLabel(LocalCharacter);

	if (State)
	{
		FDSTRStatusLineInput StatusInput;
		StatusInput.Phase = State->GetMatchPhase();
		StatusInput.CountdownRemaining = State->GetCountdownRemaining();
		StatusInput.bIsHost = State->IsHost(LocalState);
		StatusInput.bSelfDowned = LocalCharacter && LocalCharacter->IsDowned();
		StatusInput.bTeammateDowned = bTeammateDowned;
		StatusInput.bBuffActive = ASC && ASC->HasMatchingGameplayTag(DSTRGameplayTags::Effect_Buff_Attack.GetTag());
		StatusInput.SecondsSinceBossSpawn = State->GetSecondsSinceBossSpawn();
		StatusInput.BleedOutRemaining = LocalCharacter ? LocalCharacter->GetBleedOutRemaining() : 0.0f;
		StatusInput.bSelfEliminated = LocalCharacter && LocalCharacter->IsEliminated();
		const FString Status = FDSTRHUDViewModel::FormatStatusLine(StatusInput);
		StatusText->SetText(FText::FromString(Status));
		StatusPanel->SetVisibility(Status.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

		const bool bReviving = LocalCharacter && LocalCharacter->GetCurrentCombatAction() == EDSTRCombatAction::Revive;
		RevivePanel->SetVisibility(bReviving ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bReviving)
		{
			ReviveBar->SetPercent(LocalCharacter->GetCombatActionProgress());
		}

		const TArray<FDSTRMatchEvent>& Events = State->GetRecentEvents();
		const int32 Shown = FMath::Min(Events.Num(), EventRows.Num());
		for (int32 Row = 0; Row < EventRows.Num(); ++Row)
		{
			const int32 EventIndex = Events.Num() - Shown + Row;
			if (Row < Shown)
			{
				const FDSTRMatchEvent& Event = Events[EventIndex];
				EventRows[Row]->SetText(FText::FromString(FDSTRHUDViewModel::FormatEvent(Event.Kind, Event.Subject, Event.Object)));
				EventRows[Row]->SetRenderOpacity(FDSTRHUDViewModel::EventOpacity(State->GetServerWorldTimeSeconds() - Event.ServerTime));
				EventRows[Row]->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				EventRows[Row]->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		EventPanel->SetVisibility(Shown > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (++PulseTicks >= 3)
	{
		PulseTicks = 0;
		bMinimapPulseOn = !bMinimapPulseOn;
	}
	RefreshMinimap(State, PC);

	const bool bClear = State && State->GetMatchPhase() == EDSTRMatchPhase::Clear;
	const bool bFailed = State && State->GetMatchPhase() == EDSTRMatchPhase::Failed;
	if (bClear || bFailed)
	{
		ResultPanel->SetVisibility(ESlateVisibility::Visible);
		ResultText->SetRenderOpacity(1.0f);
		ResultText->SetText(FText::FromString(bClear ? TEXT("DUNGEON CLEAR") : TEXT("PARTY DEFEATED")));
		SummaryText->SetText(FText::FromString(FDSTRHUDViewModel::FormatResultSummary(
			State->GetElapsedMatchSeconds(), State->GetDownCount(), State->GetReviveCount(),
			AliveCount, State->PlayerArray.Num())));
		ResultText->SetColorAndOpacity(FSlateColor(bClear
			? FLinearColor(1.0f, 0.8f, 0.1f, 1.0f)
			: DangerColor));
		if (!bResultShown)
		{
			bResultShown = true;
			if (ADSTRPlayerController* DSTRPC = Cast<ADSTRPlayerController>(PC))
			{
				DSTRPC->SetLobbyInputMode();
			}
		}
	}
}

void UDSTRCombatHUDWidget::HandleMainMenuClicked()
{
	UE_LOG(LogDSTR, Log, TEXT("DSTR_UI_CLICK Button=ResultMainMenu"));
	if (ADSTRPlayerController* PC = Cast<ADSTRPlayerController>(GetOwningPlayer()))
	{
		PC->ReturnToMainMenu();
	}
}
