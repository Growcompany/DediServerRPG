#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DSTRCombatantInterface.generated.h"

UENUM(BlueprintType)
enum class EDSTRCombatTeam : uint8
{
	Player,
	Enemy
};

UINTERFACE(MinimalAPI)
class UDSTRCombatantInterface : public UInterface
{
	GENERATED_BODY()
};

class DEDISERVERRPG_API IDSTRCombatantInterface
{
	GENERATED_BODY()

public:
	virtual void HandleOutOfHealth() = 0;
	virtual bool IsCombatantDead() const = 0;
	virtual EDSTRCombatTeam GetCombatTeam() const = 0;
	virtual void HandleAnimationImpact(const class UAnimSequenceBase* Animation) {}
};
