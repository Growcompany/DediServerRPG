// Copyright Epic Games, Inc. All Rights Reserved.

#include "DediServerRPGCharacter.h"
#include "DSTRLog.h"
#include "DediServerRPGGameMode.h"
#include "Engine/LocalPlayer.h"
#include "Game/DSTRActorRegistry.h"
#include "Game/DSTRDungeonRules.h"
#include "Game/DSTRGameState.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Player/DSTRBotDriverComponent.h"
#include "Player/DSTREvidenceCaptureComponent.h"
#include "Player/DSTRHeroMovementComponent.h"
#include "Player/DSTRHeroMovementTuning.h"
#include "Player/DSTRPlayerController.h"
#include "Player/DSTRPlayerState.h"
#include "AbilitySystem/DSTRAbilitySystemComponent.h"
#include "AbilitySystem/DSTRGameplayEffects.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "AbilitySystem/DSTRAttributeSet.h"
#include "Combat/DSTRCombatActionReconciliation.h"
#include "Combat/DSTRCombatLibrary.h"
#include "Combat/DSTRDamageRules.h"
#include "EngineUtils.h"
#include "Enemy/DSTREnemyCharacter.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Presentation/DSTRVisualAssetRegistry.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "World/DSTRAttackBuffPickup.h"


ADediServerRPGCharacter::ADediServerRPGCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDSTRHeroMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	CurrentCombatAction = EDSTRCombatAction::None;
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, FDSTRHeroMovementTuning::RotationRateYaw, 0.0f);

	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = FDSTRHeroMovementTuning::MaxWalkSpeed;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->MaxAcceleration = FDSTRHeroMovementTuning::MaxAcceleration;
	GetCharacterMovement()->BrakingDecelerationWalking = FDSTRHeroMovementTuning::BrakingDecelerationWalking;
	GetCharacterMovement()->GroundFriction = FDSTRHeroMovementTuning::GroundFriction;
	GetCharacterMovement()->bUseSeparateBrakingFriction = FDSTRHeroMovementTuning::bUseSeparateBrakingFriction;
	GetCharacterMovement()->BrakingFriction = FDSTRHeroMovementTuning::BrakingFriction;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 60.0f);

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
	FollowCamera->PostProcessSettings.MotionBlurAmount = 0.25f;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultMappingContextAsset(
		TEXT("/Game/DediServerRPG/Input/IMC_Default.IMC_Default"));
	static ConstructorHelpers::FObjectFinder<UInputAction> JumpActionAsset(
		TEXT("/Game/DediServerRPG/Input/Actions/IA_Jump.IA_Jump"));
	static ConstructorHelpers::FObjectFinder<UInputAction> MoveActionAsset(
		TEXT("/Game/DediServerRPG/Input/Actions/IA_Move.IA_Move"));
	static ConstructorHelpers::FObjectFinder<UInputAction> LookActionAsset(
		TEXT("/Game/DediServerRPG/Input/Actions/IA_Look.IA_Look"));
	DefaultMappingContext = DefaultMappingContextAsset.Object;
	JumpAction = JumpActionAsset.Object;
	MoveAction = MoveActionAsset.Object;
	LookAction = LookActionAsset.Object;

	BasicAttackAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_BasicAttack"));
	BasicAttackAction->ValueType = EInputActionValueType::Boolean;
	MakeWayAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_MakeWay"));
	MakeWayAction->ValueType = EInputActionValueType::Boolean;
	FortifyAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Fortify"));
	FortifyAction->ValueType = EInputActionValueType::Boolean;
	ChargeAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Charge"));
	ChargeAction->ValueType = EInputActionValueType::Boolean;
	ReckoningAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Reckoning"));
	ReckoningAction->ValueType = EInputActionValueType::Boolean;
	InteractAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Interact"));
	InteractAction->ValueType = EInputActionValueType::Boolean;

	CombatMappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_DSTRCombat"));
	CombatMappingContext->MapKey(BasicAttackAction, EKeys::LeftMouseButton);
	CombatMappingContext->MapKey(MakeWayAction, EKeys::RightMouseButton);
	CombatMappingContext->MapKey(FortifyAction, EKeys::Q);
	CombatMappingContext->MapKey(ChargeAction, EKeys::E);
	CombatMappingContext->MapKey(ReckoningAction, EKeys::R);
	CombatMappingContext->MapKey(InteractAction, EKeys::F);

}

void ADediServerRPGCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UDSTRActorRegistry* Registry = UDSTRActorRegistry::Get(GetWorld()))
	{
		Registry->RegisterHero(this);
	}

	FDSTRVisualAssetRegistry::PreloadVisualAssets(
		this,
		[WeakThis = TWeakObjectPtr<ADediServerRPGCharacter>(this)]()
	{
		if (ADediServerRPGCharacter* Character = WeakThis.Get();
			Character && !FDSTRVisualAssetRegistry::ApplyCharacterVisual(
				*Character->GetMesh(), EDSTRVisualRole::Player))
		{
			UE_LOG(LogDSTR, Warning, TEXT("DSTR_PLAYER_VISUAL_MISSING Actor=%s"), *Character->GetName());
		}
	});
}

void ADediServerRPGCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearWarningEffect();
	ClearBuffEffect();
	if (UDSTRActorRegistry* Registry = UDSTRActorRegistry::Get(GetWorld()))
	{
		Registry->UnregisterHero(this);
	}
	RemoveInputMappings();
	Super::EndPlay(EndPlayReason);
}

void ADediServerRPGCharacter::UnPossessed()
{
	RemoveInputMappings();
	Super::UnPossessed();
}

void ADediServerRPGCharacter::RemoveInputMappings()
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = InputSubsystem.Get())
	{
		Subsystem->RemoveMappingContext(CombatMappingContext);
		Subsystem->RemoveMappingContext(DefaultMappingContext);
	}
	InputSubsystem.Reset();
}

void ADediServerRPGCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (IsLocallyControlled() && Controller)
	{
		FRotator Look = Controller->GetControlRotation();
		Look.Pitch = DefaultCameraPitch;
		Controller->SetControlRotation(Look);
	}

#if !UE_BUILD_SHIPPING
	// 봇과 캡처 도구는 개발 빌드의 로컬 캐릭터에만 붙인다.
	const FString& CommandLine = FCommandLine::Get();
	if (UDSTRBotDriverComponent::ShouldAttach(CommandLine, IsLocallyControlled()))
	{
		NewObject<UDSTRBotDriverComponent>(this)->RegisterComponent();
	}
	if (UDSTREvidenceCaptureComponent::ShouldAttach(CommandLine, !HasAuthority() && IsLocallyControlled()))
	{
		NewObject<UDSTREvidenceCaptureComponent>(this)->RegisterComponent();
	}
#endif
}

void ADediServerRPGCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickFallRecovery();
	if (!DesiredMoveDirection.IsNearlyZero())
	{
		AddMovementInput(DesiredMoveDirection, 1.0f);
	}
	if (IsLocallyControlled() && FollowCamera)
	{
		const float CameraDistance = FVector::Dist(
			FollowCamera->GetComponentLocation(), GetActorLocation());
		GetMesh()->SetOwnerNoSee(ShouldHideOwnerMesh(CameraDistance));
	}
	TickBossCamera(DeltaSeconds);
	if (FDSTRVisualAssetRegistry::ShouldLoadVisualAssets(GetNetMode())
		&& CurrentCombatAction == EDSTRCombatAction::None)
	{
		FDSTRVisualAssetRegistry::UpdateMovementAnimation(
			*GetMesh(), EDSTRVisualRole::Player, GetVelocity(), GetActorForwardVector());
	}
}

void ADediServerRPGCharacter::TickBossCamera(const float DeltaSeconds)
{
	if (!CameraBoom || !IsLocallyControlled()
		|| !FDSTRVisualAssetRegistry::ShouldLoadVisualAssets(GetNetMode()))
	{
		return;
	}
	const ADSTRGameState* State = GetWorld() ? GetWorld()->GetGameState<ADSTRGameState>() : nullptr;
	const ADSTREnemyCharacter* Boss = State ? State->GetBoss() : nullptr;
	const bool bNearBoss = Boss && !Boss->IsCombatantDead() && !Boss->IsDormant()
		&& FVector::Dist(Boss->GetActorLocation(), GetActorLocation()) <= FDSTRDungeonRules::BossCameraRange;
	CameraBoom->TargetArmLength = FDSTRDungeonRules::StepCameraArmLength(
		CameraBoom->TargetArmLength, bNearBoss, DeltaSeconds);
}

UAbilitySystemComponent* ADediServerRPGCharacter::GetAbilitySystemComponent() const
{
	const ADSTRPlayerState* DSTRPlayerState = GetPlayerState<ADSTRPlayerState>();
	return DSTRPlayerState ? DSTRPlayerState->GetAbilitySystemComponent() : nullptr;
}

void ADediServerRPGCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystem();
}

void ADediServerRPGCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilitySystem();
}

