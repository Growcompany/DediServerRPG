#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "DSTRCombatFeedback.generated.h"

class AActor;
class UWorld;

UENUM(BlueprintType)
enum class EDSTRCombatFeedback : uint8
{
	BasicAttack,
	Downed,
	Revived,
	AttackBuff,
	EnemyTelegraph,
	HitDealt,
	HitTaken,
	EnemyDeath,
	BossImpact,
	Fortify,
	MakeWay,
	Charge,
	ReckoningWarning,
	Reckoning,
	EnemyWake,
	BossRush,
	BossSiphon,
	BossSubjugate
};

enum class EDSTRFeedbackViewer : uint8
{
	Remote,
	Instigator,
	Victim
};

struct FDSTRShakeSample
{
	FVector LocationOffset = FVector::ZeroVector;
	FRotator RotationOffset = FRotator::ZeroRotator;
};

class DEDISERVERRPG_API FDSTRCombatFeedbackPolicy
{
public:
	static constexpr float MaxShakeRadius = 2000.0f;

	static bool ShouldPresent(ENetMode NetMode);
	static EDSTRFeedbackViewer ResolveViewer(bool bLocalInstigator, bool bLocalVictim);
	static bool IsVictimFeedback(EDSTRCombatFeedback Feedback);
	static float ClampRadius(float Radius);
	static float ClampShakeScale(float Scale);
	static float GetShakeScale(
		EDSTRCombatFeedback Feedback,
		EDSTRFeedbackViewer Viewer,
		float Distance,
		float Radius);
	static FDSTRShakeSample EvaluateShake(
		float Elapsed,
		float Duration,
		float Amplitude,
		float Frequency);
};

struct FDSTRCombatFeedbackRequest
{
	EDSTRCombatFeedback Feedback = EDSTRCombatFeedback::BasicAttack;
	FVector Location = FVector::ZeroVector;
	FVector Direction = FVector::ZeroVector;
	float Radius = 0.0f;
	bool bBossVariant = false;
	bool bSkipSound = false;
	bool bSoundOnly = false;
	const AActor* InstigatorActor = nullptr;
	const AActor* VictimActor = nullptr;
};

class DEDISERVERRPG_API FDSTRCombatFeedbackPlayer
{
public:
	static void Play(UWorld* World, const FDSTRCombatFeedbackRequest& Request);

private:
	static void PlaySound(UWorld& World, const FDSTRCombatFeedbackRequest& Request);
	static void PlayCameraShake(UWorld& World, const FDSTRCombatFeedbackRequest& Request);
};
