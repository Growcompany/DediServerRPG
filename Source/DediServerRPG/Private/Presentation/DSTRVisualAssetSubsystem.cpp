#include "Presentation/DSTRVisualAssetSubsystem.h"

#include "DSTRLog.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Presentation/DSTRVisualAssetRegistry.h"

UDSTRVisualAssetSubsystem* UDSTRVisualAssetSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UDSTRVisualAssetSubsystem>() : nullptr;
}

void UDSTRVisualAssetSubsystem::Deinitialize()
{
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->ReleaseHandle();
		PreloadHandle.Reset();
	}
	PendingCallbacks.Empty();
	bPreloadStarted = false;
	bLoaded = false;
	Super::Deinitialize();
}

void UDSTRVisualAssetSubsystem::Preload(TFunction<void()> Completion)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	// 데디 서버는 코스메틱 자산을 로드하지 않는다.
	if (!World || !FDSTRVisualAssetRegistry::ShouldLoadVisualAssets(World->GetNetMode()))
	{
		return;
	}
	if (bLoaded)
	{
		if (Completion)
		{
			Completion();
		}
		return;
	}
	// 중복 요청은 같은 비동기 로드 완료를 함께 기다린다.
	if (Completion)
	{
		PendingCallbacks.Add(MoveTemp(Completion));
	}
	if (bPreloadStarted)
	{
		return;
	}

	bPreloadStarted = true;
	PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		FDSTRVisualAssetRegistry::GetPreloadPaths(),
		FStreamableDelegate::CreateUObject(this, &UDSTRVisualAssetSubsystem::CompletePreload),
		FStreamableManager::AsyncLoadHighPriority,
		false,
		false,
		TEXT("DSTRPresentationPreload"));
	if (!PreloadHandle.IsValid())
	{
		CompletePreload();
	}
}

void UDSTRVisualAssetSubsystem::CompletePreload()
{
	bLoaded = true;
	UE_LOG(LogDSTR, Log, TEXT("DSTR_VISUAL_PRELOAD_COMPLETE"));
	TArray<TFunction<void()>> Callbacks = MoveTemp(PendingCallbacks);
	for (TFunction<void()>& Callback : Callbacks)
	{
		if (Callback)
		{
			Callback();
		}
	}
}
