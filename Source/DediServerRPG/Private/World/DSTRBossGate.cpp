#include "World/DSTRBossGate.h"
#include "DSTRLog.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "AI/NavigationSystemBase.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Game/DSTRDungeonRules.h"
#include "GameFramework/Pawn.h"
#include "GameplayCueManager.h"
#include "NavAreas/NavArea_Null.h"
#include "Net/UnrealNetwork.h"
#include "Presentation/DSTRCombatFeedback.h"
#include "Presentation/DSTRVisualAssetRegistry.h"

namespace
{
	const FLinearColor SealedLightColor(0.20f, 0.55f, 1.0f);
	const FLinearColor LockedLightColor(1.0f, 0.20f, 0.12f);
	constexpr float GateLightIntensity = 120.0f;
}

ADSTRBossGate::ADSTRBossGate()
{
	bReplicates = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = false;

	GateRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GateRoot"));
	SetRootComponent(GateRoot);

	Barrier = CreateDefaultSubobject<UBoxComponent>(TEXT("Barrier"));
	Barrier->SetupAttachment(GateRoot);
	Barrier->SetBoxExtent(FVector(
		FDSTRDungeonRules::GateThickness * 0.5f,
		FDSTRDungeonRules::GateWidth * 0.5f,
		FDSTRDungeonRules::GateHeight * 0.5f));
	Barrier->SetRelativeLocation(FVector(0.0f, 0.0f, FDSTRDungeonRules::GateHeight * 0.5f));
	Barrier->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Barrier->SetCollisionObjectType(ECC_WorldStatic);
	Barrier->SetCollisionResponseToAllChannels(ECR_Ignore);
	Barrier->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	// 물리 충돌과 내비 차단을 같은 배리어로 맞춘다.
	Barrier->SetCanEverAffectNavigation(true);
	Barrier->bDynamicObstacle = true;
	Barrier->SetAreaClassOverride(UNavArea_Null::StaticClass());

	EntryTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EntryTrigger"));
	EntryTrigger->SetupAttachment(GateRoot);
	EntryTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EntryTrigger->SetCollisionObjectType(ECC_WorldStatic);
	EntryTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	EntryTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EntryTrigger->SetGenerateOverlapEvents(true);
	EntryTrigger->SetCanEverAffectNavigation(false);

	GateLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GateLight"));
	GateLight->SetupAttachment(Barrier);
	GateLight->SetIntensityUnits(ELightUnits::Candelas);
	GateLight->SetIntensity(GateLightIntensity);
	GateLight->SetAttenuationRadius(FDSTRDungeonRules::GateWidth);
	GateLight->SetCastShadows(false);
	GateLight->SetMobility(EComponentMobility::Movable);
}

FVector ADSTRBossGate::GetEntryPoint() const
{
	return FDSTRDungeonRules::GetGateEntryPoint(GetActorLocation(), GetGateForward());
}

void ADSTRBossGate::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		FitToCorridor();
		EntryTrigger->OnComponentBeginOverlap.AddDynamic(this, &ADSTRBossGate::HandleEntryOverlap);
	}
	ApplyBarrierSize();
	ApplyCollision();
	ApplyCurtain();
}

void ADSTRBossGate::FitToCorridor()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Centre = GetActorLocation() + FVector(0.0f, 0.0f, FDSTRDungeonRules::GateHeight * 0.5f);
	const FVector Lateral = GetActorRightVector().GetSafeNormal2D();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DSTRGateFit), false, this);

	float Sides[2] = {FDSTRDungeonRules::GateFitTraceDistance, FDSTRDungeonRules::GateFitTraceDistance};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const float Sign = Index == 0 ? -1.0f : 1.0f;
		FHitResult Hit;
		const FVector End = Centre + Lateral * Sign * FDSTRDungeonRules::GateFitTraceDistance;
		if (World->LineTraceSingleByChannel(Hit, Centre, End, ECC_WorldStatic, Params))
		{
			Sides[Index] = static_cast<float>(FVector::Dist2D(Centre, Hit.ImpactPoint));
		}
	}

	BarrierHalfWidth = FDSTRDungeonRules::FitGateWidth(
		Sides[0],
		Sides[1],
		FDSTRDungeonRules::GateFitMinHalfWidth,
		FDSTRDungeonRules::GateFitMaxHalfWidth);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_GATE_FIT Width=%.0f Left=%.0f Right=%.0f"),
		BarrierHalfWidth, Sides[0], Sides[1]);
}

