#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystem/DSTRAbilitySystemComponent.h"
#include "AbilitySystem/DSTRAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Player/DSTRPlayerState.h"
#include "Presentation/DSTRCombatPresentation.h"
#include "Presentation/DSTRVisualAssetRegistry.h"
#include "UObject/UnrealType.h"

namespace
{

enum class EPresentationOrder : uint8
{
	ActionBeforeVisual,
	VisualBeforeAction,
	ActionClearedBeforeVisual
};

struct FPresentationWorld
{
	UGameInstance* GameInstance = nullptr;
	UWorld* World = nullptr;
	ADediServerRPGCharacter* Hero = nullptr;

	FPresentationWorld()
	{
		GameInstance = NewObject<UGameInstance>(GEngine);
		GameInstance->AddToRoot();
		GameInstance->InitializeStandalone();
		World = GameInstance->GetWorld();
		World->InitializeActorsForPlay(FURL());
		ADSTRPlayerState* State = World->SpawnActor<ADSTRPlayerState>();
		Hero = World->SpawnActor<ADediServerRPGCharacter>();
		Hero->SetPlayerState(State);
		UDSTRAbilitySystemComponent* ASC = State->GetDSTRAbilitySystemComponent();
		ASC->AddAttributeSetSubobject(const_cast<UDSTRAttributeSet*>(State->GetAttributeSet()));
		ASC->InitAbilityActorInfo(State, Hero);
	}

	~FPresentationWorld()
	{
		World->DestroyWorld(false);
		GameInstance->Shutdown();
		GEngine->DestroyWorldContext(World);
		GameInstance->RemoveFromRoot();
	}

	void ReceiveAction(const EDSTRCombatAction Action)
	{
		// Exercise the real RepNotify path without adding a test-only game API.
		FStructProperty* Property = FindFProperty<FStructProperty>(
			Hero->GetClass(), TEXT("ReplicatedCombatAction"));
		check(Property);
		FDSTRReplicatedCombatAction* State =
			Property->ContainerPtrToValuePtr<FDSTRReplicatedCombatAction>(Hero);
		State->Action = Action;
		State->Variant = 1;
		++State->Sequence;
		Hero->ProcessEvent(Hero->FindFunctionChecked(TEXT("OnRep_CombatAction")), nullptr);
	}
};

class FVerifyPresentation final : public IAutomationLatentCommand
{
public:
	FVerifyPresentation(FAutomationTestBase& InTest,
		TSharedRef<FPresentationWorld> InFixture, const EPresentationOrder InOrder)
		: Test(InTest), Fixture(MoveTemp(InFixture)), Order(InOrder),
		  Deadline(FPlatformTime::Seconds() + 60.0)
	{
	}

	virtual bool Update() override
	{
		if (!FDSTRVisualAssetRegistry::AreVisualAssetsLoaded(Fixture->Hero))
		{
			if (FPlatformTime::Seconds() < Deadline)
			{
				return false;
			}
			Test.AddError(TEXT("Presentation asset preload did not complete within 60 seconds"));
			return true;
		}

		UAnimInstance* AnimInstance = Fixture->Hero->GetMesh()->GetAnimInstance();
		if (!Test.TestNotNull(TEXT("The real player AnimBP is initialized"), AnimInstance))
		{
			return true;
		}
		if (Order == EPresentationOrder::VisualBeforeAction)
		{
			Fixture->ReceiveAction(EDSTRCombatAction::BasicAttack);
		}

		if (Order == EPresentationOrder::ActionClearedBeforeVisual)
		{
			Test.TestEqual(TEXT("Cleared state stays idle after visual initialization"),
				Fixture->Hero->GetCurrentCombatAction(), EDSTRCombatAction::None);
			UAnimMontage* ExpiredMontage = Cast<UAnimMontage>(
				FDSTRCombatPresentation::ResolveVariant(EDSTRCombatAction::BasicAttack, 1).ResolveObject());
			Test.TestNotNull(TEXT("The expired attack asset is available for the negative check"), ExpiredMontage);
			Test.TestFalse(TEXT("An expired attack is not replayed"),
				ExpiredMontage && AnimInstance->Montage_IsPlaying(ExpiredMontage));
		}
		else
		{
			UAnimMontage* ExpectedMontage = Cast<UAnimMontage>(
				FDSTRCombatPresentation::ResolveVariant(EDSTRCombatAction::BasicAttack, 1).ResolveObject());
			Test.TestNotNull(TEXT("The replicated attack variant asset is loaded"), ExpectedMontage);
			Test.TestEqual(TEXT("Current replicated action is presented once visuals are ready"),
				Fixture->Hero->GetCurrentCombatAction(), EDSTRCombatAction::BasicAttack);
			Test.TestTrue(TEXT("The actual montage for the replicated variant is playing"),
				ExpectedMontage && AnimInstance->Montage_IsPlaying(ExpectedMontage));
		}
		return true;
	}

private:
	FAutomationTestBase& Test;
	TSharedRef<FPresentationWorld> Fixture;
	EPresentationOrder Order;
	double Deadline;
};

bool QueuePresentationTest(FAutomationTestBase& Test, const EPresentationOrder Order)
{
	TSharedRef<FPresentationWorld> Fixture = MakeShared<FPresentationWorld>();
	if (!Test.TestNotNull(TEXT("Player spawned"), Fixture->Hero))
	{
		return false;
	}
	if (Order != EPresentationOrder::VisualBeforeAction)
	{
		Fixture->ReceiveAction(EDSTRCombatAction::BasicAttack);
		Test.TestEqual(TEXT("No presentation is possible before the player visual is applied"),
			Fixture->Hero->GetCurrentCombatAction(), EDSTRCombatAction::None);
		if (Order == EPresentationOrder::ActionClearedBeforeVisual)
		{
			Fixture->ReceiveAction(EDSTRCombatAction::None);
		}
	}
	Fixture->Hero->DispatchBeginPlay();
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<FVerifyPresentation>(Test, Fixture, Order));
	return true;
}

constexpr uint32 PresentationTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRActionBeforeVisualTest,
	"DSTR.Presentation.Player.ActionBeforeVisual", PresentationTestFlags)

bool FDSTRActionBeforeVisualTest::RunTest(const FString&)
{
	return QueuePresentationTest(*this, EPresentationOrder::ActionBeforeVisual);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRVisualBeforeActionTest,
	"DSTR.Presentation.Player.VisualBeforeAction", PresentationTestFlags)

bool FDSTRVisualBeforeActionTest::RunTest(const FString&)
{
	return QueuePresentationTest(*this, EPresentationOrder::VisualBeforeAction);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRActionClearedBeforeVisualTest,
	"DSTR.Presentation.Player.ActionClearedBeforeVisual", PresentationTestFlags)

bool FDSTRActionClearedBeforeVisualTest::RunTest(const FString&)
{
	return QueuePresentationTest(*this, EPresentationOrder::ActionClearedBeforeVisual);
}

#endif
