#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/DSTRHUDViewModel.h"
#include "UI/DSTRMinimapViewModel.h"
#include "DSTRCombatHUDWidget.generated.h"

class ADediServerRPGCharacter;
class ADSTRAttackBuffPickup;
class ADSTRBossGate;
class ADSTREnemyCharacter;
class ADSTRGameState;
class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UHorizontalBox;
class UImage;
class UProgressBar;
class UTextBlock;
class UTextureRenderTarget2D;
class UVerticalBox;
class UAbilitySystemComponent;
struct FGameplayTag;

UCLASS()
class DEDISERVERRPG_API UDSTRCombatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMinimapSource(UTextureRenderTarget2D* InRenderTarget, const FDSTRMinimapFrame& InFrame);

	static constexpr int32 AbilityCardCount = 5;
	static constexpr float AbilityPanelWidth = 550.0f;
	static constexpr float AbilityHintWidth = 150.0f;
	static constexpr float AbilityPanelOffsetX = 20.0f;
	static constexpr float AbilityHintOffsetX = 315.0f;
	static constexpr float StatsPanelRight = 300.0f;
	static constexpr float DesignWidth = 1280.0f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();
	void RefreshHUD();
	void RefreshAbilityCard(int32 Index, const UAbilitySystemComponent* ASC, const FGameplayTag& CooldownTag);
	UTextBlock* MakeText(const FString& Text, int32 Size, const FLinearColor& Color);
	UBorder* MakePanel(const FLinearColor& Color);
	void AddPanelToCanvas(
		UCanvasPanel* Canvas,
		UBorder* Panel,
		const FVector2D& Anchor,
		const FVector2D& Position,
		const FVector2D& Size,
		const FVector2D& Alignment = FVector2D::ZeroVector);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PhaseText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RunInfoText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NetworkText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StaminaText;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> StaminaBar;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossText;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> BossBar;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> BossPanel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> ResultPanel;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PartyRows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> AbilityCooldownTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> AbilityNameTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> AbilityCooldownBars;

	UFUNCTION()
	void HandleMainMenuClicked();
	UPROPERTY(Transient)
	TObjectPtr<class UButton> MainMenuButton;
	bool bResultShown = false;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> StatusPanel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> RevivePanel;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ReviveBar;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryText;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> EventPanel;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> EventRows;

	void RefreshObjective(const ADSTRGameState* State, const ADediServerRPGCharacter* LocalCharacter,
		bool bTeammateDowned, const FString& DownedTeammateName, bool bBossPanelVisible);
	AActor* ResolveObjectiveTarget(
		EDSTRObjectiveKind Kind, const ADSTRGameState* State, const AActor* LocalPawn, ADSTRAttackBuffPickup* NearbyBuff);
	static bool IsGateKind(EDSTRObjectiveKind Kind)
	{
		return Kind == EDSTRObjectiveKind::Gate || Kind == EDSTRObjectiveKind::BossIncoming;
	}
	ADSTREnemyCharacter* PickWaveMarkerTarget(const FVector& From);
	void RefreshDirectionMarker(APlayerController* PC);
	void RefreshInteractLabel(const ADediServerRPGCharacter* LocalCharacter);

	void RefreshMinimap(const ADSTRGameState* State, APlayerController* PC);
	void PlaceMinimapMarker(
		int32& MarkerIndex,
		const FVector& World,
		float Size,
		const FLinearColor& Color,
		int32 ZOrder,
		const FVector2D& PanelOffset = FVector2D::ZeroVector,
		float Opacity = 1.0f);
	void CollectSceneActors();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ObjectivePanel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ObjectiveText;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> ObjectivePanelSlot;
	UPROPERTY(Transient)
	TObjectPtr<UImage> DirectionArrow;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> DirectionArrowSlot;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DirectionDistanceText;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> DirectionDistanceSlot;
	UPROPERTY(Transient)
	TObjectPtr<UImage> HeadMarker;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> HeadMarkerSlot;
	TWeakObjectPtr<AActor> ObjectiveTargetActor;
	TWeakObjectPtr<ADSTREnemyCharacter> WaveMarkerTarget;
	FVector ObjectiveTargetLocation = FVector::ZeroVector;
	FString ObjectiveTargetName;
	FString LastMarkerTargetName;
	bool bObjectiveIsGate = false;
	bool bMarkerOffScreen = false;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADSTREnemyCharacter>> LivingEnemies;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADediServerRPGCharacter>> PartyCharacters;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADSTRAttackBuffPickup>> ActivePickups;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADSTREnemyCharacter>> AdvanceHostiles;
	UPROPERTY(Transient)
	TObjectPtr<ADSTRBossGate> BossGate;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InteractHintText;

	static constexpr float DesignHeight = 720.0f;
	static constexpr float ScreenEdgeInset = 40.0f;
	static constexpr float DirectionMarkerInset = 230.0f;
	static constexpr float DirectionArrowSize = 24.0f;
	static constexpr float HeadMarkerSize = 14.0f;
	static constexpr float HeadMarkerTopLimit = 180.0f;
	static constexpr float TeardropTipOffsetDegrees = 45.0f;
	static constexpr float GateMarkerHeight = 100.0f;

	static constexpr int32 MaxMinimapMarkers = 48;
	static constexpr float MinimapNoseDistance = 9.0f;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> MinimapPanel;
	UPROPERTY(Transient)
	TObjectPtr<UImage> MinimapImage;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> MinimapMarkers;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCanvasPanelSlot>> MinimapMarkerSlots;
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> MinimapRenderTarget;
	FDSTRMinimapFrame MinimapFrame;
	bool bMinimapBrushApplied = false;
	bool bMinimapPulseOn = false;
	int32 PulseTicks = 0;

	float RefreshAccumulator = 0.0f;
};
