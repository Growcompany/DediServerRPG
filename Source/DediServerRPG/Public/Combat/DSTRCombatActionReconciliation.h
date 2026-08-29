#pragma once

#include "CoreMinimal.h"
#include "Presentation/DSTRCombatPresentation.h"

struct DEDISERVERRPG_API FDSTRPredictedCombatActionReconciliation
{
	static bool ShouldSuppressLocalReplay(
		EDSTRCombatAction LocalAction,
		uint8 LocalVariant,
		EDSTRCombatAction AuthoritativeAction,
		uint8 AuthoritativeVariant,
		bool bWithinRecovery);
	static uint8 NormalizeVariant(uint8 Variant, int32 VariantCount);
	static uint8 NextBasicAttackVariant(uint8 AuthoritativeVariant, int32 VariantCount);
	static uint8 ReconcileNextBasicAttackVariant(
		uint8 CurrentNextVariant,
		EDSTRCombatAction AuthoritativeAction,
		uint8 AuthoritativeVariant,
		int32 VariantCount);
};
