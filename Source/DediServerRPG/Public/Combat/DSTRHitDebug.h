#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "Subsystems/WorldSubsystem.h"
#include "DSTRHitDebug.generated.h"

class AActor;

UENUM()
enum class EDSTRHitVerdict : uint8
{
	Hit,
	NotSelected,
	OutOfCone,
	OutOfReach,
	SightBlocked,
	AlreadyHit
};

USTRUCT()
struct FDSTRHitDebugTarget
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize Location = FVector::ZeroVector;

	UPROPERTY()
	float CapsuleRadius = 34.0f;

	UPROPERTY()
	float CapsuleHalfHeight = 88.0f;

	UPROPERTY()
	EDSTRHitVerdict Verdict = EDSTRHitVerdict::NotSelected;

	UPROPERTY()
	FVector_NetQuantize BlockPoint = FVector::ZeroVector;

	// 서버가 판정을 갱신할 대상을 찾을 때만 쓰는 키라 UPROPERTY가 아니다(복제 제외).
	TWeakObjectPtr<const AActor> TargetKey;
};

USTRUCT()
struct FDSTRHitDebugFrame
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize Origin = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantizeNormal Forward = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantize SightOrigin = FVector::ZeroVector;

	UPROPERTY()
	float Reach = 0.0f;

	UPROPERTY()
	float ConeHalfAngleDegrees = 0.0f;

	UPROPERTY()
	FName Skill;

	UPROPERTY()
	TArray<FDSTRHitDebugTarget> Targets;
};

UCLASS()
class DEDISERVERRPG_API UDSTRHitDebugSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDSTRHitDebugSubsystem* Get(const UWorld* World);
	static bool IsEnabled();

	void Begin(
		const AActor* Source,
		const FVector& Origin,
		float Reach,
		float ConeHalfAngleDegrees,
		FName Skill);
	void RecordCandidate(
		const AActor* Source,
		const AActor* Target,
		EDSTRHitVerdict Verdict,
		const FVector& BlockPoint = FVector::ZeroVector);
	void SetVerdict(const AActor* Source, const AActor* Target, EDSTRHitVerdict Verdict);
	void Commit(const AActor* Source);
	void Draw(const FDSTRHitDebugFrame& Frame) const;

	static FColor GetVerdictColor(EDSTRHitVerdict Verdict);
	static const TCHAR* GetVerdictLabel(EDSTRHitVerdict Verdict);

private:
	TMap<TWeakObjectPtr<const AActor>, FDSTRHitDebugFrame> Pending;
};
