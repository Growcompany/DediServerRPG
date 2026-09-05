#include "Player/DSTRPlayerState.h"

#include "AbilitySystem/DSTRAbilitySystemComponent.h"
#include "AbilitySystem/DSTRAttributeSet.h"
#include "Game/DSTRDownedRules.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

// ASC 소유자 = PlayerState (Pawn 소유면 리스폰 시 상태 소실)
ADSTRPlayerState::ADSTRPlayerState()
{
	NetUpdateFrequency = 10.0f;

	AbilitySystemComponent = CreateDefaultSubobject<UDSTRAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	// Mixed: GE는 소유 클라에만 복제 (대역폭 절약)
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UDSTRAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* ADSTRPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UDSTRAbilitySystemComponent* ADSTRPlayerState::GetDSTRAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const UDSTRAttributeSet* ADSTRPlayerState::GetAttributeSet() const
{
	return AttributeSet;
}

void ADSTRPlayerState::SetPresentationReady(const bool bNewPresentationReady)
{
	if (HasAuthority())
	{
		bPresentationReady = bNewPresentationReady;
		ForceNetUpdate();
	}
}

void ADSTRPlayerState::StartBleedOut()
{
	// 남은 초 대신 서버 마감 시각 하나만 복제한다.
	const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!HasAuthority() || bEliminated || !State)
	{
		return;
	}
	BleedOutEndServerTime = State->GetServerWorldTimeSeconds() + FDSTRDownedRules::BleedOutSeconds;
	ForceNetUpdate();
}

void ADSTRPlayerState::ClearBleedOut()
{
	if (HasAuthority())
	{
		BleedOutEndServerTime = 0.0f;
		ForceNetUpdate();
	}
}

void ADSTRPlayerState::MarkEliminated()
{
	if (HasAuthority())
	{
		bEliminated = true;
		BleedOutEndServerTime = 0.0f;
		ForceNetUpdate();
	}
}

float ADSTRPlayerState::GetBleedOutRemaining() const
{
	const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return State
		? FDSTRDownedRules::GetBleedOutRemaining(BleedOutEndServerTime, State->GetServerWorldTimeSeconds())
		: 0.0f;
}

void ADSTRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADSTRPlayerState, bPresentationReady);
	DOREPLIFETIME(ADSTRPlayerState, BleedOutEndServerTime);
	DOREPLIFETIME(ADSTRPlayerState, bEliminated);
}
