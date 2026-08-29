#include "Enemy/DSTREnemyCharacter.h"
#include "DSTRLog.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/DSTRAbilitySystemComponent.h"
#include "AbilitySystem/DSTRAttributeSet.h"
#include "AbilitySystem/DSTRGameplayEffects.h"
#include "AbilitySystem/Abilities/DSTRBossColossalBlowAbility.h"
#include "AbilitySystem/Abilities/DSTRBossPhantomRushAbility.h"
#include "AbilitySystem/Abilities/DSTRBossSiphonAbility.h"
#include "AbilitySystem/Abilities/DSTRBossSubjugateAbility.h"
#include "AbilitySystem/Abilities/DSTREnemyAttackAbility.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Enemy/DSTRAIController.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "Combat/DSTRDamageRules.h"
#include "Combat/DSTRCombatLibrary.h"
#include "Enemy/DSTRBossSkillRules.h"
#include "Enemy/DSTREnemyAIRules.h"
#include "EngineUtils.h"
#include "Game/DSTRActorRegistry.h"
#include "Game/DSTRDungeonRules.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Presentation/DSTRVisualAssetRegistry.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "TimerManager.h"

ADSTREnemyCharacter::ADSTREnemyCharacter()
{
	CurrentCombatAction = EDSTRCombatAction::None;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	bReplicates = true;
	SetReplicateMovement(true);
	AIControllerClass = ADSTRAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	AbilitySystemComponent = CreateDefaultSubobject<UDSTRAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	AttributeSet = CreateDefaultSubobject<UDSTRAttributeSet>(TEXT("AttributeSet"));

	GetCharacterMovement()->MaxWalkSpeed = 300.0f;
	// 포커스가 만든 컨트롤 회전을 몸 회전에 사용한다.
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	bUseControllerRotationYaw = false;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -88.0f), FRotator(0.0f, -90.0f, 0.0f));
}

void ADSTREnemyCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickCorpseSink();
	if (GetNetMode() != NM_DedicatedServer
		&& CurrentCombatAction == EDSTRCombatAction::None)
	{
		const EDSTRVisualRole VisualRole = bIsBoss
			? EDSTRVisualRole::Boss
			: EDSTRVisualRole::Enemy;
		FDSTRVisualAssetRegistry::UpdateMovementAnimation(
			*GetMesh(), VisualRole, GetVelocity(), GetActorForwardVector());
	}
}

void ADSTREnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (UDSTRActorRegistry* Registry = UDSTRActorRegistry::Get(GetWorld()))
	{
		Registry->RegisterEnemy(this);
	}
	FDSTRVisualAssetRegistry::PreloadVisualAssets(
		this,
		[WeakThis = TWeakObjectPtr<ADSTREnemyCharacter>(this)]()
		{
			if (ADSTREnemyCharacter* Enemy = WeakThis.Get())
			{
				Enemy->ApplyVisualAssets();
			}
		});
	InitializeAbilitySystem();
	if (HasAuthority())
	{
		ApplyConfiguredStats();
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UDSTREnemyAttackAbility::StaticClass(), 1));
	}
	else if (FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(this)
		&& FDSTRCombatPresentation::ShouldPlaySpawnEffect(bDormant))
	{
		PlaySpawnEffect();
	}
}

void ADSTREnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSpawnEffect();
	GetWorldTimerManager().ClearTimer(StunSegmentTimerHandle);
	if (UDSTRActorRegistry* Registry = UDSTRActorRegistry::Get(GetWorld()))
	{
		Registry->UnregisterEnemy(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ADSTREnemyCharacter::PlaySpawnEffect()
{
	UParticleSystem* Effect = Cast<UParticleSystem>(
		FDSTRVisualAssetRegistry::GetEnemySpawnEffect().ResolveObject());
	if (!Effect)
	{
		return;
	}
	const FVector Feet = GetActorLocation()
		- FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	StopSpawnEffect();
	SpawnEffectComponent = UGameplayStatics::SpawnEmitterAtLocation(
		GetWorld(), Effect, Feet, FRotator::ZeroRotator, FVector(0.45f), true);
	bSpawnEffectPending = true;
	GetWorldTimerManager().SetTimer(
		SpawnEffectTimerHandle,
		this,
		&ADSTREnemyCharacter::StopSpawnEffect,
		FDSTRCombatPresentation::SpawnEffectSeconds,
		false);
}

void ADSTREnemyCharacter::StopSpawnEffect()
{
	if (!bSpawnEffectPending)
	{
		return;
	}
	bSpawnEffectPending = false;
	const bool bDeactivated = SpawnEffectComponent.IsValid();
	if (UParticleSystemComponent* Effect = SpawnEffectComponent.Get())
	{
		Effect->DeactivateSystem();
	}
	SpawnEffectComponent.Reset();
	UE_LOG(LogDSTR, Log, TEXT("DSTR_SPAWN_FX Enemy=%s Seconds=%.1f Deactivated=%d"),
		*GetName(), FDSTRCombatPresentation::SpawnEffectSeconds, bDeactivated ? 1 : 0);
}

void ADSTREnemyCharacter::OnRep_Dormant()
{
	if (!bDormant)
	{
		PlayWakeEffect();
	}
}

void ADSTREnemyCharacter::PlayWakeEffect()
{
	if (!FDSTRCombatFeedbackPolicy::ShouldPresent(GetNetMode())
		|| !FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(this))
	{
		return;
	}
	const FVector Floor = GetActorLocation() - FVector(
		0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
			- FDSTRCombatPresentation::TelegraphFloorClearance);
	if (UParticleSystem* Effect = Cast<UParticleSystem>(
		FDSTRVisualAssetRegistry::GetMakeWayEffect().ResolveObject()))
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(), Effect, Floor, FRotator::ZeroRotator,
			FDSTRCombatPresentation::GetGroundRingScale3D(FDSTRCombatPresentation::WakeShockwaveRadius), true);
	}
	FDSTRCombatFeedbackRequest Request;
	Request.Feedback = EDSTRCombatFeedback::EnemyWake;
	Request.Location = GetActorLocation();
	Request.Radius = FDSTRCombatPresentation::WakeShockwaveRadius;
	Request.bBossVariant = bIsBoss;
	Request.InstigatorActor = this;
	FDSTRCombatFeedbackPlayer::Play(GetWorld(), Request);
	if (bIsBoss)
	{
		FDSTRCombatFeedbackRequest Rumble;
		Rumble.Feedback = EDSTRCombatFeedback::BossImpact;
		Rumble.Location = GetActorLocation();
		Rumble.Radius = FDSTRDungeonRules::BossWakeShakeRadius;
		Rumble.bBossVariant = true;
		Rumble.bSkipSound = true;
		Rumble.InstigatorActor = this;
		FDSTRCombatFeedbackPlayer::Play(GetWorld(), Rumble);
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_WAKE_FX Enemy=%s Boss=%s"),
		*GetName(), bIsBoss ? TEXT("true") : TEXT("false"));
}

UAbilitySystemComponent* ADSTREnemyCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UDSTRAbilitySystemComponent* ADSTREnemyCharacter::GetDSTRAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const UDSTRAttributeSet* ADSTREnemyCharacter::GetAttributeSet() const
{
	return AttributeSet;
}

float ADSTREnemyCharacter::GetConfiguredMaxHealth(
	const bool bBoss,
	const int32 PlayerCount)
{
	const int32 PartySize = FMath::Clamp(PlayerCount, 1, 4);
	return bBoss ? 200.0f + 100.0f * PartySize : 70.0f + 10.0f * PartySize;
}

float ADSTREnemyCharacter::GetConfiguredAttackPower(const bool bBoss)
{
	return bBoss ? 12.0f : 6.0f;
}

float ADSTREnemyCharacter::GetSpawnGroundClearance(const bool bBoss)
{
	const ADSTREnemyCharacter* Defaults = GetDefault<ADSTREnemyCharacter>();
	const float HalfHeight = Defaults && Defaults->GetCapsuleComponent()
		? Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 88.0f;
	return HalfHeight * GetActorScaleForRole(bBoss) + 10.0f;
}

void ADSTREnemyCharacter::InitializeAbilitySystem()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

