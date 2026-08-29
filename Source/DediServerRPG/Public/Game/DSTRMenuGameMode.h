#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DSTRMenuGameMode.generated.h"

UCLASS()
class DEDISERVERRPG_API ADSTRMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADSTRMenuGameMode();

protected:
	virtual void BeginPlay() override;
};
