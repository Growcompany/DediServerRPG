#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DSTRAnimationAuthoringLibrary.generated.h"

class UAnimBlueprint;
class UAnimMontage;
class UAnimSequence;
class UAnimationAsset;

UCLASS()
class DEDISERVERRPG_API UDSTRAnimationAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "DSTR|Authoring")
	static UAnimMontage* CreateCombatMontage(UAnimSequence* Source, FName SlotName, const FString& PackagePath, const FString& AssetName, float ImpactTimeSeconds, bool bHoldLastFrame = false, float MaxLengthSeconds = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "DSTR|Authoring")
	static FString DescribeAnimation(UAnimationAsset* Asset);

	UFUNCTION(BlueprintCallable, Category = "DSTR|Authoring")
	static TArray<FName> GetAnimBlueprintSlotNames(UClass* AnimClass);

	UFUNCTION(BlueprintCallable, Category = "DSTR|Authoring")
	static bool AddFullBodySlot(UAnimBlueprint* AnimBlueprint, FName SlotName);

	UFUNCTION(BlueprintCallable, Category = "DSTR|Authoring")
	static bool RemoveLegacyPlayerCharacterNodes(UAnimBlueprint* AnimBlueprint);

	UFUNCTION(BlueprintCallable, Category = "DSTR|Authoring")
	static FName GetFullBodySlotName();

	UFUNCTION(BlueprintCallable, Category = "DSTR|Authoring")
	static TArray<FString> GetFullBodyMontageNames();
};
