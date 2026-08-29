#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DSTRLobbyWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class DEDISERVERRPG_API UDSTRLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();
	UTextBlock* MakeText(const FString& Text, int32 Size, const FLinearColor& Color);
	UButton* MakeButton(const FString& Label);

	UFUNCTION()
	void HandleStartClicked();
	UFUNCTION()
	void HandleLeaveClicked();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SlotTexts;
	UPROPERTY(Transient)
	TObjectPtr<UButton> StartButton;
	bool bClosed = false;
	static constexpr float RefreshInterval = 0.1f;
	float RefreshAccumulator = 0.0f;
};
