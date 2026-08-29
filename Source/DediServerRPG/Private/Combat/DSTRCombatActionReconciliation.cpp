#include "Combat/DSTRCombatActionReconciliation.h"

bool FDSTRPredictedCombatActionReconciliation::ShouldSuppressLocalReplay(
	const EDSTRCombatAction LocalAction,
	const uint8 LocalVariant,
	const EDSTRCombatAction AuthoritativeAction,
	const uint8 AuthoritativeVariant,
	const bool bWithinRecovery)
{
	return bWithinRecovery
		&& LocalAction == AuthoritativeAction
		&& LocalVariant == AuthoritativeVariant;
}

uint8 FDSTRPredictedCombatActionReconciliation::NormalizeVariant(
	const uint8 Variant,
	const int32 VariantCount)
{
	return VariantCount > 0
		? static_cast<uint8>(static_cast<int32>(Variant) % VariantCount)
		: 0;
}

uint8 FDSTRPredictedCombatActionReconciliation::NextBasicAttackVariant(
	const uint8 AuthoritativeVariant,
	const int32 VariantCount)
{
	if (VariantCount <= 1)
	{
		return 0;
	}
	const int32 NormalizedVariant = NormalizeVariant(AuthoritativeVariant, VariantCount);
	return static_cast<uint8>((NormalizedVariant + 1) % VariantCount);
}

uint8 FDSTRPredictedCombatActionReconciliation::ReconcileNextBasicAttackVariant(
	const uint8 CurrentNextVariant,
	const EDSTRCombatAction AuthoritativeAction,
	const uint8 AuthoritativeVariant,
	const int32 VariantCount)
{
	return AuthoritativeAction == EDSTRCombatAction::BasicAttack
		? NextBasicAttackVariant(AuthoritativeVariant, VariantCount)
		: CurrentNextVariant;
}