void ADSTREnemyCharacter::ConfigureAsBoss(const bool bInBoss)
{
	// 보스 전환과 전투 수치는 서버만 확정한다.
	if (!HasAuthority())
	{
		return;
	}

	bIsBoss = bInBoss;
	ApplyConfiguredStats();
	if (bIsBoss && AbilitySystemComponent)
	{
		for (const TSubclassOf<UGameplayAbility> AbilityClass : {
			TSubclassOf<UGameplayAbility>(UDSTRBossColossalBlowAbility::StaticClass()),
			TSubclassOf<UGameplayAbility>(UDSTRBossPhantomRushAbility::StaticClass()),
			TSubclassOf<UGameplayAbility>(UDSTRBossSiphonAbility::StaticClass()),
			TSubclassOf<UGameplayAbility>(UDSTRBossSubjugateAbility::StaticClass())})
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
		}
	}
	OnRep_Boss();
	ForceNetUpdate();
}

void ADSTREnemyCharacter::SetDormant(const bool bNewDormant)
{
	if (!HasAuthority() || bDormant == bNewDormant)
	{
		return;
	}
	bDormant = bNewDormant;
	ForceNetUpdate();
}

void ADSTREnemyCharacter::Wake(const TCHAR* Reason)
{
	if (!HasAuthority() || !bDormant || bIsDead)
	{
		return;
	}
	bDormant = false;
	ForceNetUpdate();
	PlayWakeEffect();
	UE_LOG(LogDSTR, Log, TEXT("DSTR_ENEMY_WAKE Enemy=%s Boss=%s Reason=%s At=%s"),
		*GetName(), bIsBoss ? TEXT("true") : TEXT("false"), Reason, *GetActorLocation().ToCompactString());
	OnEnemyAwakened.Broadcast(this);
}

void ADSTREnemyCharacter::SetWarningCue(
	const FGameplayTag& CueTag,
	const bool bActive,
	const FVector& Location,
	const float Radius)
{
	if (!HasAuthority() || !AbilitySystemComponent || HasWarningCue(CueTag) == bActive)
	{
		return;
	}
	if (bActive)
	{
		FGameplayCueParameters Parameters;
		Parameters.Location = Location;
		Parameters.RawMagnitude = Radius;
		Parameters.Instigator = this;
		AbilitySystemComponent->AddGameplayCue(CueTag, Parameters);
	}
	else
	{
		AbilitySystemComponent->RemoveGameplayCue(CueTag);
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_CUE_SET Cue=%s Active=%d Enemy=%s"),
		*CueTag.ToString(), bActive ? 1 : 0, *GetName());
	ForceNetUpdate();
}

bool ADSTREnemyCharacter::HasWarningCue(const FGameplayTag& CueTag) const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(CueTag);
}

bool ADSTREnemyCharacter::IsPreparingAreaAttack() const
{
	return HasWarningCue(DSTRGameplayTags::GameplayCue_DSTR_EnemyTelegraph.GetTag());
}

bool ADSTREnemyCharacter::IsPreparingMelee() const
{
	return HasWarningCue(DSTRGameplayTags::GameplayCue_DSTR_BossWindup.GetTag());
}

void ADSTREnemyCharacter::SetPreparingAreaAttack(const bool bPreparing, const FVector& Center)
{
	const FVector Floor = Center - FVector(
		0.0f, 0.0f,
		GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - FDSTRCombatPresentation::TelegraphFloorClearance);
	SetWarningCue(
		DSTRGameplayTags::GameplayCue_DSTR_EnemyTelegraph.GetTag(),
		bPreparing,
		Floor,
		FDSTRCombatPresentation::BossAreaDamageRadius);
}

void ADSTREnemyCharacter::SetPreparingMelee(const bool bPreparing)
{
	SetWarningCue(
		DSTRGameplayTags::GameplayCue_DSTR_BossWindup.GetTag(),
		bPreparing,
		GetActorLocation(),
		FDSTRCombatPresentation::GetProfile(EDSTRCombatAction::BossMelee).HitReach);
}

void ADSTREnemyCharacter::SetRushing(const bool bNewRushing)
{
	SetWarningCue(
		DSTRGameplayTags::GameplayCue_DSTR_BossRush.GetTag(),
		bNewRushing,
		GetActorLocation(),
		FDSTRBossSkillRules::RushSweepRadius);
}