void ADediServerRPGCharacter::InitializeAbilitySystem()
{
	ADSTRPlayerState* DSTRPlayerState = GetPlayerState<ADSTRPlayerState>();
	if (!DSTRPlayerState)
	{
		return;
	}

	if (UDSTRAbilitySystemComponent* ASC = DSTRPlayerState->GetDSTRAbilitySystemComponent())
	{
		ASC->InitAbilityActorInfo(DSTRPlayerState, this);
		// 두 초기화 경로가 겹쳐도 델리게이트는 하나만 유지한다.
		if (UAbilitySystemComponent* PreviousASC = BoundAbilitySystemComponent.Get();
			PreviousASC && DeadTagChangedHandle.IsValid())
		{
			PreviousASC->RegisterGameplayTagEvent(
				DSTRGameplayTags::State_Dead.GetTag(), EGameplayTagEventType::NewOrRemoved)
				.Remove(DeadTagChangedHandle);
		}
		DeadTagChangedHandle = ASC->RegisterGameplayTagEvent(
			DSTRGameplayTags::State_Dead.GetTag(), EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ADediServerRPGCharacter::HandleDeadTagChanged);
		BoundAbilitySystemComponent = ASC;
		if (HasAuthority())
		{
			ASC->GiveStartupAbilities();
			if (ASC->TryMarkStartupEffectsApplied())
			{
				ASC->ApplyGameplayEffectToSelf(
					GetDefault<UDSTRStaminaRegenEffect>(),
					1.0f,
					ASC->MakeEffectContext());
			}
		}
		if (IsDowned())
		{
			ApplyDownedPresentation();
		}
		UE_LOG(LogDSTR, Log,
			TEXT("GAS initialized. Owner=%s Avatar=%s Authority=%s"),
			*GetNameSafe(DSTRPlayerState),
			*GetNameSafe(this),
			HasAuthority() ? TEXT("Server") : TEXT("Client"));
	}
}

bool ADediServerRPGCharacter::IsDowned() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(DSTRGameplayTags::State_Dead.GetTag());
}

bool ADediServerRPGCharacter::IsEliminated() const
{
	const ADSTRPlayerState* DSTRPlayerState = GetPlayerState<ADSTRPlayerState>();
	return DSTRPlayerState && DSTRPlayerState->IsEliminated();
}

float ADediServerRPGCharacter::GetBleedOutRemaining() const
{
	const ADSTRPlayerState* DSTRPlayerState = GetPlayerState<ADSTRPlayerState>();
	return DSTRPlayerState ? DSTRPlayerState->GetBleedOutRemaining() : 0.0f;
}

void ADediServerRPGCharacter::HandleDeadTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
	ApplyDownedPresentation();
	// 다운 태그와 출혈 시계는 같은 변경 지점에서 맞춘다.
	if (ADSTRPlayerState* DSTRPlayerState = HasAuthority() ? GetPlayerState<ADSTRPlayerState>() : nullptr)
	{
		if (NewCount > 0)
		{
			DSTRPlayerState->StartBleedOut();
		}
		else
		{
			DSTRPlayerState->ClearBleedOut();
		}
	}
	if (NewCount > 0 && GetNetMode() != NM_DedicatedServer)
	{
		PlayCombatActionLocal(EDSTRCombatAction::Downed, 0);
	}
}

void ADediServerRPGCharacter::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADediServerRPGCharacter, ReplicatedCombatAction);
}

void ADediServerRPGCharacter::HandleOutOfHealth()
{
	if (!HasAuthority() || IsDowned())
	{
		return;
	}

	UDSTRAbilitySystemComponent* ASC = Cast<UDSTRAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!ASC)
	{
		return;
	}

	ASC->SetDeadEffectHandle(ASC->ApplyGameplayEffectToSelf(
		GetDefault<UDSTRDeadEffect>(),
		1.0f,
		ASC->MakeEffectContext()));
	StartCombatAction(EDSTRCombatAction::Downed);
	PlayAbilityFeedback(
		EDSTRCombatFeedback::Downed,
		GetActorLocation(),
		GetActorForwardVector(),
		120.0f);
	ForceNetUpdate();

	UE_LOG(LogDSTR, Log, TEXT("DSTR_DOWNED Player=%s"), *GetName());
	if (ADediServerRPGGameMode* GameMode = GetWorld()->GetAuthGameMode<ADediServerRPGGameMode>())
	{
		GameMode->HandlePlayerDowned(this);
	}
}

bool ADediServerRPGCharacter::CanBeRevivedBy(
	const ADediServerRPGCharacter* Reviver) const
{
	return Reviver && UDSTRCombatLibrary::IsReviveRequestValid(
		Reviver == this,
		Reviver->IsDowned(),
		IsDowned(),
		IsEliminated(),
		FVector::Dist(Reviver->GetActorLocation(), GetActorLocation()));
}

