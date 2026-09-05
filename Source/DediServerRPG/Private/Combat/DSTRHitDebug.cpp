#include "Combat/DSTRHitDebug.h"
#include "DSTRLog.h"

#include "Combat/DSTRHitQuery.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Game/DSTRGameState.h"
#include "GameFramework/Character.h"

namespace
{
	// 기본 0. 켜기 전에는 기록·전송 없음
	TAutoConsoleVariable<int32> CVarDSTRDebugHitQuery(
		TEXT("DSTR.Debug.HitQuery"),
		0,
		TEXT("1이면 서버가 계산한 히트 판정을 클라이언트 디버그 드로우로 전송합니다."),
		ECVF_Default);

	constexpr float HitDebugLifetime = 1.5f;
	constexpr float HitDebugThickness = 2.0f;
	constexpr int32 HitDebugArcSegments = 24;
}

UDSTRHitDebugSubsystem* UDSTRHitDebugSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UDSTRHitDebugSubsystem>() : nullptr;
}

bool UDSTRHitDebugSubsystem::IsEnabled()
{
	return CVarDSTRDebugHitQuery.GetValueOnGameThread() >= 1;
}

FColor UDSTRHitDebugSubsystem::GetVerdictColor(const EDSTRHitVerdict Verdict)
{
	switch (Verdict)
	{
	case EDSTRHitVerdict::Hit: return FColor::Green;
	case EDSTRHitVerdict::OutOfCone: return FColor::Orange;
	case EDSTRHitVerdict::OutOfReach: return FColor::Silver;
	case EDSTRHitVerdict::SightBlocked: return FColor::Red;
	case EDSTRHitVerdict::AlreadyHit: return FColor::Cyan;
	default: return FColor::Yellow;
	}
}

const TCHAR* UDSTRHitDebugSubsystem::GetVerdictLabel(const EDSTRHitVerdict Verdict)
{
	switch (Verdict)
	{
	case EDSTRHitVerdict::Hit: return TEXT("HIT");
	case EDSTRHitVerdict::OutOfCone: return TEXT("OUT_OF_CONE");
	case EDSTRHitVerdict::OutOfReach: return TEXT("OUT_OF_REACH");
	case EDSTRHitVerdict::SightBlocked: return TEXT("SIGHT_BLOCKED");
	case EDSTRHitVerdict::AlreadyHit: return TEXT("ALREADY_HIT");
	default: return TEXT("NOT_SELECTED");
	}
}

void UDSTRHitDebugSubsystem::Begin(
	const AActor* Source,
	const FVector& Origin,
	const float Reach,
	const float ConeHalfAngleDegrees,
	const FName Skill)
{
#if !UE_BUILD_SHIPPING
	if (!IsEnabled() || !Source)
	{
		return;
	}
	FDSTRHitDebugFrame& Frame = Pending.FindOrAdd(Source);
	Frame = FDSTRHitDebugFrame();
	Frame.Origin = Origin;
	Frame.Forward = Source->GetActorForwardVector().GetSafeNormal();
	Frame.SightOrigin = FDSTRHitQuery::GetSightOrigin(Source, Origin);
	Frame.Reach = Reach;
	Frame.ConeHalfAngleDegrees = ConeHalfAngleDegrees;
	Frame.Skill = Skill;
#endif
}

void UDSTRHitDebugSubsystem::RecordCandidate(
	const AActor* Source,
	const AActor* Target,
	const EDSTRHitVerdict Verdict,
	const FVector& BlockPoint)
{
#if !UE_BUILD_SHIPPING
	if (!IsEnabled() || !Source || !Target)
	{
		return;
	}
	// Begin을 하지 않은 호출자는 기록 대상이 아니다.
	FDSTRHitDebugFrame* Frame = Pending.Find(Source);
	if (!Frame)
	{
		return;
	}

	FDSTRHitDebugTarget Entry;
	Entry.TargetKey = Target;
	Entry.Location = Target->GetActorLocation();
	Entry.Verdict = Verdict;
	Entry.BlockPoint = BlockPoint;
	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			Entry.CapsuleRadius = Capsule->GetScaledCapsuleRadius();
			Entry.CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
	}
	Frame->Targets.Add(Entry);
#endif
}