bool ADSTREnemyCharacter::IsRushing() const
{
	return HasWarningCue(DSTRGameplayTags::GameplayCue_DSTR_BossRush.GetTag());
}

void ADSTREnemyCharacter::SetSiphoning(const bool bNewSiphoning, const FVector& TelegraphLocation)
{
	SetWarningCue(
		DSTRGameplayTags::GameplayCue_DSTR_BossSiphon.GetTag(),
		bNewSiphoning,
		TelegraphLocation,
		FDSTRBossSkillRules::SiphonMinPullDistance);
}

bool ADSTREnemyCharacter::IsSiphoning() const
{
	return HasWarningCue(DSTRGameplayTags::GameplayCue_DSTR_BossSiphon.GetTag());
}

void ADSTREnemyCharacter::ApplyStun(const ADediServerRPGCharacter* Source)
{
	if (!HasAuthority() || bIsDead || !AbilitySystemComponent)
	{
		return;
	}
	AbilitySystemComponent->CancelAbilities();
	SetPreparingAreaAttack(false);
	SetPreparingMelee(false);
	AbilitySystemComponent->ApplyGameplayEffectToSelf(
		GetDefault<UDSTRStunEffect>(), 1.0f, AbilitySystemComponent->MakeEffectContext());
	GetCharacterMovement()->StopMovementImmediately();
	EndSiphonPull();
	SetRushing(false);
	SetSiphoning(false);
	UE_LOG(LogDSTR, Log, TEXT("DSTR_STUN Source=%s Target=%s Seconds=%.1f"),
		*GetNameSafe(Source), *GetName(), FDSTRDamageRules::ReckoningStunSeconds);
	if (bIsBoss)
	{
		const FDSTRCombatPresentation::FStunSegments Segments =
			FDSTRCombatPresentation::GetStunSegments(FDSTRDamageRules::ReckoningStunSeconds);
		StartCombatAction(EDSTRCombatAction::BossStun, 0);
		GetWorldTimerManager().SetTimer(
			StunSegmentTimerHandle, this, &ADSTREnemyCharacter::PlayStunLoop,
			FMath::Max(KINDA_SMALL_NUMBER, Segments.StartSeconds), false);
	}
}

void ADSTREnemyCharacter::PlayStunLoop()
{
	if (bIsDead)
	{
		return;
	}
	const FDSTRCombatPresentation::FStunSegments Segments =
		FDSTRCombatPresentation::GetStunSegments(FDSTRDamageRules::ReckoningStunSeconds);
	if (Segments.LoopSeconds > KINDA_SMALL_NUMBER)
	{
		StartCombatAction(EDSTRCombatAction::BossStun, 1);
	}
	GetWorldTimerManager().SetTimer(
		StunSegmentTimerHandle, this, &ADSTREnemyCharacter::PlayStunEnd,
		FMath::Max(KINDA_SMALL_NUMBER, Segments.LoopSeconds), false);
}

void ADSTREnemyCharacter::PlayStunEnd()
{
	if (!bIsDead)
	{
		StartCombatAction(EDSTRCombatAction::BossStun, 2);
	}
}

void ADSTREnemyCharacter::ApplyThrown(const ADediServerRPGCharacter* Source, const float Seconds)
{
	if (!HasAuthority() || bIsDead || !AbilitySystemComponent)
	{
		return;
	}
	AbilitySystemComponent->CancelAbilities();
	SetPreparingAreaAttack(false);
	SetPreparingMelee(false);
	EndSiphonPull();
	SetRushing(false);
	SetSiphoning(false);
	UDSTRCombatLibrary::ApplyStagger(
		Source ? Source->GetAbilitySystemComponent() : nullptr, AbilitySystemComponent, Seconds);
	if (bIsBoss)
	{
		StartCombatAction(EDSTRCombatAction::BossKnockback);
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_THROWN Source=%s Target=%s Seconds=%.1f"),
		*GetNameSafe(Source), *GetName(), Seconds);
}