ADediServerRPGCharacter* ADediServerRPGCharacter::FindNearestReviveTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	ADediServerRPGCharacter* BestTarget = nullptr;
	float BestDistanceSquared = FMath::Square(InteractRange);
	for (TActorIterator<ADediServerRPGCharacter> It(World); It; ++It)
	{
		ADediServerRPGCharacter* Candidate = *It;
		if (!Candidate || !Candidate->CanBeRevivedBy(this))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			GetActorLocation(),
			Candidate->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared)
		{
			BestTarget = Candidate;
			BestDistanceSquared = DistanceSquared;
		}
	}
	return BestTarget;
}

bool ADediServerRPGCharacter::TryReviveNearestFromServer()
{
	if (!HasAuthority() || IsDowned())
	{
		return false;
	}

	if (ADediServerRPGCharacter* Target = FindNearestReviveTarget())
	{
		return Target->ReviveFromServer(this);
	}
	return false;
}

bool ADediServerRPGCharacter::ReviveFromServer(ADediServerRPGCharacter* Reviver)
{
	if (!HasAuthority() || !CanBeRevivedBy(Reviver))
	{
		return false;
	}

	UDSTRAbilitySystemComponent* ASC = Cast<UDSTRAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!ASC)
	{
		return false;
	}

	const FActiveGameplayEffectHandle DeadEffectHandle = ASC->GetDeadEffectHandle();
	if (DeadEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(DeadEffectHandle);
	}
	else
	{
		FGameplayTagContainer DeadTags;
		DeadTags.AddTag(DSTRGameplayTags::State_Dead.GetTag());
		ASC->RemoveActiveEffectsWithGrantedTags(DeadTags);
	}
	ASC->ClearDeadEffectHandle();
	ASC->ApplyGameplayEffectToSelf(
		GetDefault<UDSTRReviveEffect>(),
		1.0f,
		ASC->MakeEffectContext());
	ASC->ApplyGameplayEffectToSelf(
		GetDefault<UDSTRReviveProtectionEffect>(),
		1.0f,
		ASC->MakeEffectContext());
	StartCombatAction(EDSTRCombatAction::Revived);
	PlayAbilityFeedback(
		EDSTRCombatFeedback::Revived,
		GetActorLocation(),
		GetActorForwardVector(),
		140.0f);
	ForceNetUpdate();

	UE_LOG(LogDSTR, Log,
		TEXT("DSTR_REVIVE Reviver=%s Target=%s"),
		*GetNameSafe(Reviver),
		*GetName());
	if (ADSTRGameState* State = GetWorld()->GetGameState<ADSTRGameState>())
	{
		State->RecordRevived(
			Reviver->GetPlayerState() ? Reviver->GetPlayerState()->GetPlayerName() : Reviver->GetName(),
			GetPlayerState() ? GetPlayerState()->GetPlayerName() : GetName());
	}
	return true;
}

void ADediServerRPGCharacter::ApplyDownedPresentation()
{
	if (IsDowned())
	{
		GetCharacterMovement()->DisableMovement();
	}
	else
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}

ADSTRAttackBuffPickup* ADediServerRPGCharacter::FindNearestPickupTarget() const
{
	UWorld* World = GetWorld();
	if (!World || IsDowned())
	{
		return nullptr;
	}

	ADSTRAttackBuffPickup* NearestPickup = nullptr;
	float NearestDistanceSquared = FMath::Square(InteractRange);
	for (TActorIterator<ADSTRAttackBuffPickup> It(World); It; ++It)
	{
		ADSTRAttackBuffPickup* Candidate = *It;
		if (!Candidate || Candidate->IsConsumed())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared <= NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestPickup = Candidate;
		}
	}
	return NearestPickup;
}

bool ADediServerRPGCharacter::TryInteractFromServer()
{
	if (!HasAuthority() || IsDowned())
	{
		return false;
	}

	if (TryReviveNearestFromServer())
	{
		return true;
	}

	ADSTRAttackBuffPickup* NearestPickup = FindNearestPickupTarget();
	if (NearestPickup && NearestPickup->TryConsume(this))
	{
		return true;
	}

	UE_LOG(LogDSTR, Verbose,
		TEXT("Interaction requested with no valid revive or pickup target. Player=%s"),
		*GetName());
	return false;
}

void ADediServerRPGCharacter::PlayAbilityFeedback(
	const EDSTRCombatFeedback Feedback,
	const FVector_NetQuantize Location,
	const FVector_NetQuantizeNormal Direction,
	const float Radius)
{
	if (HasAuthority())
	{
		MulticastCombatFeedback(Feedback, Location, Direction, Radius);
	}
}

