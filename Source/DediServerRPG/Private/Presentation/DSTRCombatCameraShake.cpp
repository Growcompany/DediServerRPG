#include "Presentation/DSTRCombatCameraShake.h"

#include "Presentation/DSTRCombatFeedback.h"

void UDSTRCombatShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(Duration);
}

void UDSTRCombatShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Elapsed = 0.0f;
}

void UDSTRCombatShakePattern::UpdateShakePatternImpl(
	const FCameraShakePatternUpdateParams& Params,
	FCameraShakePatternUpdateResult& OutResult)
{
	Elapsed += Params.DeltaTime;
	const FDSTRShakeSample Sample = FDSTRCombatFeedbackPolicy::EvaluateShake(
		Elapsed, Duration, Amplitude, Frequency);
	OutResult.Location = Sample.LocationOffset;
	OutResult.Rotation = Sample.RotationOffset;
}

bool UDSTRCombatShakePattern::IsFinishedImpl() const
{
	return Elapsed >= Duration;
}

UDSTRCombatCameraShake::UDSTRCombatCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = false;
	SetRootShakePattern(CreateDefaultSubobject<UDSTRCombatShakePattern>(TEXT("ShakePattern")));
}