void ADSTREnemyCharacter::BeginSiphonPull(ADediServerRPGCharacter* Target, const float Seconds)
{
	if (!HasAuthority() || !Target || Target->IsDowned() || bIsDead || Seconds <= 0.0f || !GetWorld())
	{
		return;
	}
	SiphonTarget = Target;

	FGameplayAbilityTargetData_LocationInfo* Destination = new FGameplayAbilityTargetData_LocationInfo();
	Destination->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	Destination->TargetLocation.LiteralTransform = FTransform(FDSTRBossSkillRules::PullStep(
		Target->GetActorLocation(),
		GetActorLocation(),
		FDSTRBossSkillRules::PullSpeed(),
		Seconds,
		FDSTRBossSkillRules::SiphonMinPullDistance));

	FGameplayEventData Payload;
	Payload.EventTag = DSTRGameplayTags::Event_Combat_Pulled.GetTag();
	Payload.Instigator = this;
	Payload.Target = Target;
	Payload.EventMagnitude = Seconds;
	Payload.TargetData.Add(Destination);
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Target, Payload.EventTag, Payload);
}

void ADSTREnemyCharacter::EndSiphonPull()
{
	if (const ADediServerRPGCharacter* Target = SiphonTarget.Get())
	{
		if (UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent())
		{
			FGameplayTagContainer PullTags;
			PullTags.AddTag(DSTRGameplayTags::Ability_Reaction_Pulled.GetTag());
			TargetASC->CancelAbilities(&PullTags);
		}
	}
	SiphonTarget.Reset();
}

bool ADSTREnemyCharacter::IsStunned() const
{
	return AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(DSTRGameplayTags::State_Stunned.GetTag());
}

void ADSTREnemyCharacter::AddThreat(const ADediServerRPGCharacter* Source, const float Amount)
{
	const UWorld* World = GetWorld();
	if (!HasAuthority() || !Source || !World || Amount <= 0.0f)
	{
		return;
	}
	if (!bIsBoss)
	{
		Wake(TEXT("Damage"));
	}
	const double Now = World->GetTimeSeconds();
	FDSTRThreatEntry& Entry = ThreatTable.FindOrAdd(Source);
	Entry.Threat = FDSTREnemyAIRules::DecayThreat(
		Entry.Threat, static_cast<float>(Now - Entry.UpdatedTime)) + Amount;
	Entry.UpdatedTime = Now;
}

float ADSTREnemyCharacter::GetThreat(const ADediServerRPGCharacter* Source) const
{
	const UWorld* World = GetWorld();
	const FDSTRThreatEntry* Entry = Source ? ThreatTable.Find(Source) : nullptr;
	if (!Entry || !World)
	{
		return 0.0f;
	}
	return FDSTREnemyAIRules::DecayThreat(
		Entry->Threat, static_cast<float>(World->GetTimeSeconds() - Entry->UpdatedTime));
}

void ADSTREnemyCharacter::ApplyConfiguredStats()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const int32 PlayerCount = State ? State->PlayerArray.Num() : 1;
	const float MaxHealth = GetConfiguredMaxHealth(bIsBoss, PlayerCount);
	const float AttackPower = GetConfiguredAttackPower(bIsBoss);
	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		UDSTREnemyStatsEffect::StaticClass(), 1.0f, AbilitySystemComponent->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(DSTRGameplayTags::Effect_Init_MaxHealth.GetTag(), MaxHealth);
	SpecHandle.Data->SetSetByCallerMagnitude(DSTRGameplayTags::Effect_Init_AttackPower.GetTag(), AttackPower);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void ADSTREnemyCharacter::HandleOutOfHealth()
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	bIsDead = true;
	GetWorldTimerManager().ClearTimer(StunSegmentTimerHandle);
	SetPreparingAreaAttack(false);
	SetPreparingMelee(false);
	AbilitySystemComponent->CancelAllAbilities();
	AbilitySystemComponent->ApplyGameplayEffectToSelf(
		GetDefault<UDSTRDeadEffect>(),
		1.0f,
		AbilitySystemComponent->MakeEffectContext());
	GetCharacterMovement()->DisableMovement();
	ApplyDeadCollision();
	StartCombatAction(bIsBoss
		? EDSTRCombatAction::BossDeath
		: EDSTRCombatAction::EnemyDeath);
	OnEnemyDefeated.Broadcast(this);
	PlayAbilityFeedback(
		GetActorLocation(), bIsBoss ? 180.0f : 100.0f, EDSTRCombatFeedback::EnemyDeath, nullptr);
	ForceNetUpdate();

	UE_LOG(LogDSTR, Log, TEXT("DSTR_ENEMY_DEFEATED Enemy=%s Boss=%s"),
		*GetName(), bIsBoss ? TEXT("true") : TEXT("false"));
	SetLifeSpan(FDSTRCombatPresentation::GetCorpseLifetime(bIsBoss));
	BeginCorpse();
}

