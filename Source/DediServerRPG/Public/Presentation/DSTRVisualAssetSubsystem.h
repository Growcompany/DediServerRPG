#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DSTRVisualAssetSubsystem.generated.h"

struct FStreamableHandle;

UCLASS()
class DEDISERVERRPG_API UDSTRVisualAssetSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UDSTRVisualAssetSubsystem* Get(const UObject* WorldContextObject);

	virtual void Deinitialize() override;

	void Preload(TFunction<void()> Completion);
	bool IsLoaded() const { return bLoaded; }

private:
	void CompletePreload();

	TSharedPtr<FStreamableHandle> PreloadHandle;
	TArray<TFunction<void()>> PendingCallbacks;
	bool bPreloadStarted = false;
	bool bLoaded = false;
};