void ADediServerRPGCharacter::MulticastCombatFeedback_Implementation(
	const EDSTRCombatFeedback Feedback,
	const FVector_NetQuantize Location,
	const FVector_NetQuantizeNormal Direction,
	const float Radius)
{
	if (!FDSTRCombatFeedbackPolicy::ShouldPresent(GetNetMode()))
	{
		return;
	}

	const FSoftObjectPath* EffectPath = nullptr;
	FVector Scale(0.25f);
	FVector SpawnLocation(Location);
	switch (Feedback)
	{
	case EDSTRCombatFeedback::BasicAttack:
		EffectPath = &FDSTRVisualAssetRegistry::GetBasicAttackEffect();
		break;
	case EDSTRCombatFeedback::Fortify:
		EffectPath = &FDSTRVisualAssetRegistry::GetFortifyEffect();
		Scale = FVector(0.4f);
		break;
	case EDSTRCombatFeedback::MakeWay:
	case EDSTRCombatFeedback::Charge:
		EffectPath = Feedback == EDSTRCombatFeedback::MakeWay
			? &FDSTRVisualAssetRegistry::GetMakeWayEffect()
			: &FDSTRVisualAssetRegistry::GetChargeEffect();
		Scale = FDSTRCombatPresentation::GetGroundRingScale3D(Radius);
		SpawnLocation.Z -= GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
			- FDSTRCombatPresentation::TelegraphFloorClearance;
		break;
	case EDSTRCombatFeedback::ReckoningWarning:
		EffectPath = &FDSTRVisualAssetRegistry::GetEnemyTelegraphEffect();
		Scale = FDSTRCombatPresentation::GetTelegraphScale3D(Radius);
		SpawnLocation.Z -= GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
			- FDSTRCombatPresentation::TelegraphFloorClearance;
		break;
	case EDSTRCombatFeedback::Reckoning:
		EffectPath = &FDSTRVisualAssetRegistry::GetReckoningEffect();
		Scale = FVector(FDSTRCombatPresentation::GetImpactBurstScale(Radius));
		SpawnLocation.Z -= GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		break;
	case EDSTRCombatFeedback::Downed:
		EffectPath = &FDSTRVisualAssetRegistry::GetDownedEffect();
		Scale = FVector(0.35f);
		break;
	case EDSTRCombatFeedback::Revived:
		EffectPath = &FDSTRVisualAssetRegistry::GetReviveEffect();
		Scale = FVector(0.3f);
		break;
	case EDSTRCombatFeedback::AttackBuff:
		EffectPath = &FDSTRVisualAssetRegistry::GetAttackBuffEffect();
		Scale = FVector(FDSTRCombatPresentation::AttackBuffEffectScale);
		break;
	case EDSTRCombatFeedback::EnemyTelegraph:
		EffectPath = &FDSTRVisualAssetRegistry::GetEnemyTelegraphEffect();
		Scale = FVector(0.45f);
		break;
	case EDSTRCombatFeedback::HitTaken:
		EffectPath = &FDSTRVisualAssetRegistry::GetBasicAttackEffect();
		Scale = FVector(0.18f);
		if (CurrentCombatAction == EDSTRCombatAction::None)
		{
			const EDSTRHitDirection HitDirection = FDSTRCombatPresentation::ResolveHitDirection(
				GetActorForwardVector(), -FVector(Direction));
			UE_LOG(LogDSTR, Verbose, TEXT("DSTR_HIT_DIRECTION Dir=%d"), static_cast<int32>(HitDirection));
			PlayCombatActionLocal(EDSTRCombatAction::HitReact, static_cast<uint8>(HitDirection));
		}
		break;
	default:
		break;
	}

	UParticleSystem* Effect = EffectPath
		? Cast<UParticleSystem>(EffectPath->ResolveObject()) : nullptr;
	if (Effect)
	{
		const FRotator Rotation = FVector(Direction).IsNearlyZero()
			? GetActorRotation() : FVector(Direction).Rotation();
		UParticleSystemComponent* Spawned = UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(), Effect, SpawnLocation, Rotation, Scale, true);
		if (Spawned && Feedback == EDSTRCombatFeedback::ReckoningWarning)
		{
			ClearWarningEffect();
			WarningEffectComponent = Spawned;
			GetWorldTimerManager().SetTimer(
				WarningEffectTimerHandle,
				this,
				&ADediServerRPGCharacter::ClearWarningEffect,
				FDSTRDamageRules::ReckoningWarningSeconds,
				false);
		}
		else if (Spawned && Feedback == EDSTRCombatFeedback::AttackBuff)
		{
			ClearBuffEffect();
			BuffEffectComponent = Spawned;
			GetWorldTimerManager().SetTimer(
				BuffEffectTimerHandle,
				this,
				&ADediServerRPGCharacter::ClearBuffEffect,
				FDSTRCombatPresentation::AttackBuffEffectSeconds,
				false);
		}
	}

	FDSTRCombatFeedbackRequest Request;
	Request.Feedback = Feedback;
	Request.Location = FVector(Location);
	Request.Direction = FVector(Direction);
	Request.Radius = Radius;
	Request.bSkipSound = Feedback == EDSTRCombatFeedback::BasicAttack;
	if (FDSTRCombatFeedbackPolicy::IsVictimFeedback(Feedback))
	{
		Request.VictimActor = this;
	}
	else
	{
		Request.InstigatorActor = this;
	}
	FDSTRCombatFeedbackPlayer::Play(GetWorld(), Request);
}