void ADSTREnemyCharacter::OnRep_Dead()
{
	ApplyDeadCollision();
	BeginCorpse();
}

void ADSTREnemyCharacter::ApplyDeadCollision()
{
	if (!bIsDead)
	{
		return;
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADSTREnemyCharacter::BeginCorpse()
{
	const UWorld* World = GetWorld();
	if (!bIsDead || !World || CorpseDeathTime >= 0.0
		|| !FDSTRVisualAssetRegistry::ShouldLoadVisualAssets(GetNetMode()))
	{
		return;
	}
	CorpseDeathTime = World->GetTimeSeconds();
	CorpseMeshRestZ = static_cast<float>(GetMesh()->GetRelativeLocation().Z);
	PrimaryActorTick.TickInterval = 0.0f;
}

void ADSTREnemyCharacter::TickCorpseSink()
{
	const UWorld* World = GetWorld();
	if (CorpseDeathTime < 0.0 || !World)
	{
		return;
	}
	const float Scale = FMath::Max(KINDA_SMALL_NUMBER, static_cast<float>(GetActorScale3D().Z));
	const float Sink = FDSTRCombatPresentation::CorpseSinkOffset(
		static_cast<float>(World->GetTimeSeconds() - CorpseDeathTime), bIsBoss);
	FVector Relative = GetMesh()->GetRelativeLocation();
	Relative.Z = CorpseMeshRestZ + Sink / Scale;
	GetMesh()->SetRelativeLocation(Relative);
}

ADediServerRPGCharacter* ADSTREnemyCharacter::FindNearestLivingPlayer(const float MaxRange) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	ADediServerRPGCharacter* BestTarget = nullptr;
	float BestDistanceSquared = FMath::Square(MaxRange);
	for (TActorIterator<ADediServerRPGCharacter> It(World); It; ++It)
	{
		ADediServerRPGCharacter* Candidate = *It;
		if (!Candidate || Candidate->IsDowned())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}
	return BestTarget;
}

void ADSTREnemyCharacter::PlayAbilityFeedback(
	const FVector_NetQuantize Location,
	const float Radius,
	const EDSTRCombatFeedback Feedback,
	AActor* InstigatorActor)
{
	if (HasAuthority())
	{
		// 일회성 타격 연출은 신뢰성 없는 멀티캐스트로 보낸다.
		MulticastEnemyFeedback(Location, Radius, Feedback, InstigatorActor);
	}
}

void ADSTREnemyCharacter::MulticastEnemyFeedback_Implementation(
	const FVector_NetQuantize Location,
	const float Radius,
	const EDSTRCombatFeedback Feedback,
	AActor* InstigatorActor)
{
	PlayEnemyFeedbackLocal(Location, Radius, Feedback, InstigatorActor);
}

void ADSTREnemyCharacter::PlayEnemyFeedbackLocal(
	const FVector Location,
	const float Radius,
	const EDSTRCombatFeedback Feedback,
	const AActor* InstigatorActor)
{
	if (!FDSTRCombatFeedbackPolicy::ShouldPresent(GetNetMode()))
	{
		return;
	}

	const FSoftObjectPath* EffectPath = nullptr;
	FVector Scale(0.3f);
	switch (Feedback)
	{
	case EDSTRCombatFeedback::EnemyDeath:
	case EDSTRCombatFeedback::BossImpact:
		EffectPath = &FDSTRVisualAssetRegistry::GetEnemyImpactEffect();
		Scale = FVector(FDSTRCombatPresentation::GetImpactBurstScale(Radius));
		break;
	case EDSTRCombatFeedback::HitDealt:
		EffectPath = &FDSTRVisualAssetRegistry::GetBasicAttackEffect();
		Scale = FVector(bIsBoss ? 0.3f : 0.22f);
		if (CurrentCombatAction == EDSTRCombatAction::None)
		{
			const FVector ToAttacker = InstigatorActor
				? InstigatorActor->GetActorLocation() - GetActorLocation() : FVector::ZeroVector;
			const EDSTRHitDirection HitDirection =
				FDSTRCombatPresentation::ResolveHitDirection(GetActorForwardVector(), ToAttacker);
			PlayCombatActionLocal(
				bIsBoss ? EDSTRCombatAction::BossHitReact : EDSTRCombatAction::EnemyHitReact,
				static_cast<uint8>(HitDirection));
		}
		break;
	default:
		break;
	}

	if (UParticleSystem* Effect = EffectPath
		? Cast<UParticleSystem>(EffectPath->ResolveObject()) : nullptr)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(), Effect, Location, FRotator::ZeroRotator, Scale, true);
	}

	FDSTRCombatFeedbackRequest Request;
	Request.Feedback = Feedback;
	Request.Location = Location;
	Request.Radius = Radius;
	Request.bBossVariant = bIsBoss;
	Request.InstigatorActor = InstigatorActor;
	Request.VictimActor = this;
	FDSTRCombatFeedbackPlayer::Play(GetWorld(), Request);
}

