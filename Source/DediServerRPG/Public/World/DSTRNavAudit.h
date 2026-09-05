#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DSTRNavAudit.generated.h"

// 내비 생성이 조용히 0타일로 끝나는 원인을 계측하기 위한 에디터/커맨드릿용 진단.
UCLASS()
class DEDISERVERRPG_API UDSTRNavAudit : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 에이전트 필터를 통과해 생성기에 도달하는 볼륨 수를 로그와 함께 반환한다.
	UFUNCTION(BlueprintCallable, Category = "DSTR|Nav", meta = (WorldContext = "WorldContextObject"))
	static int32 AuditNavBounds(UObject* WorldContextObject);

	// 두께 없는 평면 메시는 convex 수집을 꺼서 트라이메시 export로 폴백시킨다.
	UFUNCTION(BlueprintCallable, Category = "DSTR|Nav", meta = (WorldContext = "WorldContextObject"))
	static int32 FixFlatMeshNavExport(UObject* WorldContextObject);

	// 레벨이 쓰는 모든 스태틱 메시의 NavCollision을 다시 굽는다(빈 채 저장된 에셋 복구).
	UFUNCTION(BlueprintCallable, Category = "DSTR|Nav", meta = (WorldContext = "WorldContextObject"))
	static int32 RecreateLevelNavCollision(UObject* WorldContextObject);

	// 지정 지점 상공의 옥트리 요소와 지오메트리 보유 여부를 로그로 남긴다.
	UFUNCTION(BlueprintCallable, Category = "DSTR|Nav", meta = (WorldContext = "WorldContextObject"))
	static int32 AuditOctreeAt(UObject* WorldContextObject, FVector Point);

	// 옥트리 재등록까지 포함한 강제 동기 빌드. 성공 시 true.
	UFUNCTION(BlueprintCallable, Category = "DSTR|Nav", meta = (WorldContext = "WorldContextObject"))
	static bool ForceBuildNavigation(UObject* WorldContextObject);

	// 내비를 동기로 굽는다(Build는 EnsureBuildCompletion까지 포함). 성공 시 true.
	UFUNCTION(BlueprintCallable, Category = "DSTR|Nav", meta = (WorldContext = "WorldContextObject"))
	static bool BuildNavigationSync(UObject* WorldContextObject);
};
