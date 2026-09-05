// Copyright Epic Games, Inc. All Rights Reserved.

#include "DediServerRPG.h"
#include "DSTRLog.h"
#include "Modules/ModuleManager.h"
#include "Runtime/Launch/Resources/Version.h"

#if WITH_EDITOR && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4
#include "Abilities/GameplayAbilityRepAnimMontage.h"
#include "GameFramework/Character.h"
#include "UObject/UnrealType.h"
#endif

DEFINE_LOG_CATEGORY(LogDSTR);

class FDSTRGameModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();

#if WITH_EDITOR && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4
		// UE 5.4 includes these deprecated editor-only fields in network checksums
		// and replication layouts, although cooked clients do not contain them.
		// Apply the missing NotReplicated flag before any net driver is created.
		// Checksum validation stays enabled; no engine installation is modified.
		for (UScriptStruct* Struct : { FRepRootMotionMontage::StaticStruct(), FGameplayAbilityRepAnimMontage::StaticStruct() })
		{
			bool bFound = false;
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				if ((It->GetFName() == TEXT("AnimMontage") || It->GetFName() == TEXT("AnimMontage_DEPRECATED"))
					&& It->HasAllPropertyFlags(CPF_EditorOnly | CPF_Deprecated))
				{
					It->SetPropertyFlags(CPF_RepSkip);
					bFound = true;
					UE_LOG(LogDSTR, Log, TEXT("DSTR_EDITOR_NET_COMPAT Struct=%s Field=%s"),
						*Struct->GetName(), *It->GetName());
				}
			}
			ensureMsgf(bFound, TEXT("UE 5.4 montage compatibility field missing in %s"), *Struct->GetName());
		}
#endif
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FDSTRGameModule, DediServerRPG, "DediServerRPG");