void ADediServerRPGCharacter::ClearWarningEffect()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarningEffectTimerHandle);
	}
	else
	{
		WarningEffectTimerHandle.Invalidate();
	}
	if (UParticleSystemComponent* Effect = WarningEffectComponent.Get())
	{
		Effect->DeactivateSystem();
	}
	WarningEffectComponent.Reset();
}

void ADediServerRPGCharacter::ClearBuffEffect()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BuffEffectTimerHandle);
	}
	else
	{
		BuffEffectTimerHandle.Invalidate();
	}
	if (UParticleSystemComponent* Effect = BuffEffectComponent.Get())
	{
		Effect->DeactivateSystem();
	}
	BuffEffectComponent.Reset();
}

uint8 ADediServerRPGCharacter::StartCombatAction(const EDSTRCombatAction Action)
{
	if (Controller && Controller->IsPlayerController()
		&& (Action == EDSTRCombatAction::BasicAttack
			|| Action == EDSTRCombatAction::Fortify
			|| Action == EDSTRCombatAction::ReckoningEntry
			|| Action == EDSTRCombatAction::Reckoning
			|| Action == EDSTRCombatAction::Charge))
	{
		SetActorRotation(FRotator(0.0f, Controller->GetControlRotation().Yaw, 0.0f));
	}
	const int32 VariantCount = FDSTRCombatPresentation::GetVariantCount(Action);
	uint8 Variant = 0;
	if (Action == EDSTRCombatAction::BasicAttack && VariantCount > 1)
	{
		Variant = FDSTRPredictedCombatActionReconciliation::NormalizeVariant(
			NextAttackVariant, VariantCount);
		NextAttackVariant = FDSTRPredictedCombatActionReconciliation::NextBasicAttackVariant(
			Variant, VariantCount);
	}
	// 서버는 동작 번호만 복제하고 애니메이션은 각 클라이언트가 재생한다.
	if (HasAuthority())
	{
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
				&ADediServerRPGCharacter::ClearReplicatedCombatAction,
				Profile.RecoveryDuration,
				false);
		}
		ForceNetUpdate();
		if (GetNetMode() != NM_DedicatedServer)
		{
			PlayCombatActionLocal(Action, Variant);
		}
	}
	else if (IsLocallyControlled())
	{
		PlayCombatActionLocal(Action, Variant);
	}
	return Variant;
}

void ADediServerRPGCharacter::OnRep_CombatAction()
{
	const int32 VariantCount = FDSTRCombatPresentation::GetVariantCount(ReplicatedCombatAction.Action);
	if (ReplicatedCombatAction.Action == EDSTRCombatAction::BasicAttack)
	{
		ReplicatedCombatAction.Variant =
			FDSTRPredictedCombatActionReconciliation::NormalizeVariant(
				ReplicatedCombatAction.Variant, VariantCount);
	}
	NextAttackVariant = FDSTRPredictedCombatActionReconciliation::ReconcileNextBasicAttackVariant(
		NextAttackVariant,
		ReplicatedCombatAction.Action,
		ReplicatedCombatAction.Variant,
		VariantCount);

	if (ReplicatedCombatAction.Action == EDSTRCombatAction::None)
	{
		FinishCombatAction();
		return;
	}

	// 예측과 서버 결과가 같으면 같은 몽타주를 다시 틀지 않는다.
	if (IsLocallyControlled() && CurrentCombatAction == ReplicatedCombatAction.Action)
	{
		const FDSTRCombatActionProfile& Profile =
			FDSTRCombatPresentation::GetProfile(CurrentCombatAction);
		const UWorld* World = GetWorld();
		const bool bWithinRecovery = World
			&& World->GetTimeSeconds() - CombatActionStartTime < Profile.RecoveryDuration;
		if (FDSTRPredictedCombatActionReconciliation::ShouldSuppressLocalReplay(
			CurrentCombatAction,
			CurrentCombatVariant,
			ReplicatedCombatAction.Action,
			ReplicatedCombatAction.Variant,
			bWithinRecovery))
		{
			return;
		}
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		PlayCombatActionLocal(ReplicatedCombatAction.Action, ReplicatedCombatAction.Variant);
	}
}

void ADediServerRPGCharacter::ClearReplicatedCombatAction()
{
	if (!HasAuthority())
	{
		return;
	}
	++ReplicatedCombatAction.Sequence;
	ReplicatedCombatAction.Action = EDSTRCombatAction::None;
	ForceNetUpdate();
}

