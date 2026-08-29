#include "Presentation/DSTRAnimNotifies.h"

#include "Combat/DSTRCombatantInterface.h"
#include "Components/SkeletalMeshComponent.h"

void UDSTRAnimNotify_Impact::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (IDSTRCombatantInterface* Combatant = Cast<IDSTRCombatantInterface>(Owner))
	{
		Combatant->HandleAnimationImpact(Animation);
	}
}
