#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/DSTRServerStatus.h"
#include "DSTRMainMenuWidget.generated.h"

class UBorder;
class UButton;
class UEditableTextBox;
class UTextBlock;
class UDSTRServerStatusQuery;

UCLASS()
class DEDISERVERRPG_API UDSTRMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static bool CanHostLocalServer();

	static constexpr float LocalServerFirstJoinSeconds = 3.0f;
	static constexpr float LocalServerRetrySeconds = 2.0f;
	static constexpr int32 LocalServerMaxRetries = 3;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();
	UTextBlock* MakeText(const FString& Text, int32 Size, const FLinearColor& Color);
	UButton* MakeButton(const FString& Label);
	void SetStatus(const FString& Text, const FLinearColor& Color);
	void TravelToAddress(const FString& AddressText, const FString& RoomOptions);
	void CaptureMenuEvidence();
	void UpdateRoomList();
	void StartStatusQuery(int32 ServerIndex);
	void CancelStatusQueries();
	bool IsSlotFresh(int32 ServerIndex) const;
	int32 FindJoinableRoom(const FString& RequestedName = FString()) const;
	int32 FindCreatableSlot() const;
	void JoinRoomByIndex(int32 ServerIndex);
	void RunDevelopmentSmokeCommands();

	UFUNCTION()
	void RefreshServerStatus();
	UFUNCTION()
	void HandleCreateClicked();
	UFUNCTION()
	void HandleQuickJoinClicked();
	UFUNCTION()
	void HandleRoom0JoinClicked();
	UFUNCTION()
	void HandleRoom1JoinClicked();
	UFUNCTION()
	void HandleRoom2JoinClicked();
	UFUNCTION()
	void HandleHostClicked();
	UFUNCTION()
	void HandleQuitClicked();
	void JoinLocalServer();
	void RetryLocalServerJoin();

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> NicknameBox;
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> RoomNameBox;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDSTRServerStatusQuery>> StatusQueries;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> RoomRows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RoomNameTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RoomStateTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PopulationTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> RoomJoinButtons;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EmptyListText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AvailabilityText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> CreateButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> QuickJoinButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> RefreshButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	FTimerHandle LocalServerTimerHandle;
	FTimerHandle EvidenceTimerHandle;
	TArray<FString> ServerAddresses;
	TArray<FDSTRServerStatus> ServerStatuses;
	TArray<double> NextRefreshAt;
	TArray<bool> bCompletedInitialRefresh;
	FString LocalServerAddress;
	int32 LocalServerJoinAttempts = 0;
	bool bJoining = false;
	bool bEvidenceScheduled = false;
};
