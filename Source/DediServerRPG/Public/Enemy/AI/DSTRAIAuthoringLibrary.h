#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DSTRAIAuthoringLibrary.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRAIAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "DSTR|Authoring")
	static bool CreateOrUpdateEnemyBehaviorTreeAssets();
};