void ADSTRBossGate::ApplyBarrierSize()
{
	Barrier->SetBoxExtent(FVector(
		FDSTRDungeonRules::GateThickness * 0.5f,
		BarrierHalfWidth,
		FDSTRDungeonRules::GateHeight * 0.5f));
	FNavigationSystem::UpdateComponentData(*Barrier);

	EntryTrigger->SetBoxExtent(FVector(
		FDSTRDungeonRules::GateEntryTriggerThickness * 0.5f,
		BarrierHalfWidth,
		FDSTRDungeonRules::GateEntryTriggerHeight * 0.5f));
	EntryTrigger->SetRelativeLocation(FVector(
		(FDSTRDungeonRules::GateThickness + FDSTRDungeonRules::GateEntryTriggerThickness) * 0.5f,
		0.0f,
		FDSTRDungeonRules::GateEntryTriggerHeight * 0.5f));
}

void ADSTRBossGate::OnRep_BarrierHalfWidth()
{
	ApplyBarrierSize();
}

void ADSTRBossGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCurtain();
	Super::EndPlay(EndPlayReason);
}

void ADSTRBossGate::SetGateState(const EDSTRGateState NewState)
{
	if (!HasAuthority() || GateState == NewState)
	{
		return;
	}
	GateState = NewState;
	OnRep_GateState();
	ForceNetUpdate();
}

void ADSTRBossGate::OnRep_GateState()
{
	ApplyCollision();
	ApplyCurtain();
}

void ADSTRBossGate::ApplyCollision()
{
	const bool bClosed = GateState != EDSTRGateState::Open;
	// 열릴 때 충돌과 내비 차단을 함께 해제한다.
	Barrier->SetCollisionEnabled(bClosed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Barrier->SetCanEverAffectNavigation(bClosed);
	EntryTrigger->SetCollisionEnabled(HasAuthority() && !bClosed && !bEntryConsumed
		? ECollisionEnabled::QueryOnly
		: ECollisionEnabled::NoCollision);
}

void ADSTRBossGate::HandleEntryOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!HasAuthority() || bEntryConsumed || GateState != EDSTRGateState::Open
		|| !Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}
	const FVector Forward = GetGateForward().GetSafeNormal2D();
	if (!FDSTRDungeonRules::EnteredGateInward(Pawn->GetVelocity(), Forward))
	{
		return;
	}
	const FVector Offset = Pawn->GetActorLocation() - GetActorLocation();
	UE_LOG(LogDSTR, Log,
		TEXT("DSTR_GATE_OVERLAP Player=%s At=%s Lateral=%.0f HalfWidth=%.0f Depth=%.0f"),
		*GetNameSafe(Pawn),
		*Pawn->GetActorLocation().ToCompactString(),
		FMath::Abs(FVector::DotProduct(Offset, GetActorRightVector().GetSafeNormal2D())),
		BarrierHalfWidth,
		FVector::DotProduct(Offset, Forward));
	bEntryConsumed = true;
	EntryTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OnGateEntered.Broadcast(Pawn);
}

void ADSTRBossGate::ApplyCurtain()
{
	if (!FDSTRCombatFeedbackPolicy::ShouldPresent(GetNetMode())
		|| !FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(this))
	{
		return;
	}

	GateLight->SetVisibility(GateState != EDSTRGateState::Open);
	GateLight->SetLightColor(
		GateState == EDSTRGateState::Locked ? LockedLightColor : SealedLightColor);
	SetCurtainCue(GateState == EDSTRGateState::Sealed);
}

void ADSTRBossGate::SetCurtainCue(const bool bActive)
{
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!CueManager || bCurtainCueActive == bActive)
	{
		return;
	}
	bCurtainCueActive = bActive;

	FGameplayCueParameters Parameters;
	Parameters.Location = Barrier->GetComponentLocation();
	Parameters.Normal = GetActorRotation().Vector();
	CueManager->HandleGameplayCue(
		this,
		DSTRGameplayTags::GameplayCue_DSTR_GateSealed.GetTag(),
		bActive ? EGameplayCueEvent::WhileActive : EGameplayCueEvent::Removed,
		Parameters);
}

void ADSTRBossGate::StopCurtain()
{
	SetCurtainCue(false);
}

void ADSTRBossGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADSTRBossGate, GateState);
	DOREPLIFETIME(ADSTRBossGate, BarrierHalfWidth);
}
