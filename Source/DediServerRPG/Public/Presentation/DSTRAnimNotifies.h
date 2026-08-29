#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "DSTRAnimNotifies.generated.h"

UCLASS(meta = (DisplayName = "DSTR Impact"))
class DEDISERVERRPG_API UDSTRAnimNotify_Impact : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("DSTR Impact"); }
};
