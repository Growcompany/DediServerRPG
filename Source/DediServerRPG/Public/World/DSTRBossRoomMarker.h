#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/DSTRDungeonRules.h"
#include "DSTRBossRoomMarker.generated.h"

class USphereComponent;

// 레벨에 배치하면 런타임 보스방 탐색을 대신해 이 위치가 아레나가 된다.
UCLASS()
class DEDISERVERRPG_API ADSTRBossRoomMarker : public AActor
{
	GENERATED_BODY()

public:
	ADSTRBossRoomMarker();

	float GetArenaRadius() const { return ArenaRadius; }
	float GetArenaHalfHeight() const { return ArenaHalfHeight; }

	static ADSTRBossRoomMarker* FindInWorld(const UWorld* World);

	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY(EditAnywhere, Category = "Boss Room", meta = (ClampMin = "100.0"))
	float ArenaRadius = FDSTRDungeonRules::BossRoomRadius;

	// 지하 홀이면 위층을 아레나 안으로 오인하지 않도록 천장보다 낮게 잡는다.
	UPROPERTY(EditAnywhere, Category = "Boss Room", meta = (ClampMin = "100.0"))
	float ArenaHalfHeight = FDSTRDungeonRules::BossRoomHalfHeight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> MarkerRoot;

// 엔진의 ATargetPoint와 같이 시각화는 에디터 전용으로 둬 배포 빌드에 싣지 않는다.
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> ArenaPreview;
#endif
};
