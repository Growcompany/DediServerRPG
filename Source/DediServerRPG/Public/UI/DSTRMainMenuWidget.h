#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DSTRMainMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

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

private:
	void BuildWidgetTree();
	UTextBlock* MakeText(const FString& Text, int32 Size, const FLinearColor& Color);
	UButton* MakeButton(const FString& Label);
	void SetStatus(const FString& Text, const FLinearColor& Color);
	void JoinAddress(const FString& AddressText);
	void CaptureMenuEvidence();

	UFUNCTION()
	void HandleHostClicked();
	UFUNCTION()
	void HandleJoinClicked();
	UFUNCTION()
	void HandleQuitClicked();
	void JoinLocalServer();
	void RetryLocalServerJoin();

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> NicknameBox;
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> AddressBox;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
	FTimerHandle LocalServerTimerHandle;
	FTimerHandle EvidenceTimerHandle;
	int32 LocalServerJoinAttempts = 0;
};
