#pragma once

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "DSTRLocalPlayer.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:
	virtual FString GetNickname() const override;
};