uint8 ADSTREnemyCharacter::StartCombatAction(const EDSTRCombatAction Action, const int32 ExplicitVariant)
{
	if (!HasAuthority())
	{
		return 0;
	}

	const int32 VariantCount = FDSTRCombatPresentation::GetVariantCount(Action);
	const uint8 Variant = ExplicitVariant >= 0
		? static_cast<uint8>(ExplicitVariant)
		: (VariantCount > 1 ? NextAttackVariant : 0);
	if (ExplicitVariant < 0 && VariantCount > 1)
	{
		NextAttackVariant = static_cast<uint8>((static_cast<int32>(Variant) + 1) % VariantCount);
	}
	// 데디 서버는 애니메이션 대신 동작 정보만 복제한다.
	++ReplicatedCombatAction.Sequence;
	ReplicatedCombatAction.Action = Action;
	ReplicatedCombatAction.Variant = Variant;
	GetWorldTimerManager().ClearTimer(ReplicatedCombatActionTimerHandle);
	const FDSTRCombatActionProfile& Profile = FDSTRCombatPresentation::GetProfile(Action);
	if (!Profile.bHoldLastFrame && Profile.RecoveryDuration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			ReplicatedCombatActionTimerHandle,
			this,
			&ADSTREnemyCharacter::ClearReplicatedCombatAction,
			Profile.RecoveryDuration,
			false);
	}
	ForceNetUpdate();
	if (GetNetMode() != NM_DedicatedServer)
	{
		PlayCombatActionLocal(Action, Variant);
	}
	return Variant;
}

void ADSTREnemyCharacter::OnRep_CombatAction()
{
	if (ReplicatedCombatAction.Action == EDSTRCombatAction::None)
	{
		FinishCombatAction();
		return;
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		PlayCombatActionLocal(ReplicatedCombatAction.Action, ReplicatedCombatAction.Variant);
	}
}

void ADSTREnemyCharacter::ClearReplicatedCombatAction()
{
	if (!HasAuthority())
	{
		return;
	}
	++ReplicatedCombatAction.Sequence;
	ReplicatedCombatAction.Action = EDSTRCombatAction::None;
	ForceNetUpdate();
}

