#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/DSTRAbilitySystemComponent.h"
#include "AbilitySystem/DSTRAttributeSet.h"
#include "AbilitySystem/DSTRGameplayTags.h"
#include "AbilitySystem/Abilities/DSTRBasicAttackAbility.h"
#include "AbilitySystem/Abilities/DSTRFortifyAbility.h"
#include "AbilitySystem/Abilities/DSTRSprintAbility.h"
#include "Combat/DSTRCombatLibrary.h"
#include "DediServerRPG/DediServerRPGCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/DSTRPlayerState.h"

namespace
{
	struct FCombatInputWorld
	{
		UWorld* World = nullptr;
		ADediServerRPGCharacter* Hero = nullptr;
		UDSTRAbilitySystemComponent* ASC = nullptr;

		FCombatInputWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			ADSTRPlayerState* State = World->SpawnActor<ADSTRPlayerState>();
			Hero = World->SpawnActor<ADediServerRPGCharacter>();
			Hero->SetPlayerState(State);
			ASC = State->GetDSTRAbilitySystemComponent();
			ASC->AddAttributeSetSubobject(const_cast<UDSTRAttributeSet*>(State->GetAttributeSet()));
			ASC->InitAbilityActorInfo(State, Hero);
			ASC->GiveStartupAbilities();
		}

		~FCombatInputWorld()
		{
			ASC->CancelAbilities();
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		void Advance(const float Seconds)
		{
			// Use real small frames: a single long tick is clamped and only schedules new timers.
			for (float Remaining = Seconds; Remaining > KINDA_SMALL_NUMBER;)
			{
				const float Step = FMath::Min(0.01f, Remaining);
				++GFrameCounter;
				World->Tick(LEVELTICK_All, Step);
				Remaining -= Step;
			}
		}

		bool IsActive(const TSubclassOf<UGameplayAbility> Ability) const
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(Ability);
			return Spec && Spec->IsActive();
		}
	};

	constexpr uint32 TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRBufferedAttackTest,
	"DSTR.Combat.Input.AttackNearRecovery", TestFlags)