void UDSTRHitDebugSubsystem::SetVerdict(
	const AActor* Source,
	const AActor* Target,
	const EDSTRHitVerdict Verdict)
{
#if !UE_BUILD_SHIPPING
	if (!IsEnabled() || !Source || !Target)
	{
		return;
	}
	if (FDSTRHitDebugFrame* Frame = Pending.Find(Source))
	{
		for (FDSTRHitDebugTarget& Entry : Frame->Targets)
		{
			if (Entry.TargetKey == Target)
			{
				Entry.Verdict = Verdict;
				return;
			}
		}
	}
	RecordCandidate(Source, Target, Verdict);
#endif
}

// 모은 프레임 → GameState 멀티캐스트 → 클라가 드로우
void UDSTRHitDebugSubsystem::Commit(const AActor* Source)
{
#if !UE_BUILD_SHIPPING
	if (!Source)
	{
		return;
	}
	FDSTRHitDebugFrame Frame;
	if (!Pending.RemoveAndCopyValue(Source, Frame) || Frame.Targets.Num() <= 0)
	{
		return;
	}
	UE_LOG(LogDSTR, Verbose, TEXT("DSTR_HIT_DEBUG Skill=%s Targets=%d"),
		*Frame.Skill.ToString(), Frame.Targets.Num());

	const UWorld* World = Source->GetWorld();
	if (ADSTRGameState* GameState = World ? World->GetGameState<ADSTRGameState>() : nullptr)
	{
		GameState->MulticastHitDebug(Frame);
	}
#endif
}

void UDSTRHitDebugSubsystem::Draw(const FDSTRHitDebugFrame& Frame) const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UE_LOG(LogDSTR, Verbose, TEXT("DSTR_HIT_DEBUG_DRAW Skill=%s Targets=%d"),
		*Frame.Skill.ToString(), Frame.Targets.Num());

	const FColor QueryColor(0, 200, 255);
	const FVector Ground = FVector(Frame.Origin) + FVector(0.0f, 0.0f, 5.0f);
	if (Frame.ConeHalfAngleDegrees > 0.0f)
	{
		const FVector Facing = FVector(Frame.Forward).GetSafeNormal2D();
		FVector Previous = Ground
			+ Facing.RotateAngleAxis(-Frame.ConeHalfAngleDegrees, FVector::UpVector) * Frame.Reach;
		DrawDebugLine(World, Ground, Previous, QueryColor, false, HitDebugLifetime, 0, HitDebugThickness);
		for (int32 Index = 1; Index <= HitDebugArcSegments; ++Index)
		{
			const float Alpha = static_cast<float>(Index) / static_cast<float>(HitDebugArcSegments);
			const FVector Direction = Facing.RotateAngleAxis(
				FMath::Lerp(-Frame.ConeHalfAngleDegrees, Frame.ConeHalfAngleDegrees, Alpha),
				FVector::UpVector);
			const FVector Point = Ground + Direction * Frame.Reach;
			DrawDebugLine(World, Previous, Point, QueryColor, false, HitDebugLifetime, 0, HitDebugThickness);
			Previous = Point;
		}
		DrawDebugLine(World, Ground, Previous, QueryColor, false, HitDebugLifetime, 0, HitDebugThickness);
	}
	else
	{
		DrawDebugCircle(World, Ground, Frame.Reach, 32, QueryColor, false, HitDebugLifetime, 0,
			HitDebugThickness, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), false);
		DrawDebugSphere(World, Frame.Origin, Frame.Reach, 12, QueryColor, false, HitDebugLifetime, 0,
			HitDebugThickness);
	}

	for (const FDSTRHitDebugTarget& Target : Frame.Targets)
	{
		const FColor Color = GetVerdictColor(Target.Verdict);
		DrawDebugCapsule(World, Target.Location, Target.CapsuleHalfHeight, Target.CapsuleRadius,
			FQuat::Identity, Color, false, HitDebugLifetime, 0, HitDebugThickness);
		DrawDebugString(World,
			FVector(Target.Location) + FVector(0.0f, 0.0f, Target.CapsuleHalfHeight + 30.0f),
			FString::Printf(TEXT("%s %s"), *Frame.Skill.ToString(), GetVerdictLabel(Target.Verdict)),
			nullptr, Color, HitDebugLifetime);
		if (Target.Verdict == EDSTRHitVerdict::SightBlocked)
		{
			DrawDebugLine(World, Frame.SightOrigin, Target.BlockPoint, FColor::Red, false,
				HitDebugLifetime, 0, HitDebugThickness);
			DrawDebugSphere(World, Target.BlockPoint, 8.0f, 8, FColor::Red, false, HitDebugLifetime, 0,
				HitDebugThickness);
		}
	}
#endif
}
