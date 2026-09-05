#include "World/DSTRAttackBuffPickup.h"
#include "DSTRLog.h"

#include "Game/DSTRActorRegistry.h"
#include "Game/DSTRGameState.h"
#include "GameFramework/PlayerState.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DSTRGameplayEffects.h"
#include "Combat/DSTRCombatLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "Presentation/DSTRVisualAssetRegistry.h"

ADSTRAttackBuffPickup::ADSTRAttackBuffPickup()
{
	bReplicates = true;
	SetReplicateMovement(true);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	SetRootComponent(InteractionSphere);
	InteractionSphere->InitSphereRadius(80.0f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(InteractionSphere);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetRelativeScale3D(FVector(0.45f));
}

void ADSTRAttackBuffPickup::BeginPlay()
{
	Super::BeginPlay();
	if (UDSTRActorRegistry* Registry = UDSTRActorRegistry::Get(GetWorld()))
	{
		Registry->RegisterPickup(this);
	}
	FDSTRVisualAssetRegistry::PreloadVisualAssets(
		this,
		[WeakThis = TWeakObjectPtr<ADSTRAttackBuffPickup>(this)]()
	{
		if (ADSTRAttackBuffPickup* Pickup = WeakThis.Get())
		{
			Pickup->ApplyPickupMesh();
		}
	});
}

void ADSTRAttackBuffPickup::ApplyPickupMesh()
{
	UStaticMesh* Mesh = Cast<UStaticMesh>(
		FDSTRVisualAssetRegistry::GetAttackBuffPickupMesh().ResolveObject());
	if (PickupMesh && Mesh)
	{
		PickupMesh->SetStaticMesh(Mesh);
	}
}

void ADSTRAttackBuffPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDSTRActorRegistry* Registry = UDSTRActorRegistry::Get(GetWorld()))
	{
		Registry->UnregisterPickup(this);
	}
	Super::EndPlay(EndPlayReason);
}

// 픽업 소비도 서버 확정. 거리·상태 재검사 후 적용
bool ADSTRAttackBuffPickup::TryConsume(ADediServerRPGCharacter* Player)
{
	if (!HasAuthority() || !Player)
	{
		return false;
	}

	const float Distance = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
	if (!UDSTRCombatLibrary::CanConsumePickup(bConsumed, Player->IsDowned(), Distance))
	{
		return false;
	}

	UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
	if (!PlayerASC)
	{
		return false;
	}

	PlayerASC->ApplyGameplayEffectToSelf(
		GetDefault<UDSTRAttackBuffEffect>(),
		1.0f,
		PlayerASC->MakeEffectContext());
	bConsumed = true;
	OnRep_Consumed();
	Player->PlayAbilityFeedback(
		EDSTRCombatFeedback::AttackBuff,
		GetActorLocation(),
		FVector::UpVector,
		120.0f);
	ForceNetUpdate();
	SetLifeSpan(0.5f);
	if (ADSTRGameState* State = GetWorld()->GetGameState<ADSTRGameState>())
	{
		State->PushEvent(EDSTRMatchEventKind::BuffPicked,
			Player->GetPlayerState() ? Player->GetPlayerState()->GetPlayerName() : Player->GetName());
	}

	UE_LOG(LogDSTR, Log, TEXT("DSTR_PICKUP Player=%s Effect=AttackPower Duration=30"), *GetNameSafe(Player));
	return true;
}

void ADSTRAttackBuffPickup::OnRep_Consumed()
{
	SetActorHiddenInGame(bConsumed);
	SetActorEnableCollision(!bConsumed);
}

void ADSTRAttackBuffPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADSTRAttackBuffPickup, bConsumed);
}