bool FDSTRBufferedAttackTest::RunTest(const FString&)
{
	FCombatInputWorld Fixture;
	Fixture.ASC->AddLooseGameplayTag(DSTRGameplayTags::State_Attacking.GetTag());
	Fixture.ASC->AbilityInputTagPressed(DSTRGameplayTags::InputTag_Ability_BasicAttack.GetTag());
	Fixture.ASC->AbilityInputTagReleased(DSTRGameplayTags::InputTag_Ability_BasicAttack.GetTag());
	TestFalse(TEXT("Input does not interrupt the current attack"), Fixture.IsActive(UDSTRBasicAttackAbility::StaticClass()));
	Fixture.Advance(0.10f);
	Fixture.ASC->RemoveLooseGameplayTag(DSTRGameplayTags::State_Attacking.GetTag());
	Fixture.Advance(0.03f);
	TestTrue(TEXT("A recent tap starts once recovery ends"), Fixture.IsActive(UDSTRBasicAttackAbility::StaticClass()));
	Fixture.Advance(0.80f);
	Fixture.Advance(0.03f);
	TestFalse(TEXT("One tap cannot repeat indefinitely"), Fixture.IsActive(UDSTRBasicAttackAbility::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRExpiredInputTest,
	"DSTR.Combat.Input.OldTapExpires", TestFlags)

bool FDSTRExpiredInputTest::RunTest(const FString&)
{
	FCombatInputWorld Fixture;
	Fixture.ASC->AddLooseGameplayTag(DSTRGameplayTags::State_Attacking.GetTag());
	Fixture.ASC->AbilityInputTagPressed(DSTRGameplayTags::InputTag_Ability_BasicAttack.GetTag());
	Fixture.Advance(0.30f);
	Fixture.ASC->RemoveLooseGameplayTag(DSTRGameplayTags::State_Attacking.GetTag());
	Fixture.Advance(0.03f);
	TestFalse(TEXT("An old tap does not become a delayed surprise attack"), Fixture.IsActive(UDSTRBasicAttackAbility::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRLatestInputTest,
	"DSTR.Combat.Input.LatestTapWins", TestFlags)

bool FDSTRLatestInputTest::RunTest(const FString&)
{
	FCombatInputWorld Fixture;
	Fixture.ASC->AddLooseGameplayTag(DSTRGameplayTags::State_Attacking.GetTag());
	Fixture.ASC->AbilityInputTagPressed(DSTRGameplayTags::InputTag_Ability_BasicAttack.GetTag());
	Fixture.Advance(0.04f);
	Fixture.ASC->AbilityInputTagPressed(DSTRGameplayTags::InputTag_Ability_Fortify.GetTag());
	Fixture.ASC->RemoveLooseGameplayTag(DSTRGameplayTags::State_Attacking.GetTag());
	Fixture.Advance(0.03f);
	TestTrue(TEXT("The latest skill tap wins"), Fixture.IsActive(UDSTRFortifyAbility::StaticClass()));
	TestFalse(TEXT("The earlier attack is not also queued"), Fixture.IsActive(UDSTRBasicAttackAbility::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRDisabledInputTest,
	"DSTR.Combat.Input.StunClearsPendingTap", TestFlags)

bool FDSTRDisabledInputTest::RunTest(const FString&)
{
	FCombatInputWorld Fixture;
	Fixture.ASC->AddLooseGameplayTag(DSTRGameplayTags::State_Attacking.GetTag());
	Fixture.ASC->AbilityInputTagPressed(DSTRGameplayTags::InputTag_Ability_BasicAttack.GetTag());
	Fixture.ASC->AddLooseGameplayTag(DSTRGameplayTags::State_Stunned.GetTag());
	Fixture.Advance(0.03f);
	Fixture.ASC->RemoveLooseGameplayTag(DSTRGameplayTags::State_Stunned.GetTag());
	Fixture.ASC->RemoveLooseGameplayTag(DSTRGameplayTags::State_Attacking.GetTag());
	Fixture.Advance(0.03f);
	TestFalse(TEXT("Recovery from a stun cannot replay an earlier tap"), Fixture.IsActive(UDSTRBasicAttackAbility::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRReleasedInactiveInputTest,
	"DSTR.Combat.Input.ReleaseAfterRejectedSprint", TestFlags)

bool FDSTRReleasedInactiveInputTest::RunTest(const FString&)
{
	FCombatInputWorld Fixture;
	Fixture.ASC->AddLooseGameplayTag(DSTRGameplayTags::State_Attacking.GetTag());
	Fixture.ASC->AbilityInputTagPressed(DSTRGameplayTags::InputTag_Ability_Sprint.GetTag());
	Fixture.ASC->AbilityInputTagReleased(DSTRGameplayTags::InputTag_Ability_Sprint.GetTag());
	const FGameplayAbilitySpec* Sprint = Fixture.ASC->FindAbilitySpecFromClass(UDSTRSprintAbility::StaticClass());
	TestFalse(TEXT("Releasing a rejected sprint clears its held input"), Sprint->InputPressed);
	Fixture.ASC->RemoveLooseGameplayTag(DSTRGameplayTags::State_Attacking.GetTag());
	Fixture.Advance(0.03f);
	TestFalse(TEXT("Sprint is never buffered after its key was released"), Fixture.IsActive(UDSTRSprintAbility::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRDownedCancelsCombatTest,
	"DSTR.Combat.Downed.CancelsAttackAndSprint", TestFlags)

bool FDSTRDownedCancelsCombatTest::RunTest(const FString&)
{
	FCombatInputWorld Fixture;
	Fixture.ASC->AbilityInputTagPressed(DSTRGameplayTags::InputTag_Ability_Sprint.GetTag());
	Fixture.ASC->AbilityInputTagPressed(DSTRGameplayTags::InputTag_Ability_BasicAttack.GetTag());
	TestTrue(TEXT("Sprint started before the down"), Fixture.IsActive(UDSTRSprintAbility::StaticClass()));
	TestTrue(TEXT("Attack started before the down"), Fixture.IsActive(UDSTRBasicAttackAbility::StaticClass()));
	Fixture.Hero->HandleOutOfHealth();
	TestTrue(TEXT("Hero is downed"), Fixture.Hero->IsDowned());
	TestFalse(TEXT("Downing immediately cancels the pending attack"), Fixture.IsActive(UDSTRBasicAttackAbility::StaticClass()));
	TestFalse(TEXT("Downing immediately cancels sprint"), Fixture.IsActive(UDSTRSprintAbility::StaticClass()));
	TestFalse(TEXT("Sprint speed effect is removed"), Fixture.ASC->HasMatchingGameplayTag(DSTRGameplayTags::State_Sprinting.GetTag()));
	const float Stamina = Fixture.ASC->GetNumericAttribute(UDSTRAttributeSet::GetStaminaAttribute());
	Fixture.Advance(0.50f);
	TestEqual(TEXT("Downed hero no longer spends sprint stamina"), Fixture.ASC->GetNumericAttribute(UDSTRAttributeSet::GetStaminaAttribute()), Stamina);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDSTRDownedDamageTest,
	"DSTR.Combat.Downed.RejectsLateDamage", TestFlags)

bool FDSTRDownedDamageTest::RunTest(const FString&)
{
	FCombatInputWorld Fixture;
	ADSTRPlayerState* Target = Fixture.World->SpawnActor<ADSTRPlayerState>();
	UDSTRAbilitySystemComponent* TargetASC = Target->GetDSTRAbilitySystemComponent();
	TargetASC->AddAttributeSetSubobject(const_cast<UDSTRAttributeSet*>(Target->GetAttributeSet()));
	TargetASC->InitAbilityActorInfo(Target, Target);
	TestTrue(TEXT("Living source can deal damage"), UDSTRCombatLibrary::ApplyDamage(Fixture.ASC, TargetASC, 1.0f));
	const float Health = TargetASC->GetNumericAttribute(UDSTRAttributeSet::GetHealthAttribute());
	Fixture.ASC->AddLooseGameplayTag(DSTRGameplayTags::State_Dead.GetTag());
	TestFalse(TEXT("A late hit from a downed source is rejected"), UDSTRCombatLibrary::ApplyDamage(Fixture.ASC, TargetASC, 1.0f));
	TestEqual(TEXT("The rejected hit leaves target health unchanged"), TargetASC->GetNumericAttribute(UDSTRAttributeSet::GetHealthAttribute()), Health);
	return true;
}

#endif