void ADSTREnemyCharacter::PlayCombatActionLocal(const EDSTRCombatAction Action, const uint8 Variant)
{
	UWorld* World = GetWorld();
	if (!World || Action == EDSTRCombatAction::None)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (CurrentCombatAction == Action && Now - CombatActionStartTime < 0.2)
	{
		return;
	}
	if (!GetMesh()->GetAnimInstance() && !GetMesh()->GetSingleNodeInstance())
	{
		return;
	}

	const FDSTRCombatActionProfile& Profile = FDSTRCombatPresentation::GetProfile(Action);
	if (!FDSTRCombatPresentation::PlayAction(*GetMesh(), Action, Variant))
	{
		if (FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(this))
		{
			UE_LOG(LogDSTR, Warning,
				TEXT("Enemy combat animation unavailable. Enemy=%s Action=%d"),
				*GetName(), static_cast<int32>(Action));
		}
		return;
	}

	CurrentCombatAction = Action;
	CurrentCombatVariant = Variant;
	CombatActionStartTime = Now;
#if !UE_BUILD_SHIPPING
	if (!HasAuthority() && UE_LOG_ACTIVE(LogDSTR, Verbose)
		&& (Action == EDSTRCombatAction::EnemyMelee || Action == EDSTRCombatAction::BossMelee))
	{
		if (const APlayerController* LocalPC = World->GetFirstPlayerController(); LocalPC && LocalPC->GetPawn())
		{
			const FVector ToLocal = (LocalPC->GetPawn()->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			const float YawDelta = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
				FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToLocal), -1.0f, 1.0f)));
			UE_LOG(LogDSTR, Verbose, TEXT("DSTR_CLIENT_SWING Enemy=%s YawToLocal=%.0f Dist=%.0f MeshYaw=%.0f ActorYaw=%.0f"),
				*GetName(), YawDelta, FVector::Dist2D(LocalPC->GetPawn()->GetActorLocation(), GetActorLocation()),
				GetMesh()->GetComponentRotation().Yaw, GetActorRotation().Yaw);
		}
	}
#endif
	GetWorldTimerManager().ClearTimer(CombatActionTimerHandle);
	if (!Profile.bHoldLastFrame && Profile.RecoveryDuration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			CombatActionTimerHandle,
			this,
			&ADSTREnemyCharacter::FinishCombatAction,
			Profile.RecoveryDuration,
			false);
	}
}

void ADSTREnemyCharacter::FinishCombatAction()
{
	CurrentCombatAction = EDSTRCombatAction::None;
	CombatActionStartTime = -1.0;
	if (FDSTRVisualAssetRegistry::ShouldLoadVisualAssets(GetNetMode()))
	{
		FDSTRVisualAssetRegistry::UpdateMovementAnimation(
			*GetMesh(),
			bIsBoss ? EDSTRVisualRole::Boss : EDSTRVisualRole::Enemy,
			GetVelocity(),
			GetActorForwardVector());
	}
}

void ADSTREnemyCharacter::HandleAnimationImpact(const UAnimSequenceBase* Animation)
{
	UE_LOG(LogDSTR, Verbose, TEXT("DSTR_ANIM_IMPACT Enemy=%s Action=%d Variant=%d"),
		*GetName(), static_cast<int32>(CurrentCombatAction), CurrentCombatVariant);
}

void ADSTREnemyCharacter::OnRep_Boss()
{
	ApplyVisualAssets();
	SetActorScale3D(FVector(GetActorScaleForRole(bIsBoss)));
}

void ADSTREnemyCharacter::ApplyVisualAssets()
{
	if (!FDSTRVisualAssetRegistry::ShouldLoadVisualAssets(GetNetMode()))
	{
		return;
	}
	if (!FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(this))
	{
		return;
	}

	const EDSTRVisualRole VisualRole = bIsBoss ? EDSTRVisualRole::Boss : EDSTRVisualRole::Enemy;
	if (!FDSTRVisualAssetRegistry::ApplyCharacterVisual(*GetMesh(), VisualRole))
	{
		UE_LOG(LogDSTR, Warning,
			TEXT("Enemy presentation assets are unavailable. Enemy=%s Boss=%s"),
			*GetName(), bIsBoss ? TEXT("true") : TEXT("false"));
		return;
	}

	// 복제 액션이 비주얼보다 먼저 도착했으면 준비 직후 재생한다.
	if (ReplicatedCombatAction.Action != EDSTRCombatAction::None)
	{
		CurrentCombatAction = EDSTRCombatAction::None;
		CombatActionStartTime = -1.0;
		PlayCombatActionLocal(ReplicatedCombatAction.Action, ReplicatedCombatAction.Variant);
	}
}

void ADSTREnemyCharacter::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADSTREnemyCharacter, bIsBoss);
	DOREPLIFETIME(ADSTREnemyCharacter, bIsDead);
	DOREPLIFETIME(ADSTREnemyCharacter, bDormant);
	DOREPLIFETIME(ADSTREnemyCharacter, ReplicatedCombatAction);
}