void ADediServerRPGCharacter::PlayCombatActionLocal(const EDSTRCombatAction Action, const uint8 Variant)
{
	UWorld* World = GetWorld();
	if (!World || Action == EDSTRCombatAction::None)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (CurrentCombatAction == Action
		&& CurrentCombatVariant == Variant
		&& Now - CombatActionStartTime < 0.2)
	{
		return;
	}

	const FDSTRCombatActionProfile& Profile = FDSTRCombatPresentation::GetProfile(Action);
	if (!FDSTRCombatPresentation::PlayAction(*GetMesh(), Action, Variant))
	{
		if (FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(this))
		{
			UE_LOG(LogDSTR, Warning,
				TEXT("Combat animation unavailable. Player=%s Action=%d"),
				*GetName(), static_cast<int32>(Action));
		}
		return;
	}

	CurrentCombatAction = Action;
	CurrentCombatVariant = Variant;
	CombatActionStartTime = Now;
	GetWorldTimerManager().ClearTimer(CombatActionTimerHandle);
	if (!Profile.bHoldLastFrame && Profile.RecoveryDuration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			CombatActionTimerHandle,
			this,
			&ADediServerRPGCharacter::FinishCombatAction,
			Profile.RecoveryDuration,
			false);
	}
}

void ADediServerRPGCharacter::FinishCombatAction()
{
	CurrentCombatAction = EDSTRCombatAction::None;
	CombatActionStartTime = -1.0;
	if (FDSTRVisualAssetRegistry::ShouldLoadVisualAssets(GetNetMode()))
	{
		FDSTRVisualAssetRegistry::UpdateMovementAnimation(
			*GetMesh(), EDSTRVisualRole::Player, GetVelocity(), GetActorForwardVector());
	}
}

float ADediServerRPGCharacter::GetCombatActionProgress() const
{
	const UWorld* World = GetWorld();
	if (!World || CurrentCombatAction == EDSTRCombatAction::None || CombatActionStartTime < 0.0)
	{
		return 0.0f;
	}
	const float Window = FDSTRCombatPresentation::GetImpactDelay(CurrentCombatAction, CurrentCombatVariant);
	if (Window <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}
	return FMath::Clamp(static_cast<float>(World->GetTimeSeconds() - CombatActionStartTime) / Window, 0.0f, 1.0f);
}

void ADediServerRPGCharacter::HandleAnimationImpact(const UAnimSequenceBase* Animation)
{
	UE_LOG(LogDSTR, Verbose, TEXT("DSTR_ANIM_IMPACT Action=%d Variant=%d"),
		static_cast<int32>(CurrentCombatAction), CurrentCombatVariant);
	FDSTRCombatFeedbackRequest Request;
	Request.Feedback = EDSTRCombatFeedback::BasicAttack;
	Request.Location = GetActorLocation();
	Request.InstigatorActor = this;
	Request.bSoundOnly = true;
	FDSTRCombatFeedbackPlayer::Play(GetWorld(), Request);
}

void ADediServerRPGCharacter::TickFallRecovery()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	UWorld* World = GetWorld();
	if (!HasAuthority() || !Movement || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (Movement->IsMovingOnGround())
	{
		LastGroundedLocation = GetActorLocation();
		FallRecoveryRing.Sample(LastGroundedLocation, Now);
		FallStartTime = -1.0;
		return;
	}
	if (!Movement->IsFalling())
	{
		return;
	}
	if (FallStartTime < 0.0)
	{
		FallStartTime = Now;
	}
	const float FallSeconds = static_cast<float>(Now - FallStartTime);
	const FVector FallLocation = GetActorLocation();
	const float Depth = static_cast<float>(LastGroundedLocation.Z - FallLocation.Z);
	if (!FDSTRFallRecoveryRing::ShouldRecover(FallSeconds, Depth) || LastGroundedLocation.IsZero())
	{
		return;
	}

	FVector Target = FVector::ZeroVector;
	const TCHAR* Source = TEXT("Ring");
	if (!FallRecoveryRing.FindRecoveryPoint(FallLocation, Target))
	{
		Source = TEXT("Origin");
		const ADediServerRPGGameMode* GameMode = World->GetAuthGameMode<ADediServerRPGGameMode>();
		Target = GameMode ? GameMode->GetMatchOrigin() : FVector::ZeroVector;
		if (Target.IsZero())
		{
			Source = TEXT("LastGround");
			Target = LastGroundedLocation;
		}
	}

	// 순간이동 전에 이동형 루트 모션을 끊어 재낙하를 막는다.
	Movement->CurrentRootMotion.Clear();
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		FGameplayTagContainer ChargeTags;
		ChargeTags.AddTag(DSTRGameplayTags::Ability_Skill_Charge.GetTag());
		ChargeTags.AddTag(DSTRGameplayTags::Ability_Skill_MakeWay.GetTag());
		ChargeTags.AddTag(DSTRGameplayTags::Ability_Reaction_Pulled.GetTag());
		ASC->CancelAbilities(&ChargeTags);
	}
	const FVector Destination = Target + FVector(0.0f, 0.0f, 20.0f);
	if (!TeleportTo(Destination, GetActorRotation(), false, false))
	{
		const ADediServerRPGGameMode* GameMode = World->GetAuthGameMode<ADediServerRPGGameMode>();
		const FVector Origin = GameMode ? GameMode->GetMatchOrigin() : FVector::ZeroVector;
		Source = TEXT("OriginRetry");
		TeleportTo(Origin + FVector(0.0f, 0.0f, 20.0f), GetActorRotation(), false, true);
	}
	UE_LOG(LogDSTR, Warning,
		TEXT("DSTR_FALL_RECOVERED Player=%s From=%s To=%s Source=%s Ring=%d Downed=%d FallSeconds=%.1f Depth=%.0f"),
		*GetName(), *FallLocation.ToCompactString(), *Destination.ToCompactString(), Source,
		FallRecoveryRing.Num(), IsDowned() ? 1 : 0, FallSeconds, Depth);
	Movement->Velocity = FVector::ZeroVector;
	if (!IsDowned())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
	FallStartTime = -1.0;
}


void ADediServerRPGCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
			Subsystem->AddMappingContext(CombatMappingContext, 1);
			InputSubsystem = Subsystem;
		}
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADediServerRPGCharacter::Move);

		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADediServerRPGCharacter::Look);

		EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Started, this, &ADediServerRPGCharacter::HandleBasicAttackInput);
		EnhancedInputComponent->BindAction(MakeWayAction, ETriggerEvent::Started, this, &ADediServerRPGCharacter::HandleMakeWayInput);
		EnhancedInputComponent->BindAction(FortifyAction, ETriggerEvent::Started, this, &ADediServerRPGCharacter::HandleFortifyInput);
		EnhancedInputComponent->BindAction(ChargeAction, ETriggerEvent::Started, this, &ADediServerRPGCharacter::HandleChargeInput);
		EnhancedInputComponent->BindAction(ReckoningAction, ETriggerEvent::Started, this, &ADediServerRPGCharacter::HandleReckoningInput);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ADediServerRPGCharacter::HandleInteractInput);
	}
	else
	{
		UE_LOG(LogDSTR, Error,
			TEXT("DSTR_INPUT_COMPONENT_MISSING Actor=%s Expected=EnhancedInputComponent"),
			*GetNameSafe(this));
	}
}

void ADediServerRPGCharacter::PressAbilityInput(const FGameplayTag& InputTag)
{
	if (UDSTRAbilitySystemComponent* ASC = Cast<UDSTRAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void ADediServerRPGCharacter::HandleBasicAttackInput()
{
	PressAbilityInput(DSTRGameplayTags::InputTag_Ability_BasicAttack.GetTag());
}

void ADediServerRPGCharacter::HandleMakeWayInput()
{
	PressAbilityInput(DSTRGameplayTags::InputTag_Ability_MakeWay.GetTag());
}

void ADediServerRPGCharacter::HandleFortifyInput()
{
	PressAbilityInput(DSTRGameplayTags::InputTag_Ability_Fortify.GetTag());
}

void ADediServerRPGCharacter::HandleChargeInput()
{
	PressAbilityInput(DSTRGameplayTags::InputTag_Ability_Charge.GetTag());
}

void ADediServerRPGCharacter::HandleReckoningInput()
{
	PressAbilityInput(DSTRGameplayTags::InputTag_Ability_Reckoning.GetTag());
}

bool ADediServerRPGCharacter::IsAbilityReady(const FGameplayTag& CooldownTag) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && !ASC->HasMatchingGameplayTag(CooldownTag);
}

void ADediServerRPGCharacter::HandleInteractInput()
{
	PressAbilityInput(DSTRGameplayTags::InputTag_Ability_Revive.GetTag());
}

void ADediServerRPGCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	const UWorld* World = GetWorld();
	const bool bAbilityLocksMovement = ASC && World
		&& ASC->HasMatchingGameplayTag(DSTRGameplayTags::State_Attacking.GetTag())
		&& World->GetTimeSeconds() - CombatActionStartTime
			< FDSTRCombatPresentation::GetMovementLockDuration(CurrentCombatAction, CurrentCombatVariant);
	if (Controller != nullptr && !bAbilityLocksMovement)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ADediServerRPGCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}
