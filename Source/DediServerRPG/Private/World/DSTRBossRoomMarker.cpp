#include "World/DSTRBossRoomMarker.h"
#include "DSTRLog.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"

ADSTRBossRoomMarker::ADSTRBossRoomMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	MarkerRoot = CreateDefaultSubobject<USceneComponent>(TEXT("MarkerRoot"));
	SetRootComponent(MarkerRoot);

#if WITH_EDITORONLY_DATA
	ArenaPreview = CreateDefaultSubobject<USphereComponent>(TEXT("ArenaPreview"));
	ArenaPreview->SetupAttachment(MarkerRoot);
	ArenaPreview->SetSphereRadius(ArenaRadius);
	ArenaPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArenaPreview->SetHiddenInGame(true);
	ArenaPreview->bVisualizeComponent = true;
#endif
}

ADSTRBossRoomMarker* ADSTRBossRoomMarker::FindInWorld(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	ADSTRBossRoomMarker* Chosen = nullptr;
	int32 Count = 0;
	for (TActorIterator<ADSTRBossRoomMarker> It(const_cast<UWorld*>(World)); It; ++It)
	{
		++Count;
		// 중복 배치 시 이름 순으로 고정 선택해야 서버를 다시 켤 때마다 보스방이 바뀌지 않는다.
		if (!Chosen || It->GetName() < Chosen->GetName())
		{
			Chosen = *It;
		}
	}
	if (Count > 1)
	{
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_BOSS_MARKER_DUPLICATE Count=%d Using=%s"),
			Count, *GetNameSafe(Chosen));
	}
	return Chosen;
}

void ADSTRBossRoomMarker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#if WITH_EDITORONLY_DATA
	if (ArenaPreview)
	{
		ArenaPreview->SetSphereRadius(ArenaRadius);
	}
#endif
}
