#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "DSTRCombatCameraShake.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTRCombatShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "DSTR|Shake")
	float Duration = 0.25f;

	UPROPERTY(EditAnywhere, Category = "DSTR|Shake")
	float Amplitude = 6.0f;

	UPROPERTY(EditAnywhere, Category = "DSTR|Shake")
	float Frequency = 18.0f;

private:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(
		const FCameraShakePatternUpdateParams& Params,
		FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;

	float Elapsed = 0.0f;
};

UCLASS()
class DEDISERVERRPG_API UDSTRCombatCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UDSTRCombatCameraShake(const FObjectInitializer& ObjectInitializer);
};
