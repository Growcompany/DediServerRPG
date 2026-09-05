#include "World/DSTRNavAudit.h"
#include "DSTRLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "AI/Navigation/NavCollisionBase.h"
#include "PhysicsEngine/BodySetup.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavCollision.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/RecastNavMeshGenerator.h"

int32 UDSTRNavAudit::AuditNavBounds(UObject* WorldContextObject)
{
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!World || !Navigation)
	{
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_NAVAUDIT World=%d NavSys=0"), World != nullptr);
		return -1;
	}

	UE_LOG(LogDSTR, Log, TEXT("DSTR_NAVAUDIT Agents=%d IsGameWorld=%d NavDataCount=%d InvokersOnly=%d"),
		Navigation->GetSupportedAgents().Num(),
		World->IsGameWorld() ? 1 : 0,
		Navigation->NavDataSet.Num(),
		Navigation->IsActiveTilesGenerationEnabled() ? 1 : 0);

	int32 Result = -1;
	for (ANavigationData* NavData : Navigation->NavDataSet)
	{
		if (!NavData)
		{
			continue;
		}
		TArray<FBox> Bounds;
		Navigation->GetNavigationBoundsForNavData(*NavData, Bounds);
		Result = Bounds.Num();
		const FRecastNavMeshGenerator* Recast =
			static_cast<const FRecastNavMeshGenerator*>(NavData->GetGenerator());
		UE_LOG(LogDSTR, Log,
			TEXT("DSTR_NAVAUDIT NavData=%s SupportedBounds=%d Generator=%d RestrictToActive=%d Remaining=%d"),
			*NavData->GetName(), Bounds.Num(), Recast ? 1 : 0,
			Recast && Recast->IsBuildingRestrictedToActiveTiles() ? 1 : 0,
			Recast ? Recast->GetNumRemaningBuildTasks() : -1);
		for (const FBox& Box : Bounds)
		{
			UE_LOG(LogDSTR, Log, TEXT("DSTR_NAVAUDIT Bounds Min=%s Max=%s"),
				*Box.Min.ToCompactString(), *Box.Max.ToCompactString());
		}
	}
	return Result;
}

bool UDSTRNavAudit::BuildNavigationSync(UObject* WorldContextObject)
{
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!World || !Navigation)
	{
		return false;
	}

	const double Start = FPlatformTime::Seconds();
	// Build()를 수동 분해: 큐잉 직후 타일 수를 계측한다.
	// UNavigationSystemV1::RebuildAll은 protected — NavData 쪽 공개 API로 동일 경로를 탄다.
	for (ANavigationData* NavData : Navigation->NavDataSet)
	{
		const FRecastNavMeshGenerator* Recast = nullptr;
		if (!NavData)
		{
			continue;
		}
		NavData->RebuildAll();
		Recast = static_cast<const FRecastNavMeshGenerator*>(NavData->GetGenerator());
		if (!Recast)
		{
			UE_LOG(LogDSTR, Warning, TEXT("DSTR_NAVQUEUE NoGenerator %s"), *NavData->GetName());
			continue;
		}
		const FBox Total = Recast->GetTotalBounds();
		UE_LOG(LogDSTR, Log,
			TEXT("DSTR_NAVQUEUE NavData=%s Queued=%d TotalBounds Valid=%d Min=%s Max=%s"),
			*NavData->GetName(), Recast->GetNumRemaningBuildTasks(),
			Total.IsValid ? 1 : 0, *Total.Min.ToCompactString(), *Total.Max.ToCompactString());
		NavData->EnsureBuildCompletion();
	}
	const double Seconds = FPlatformTime::Seconds() - Start;

	FBox NavBounds(ForceInit);
	for (const ANavigationData* NavData : Navigation->NavDataSet)
	{
		if (NavData)
		{
			NavBounds += NavData->GetBounds();
		}
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_NAVBUILD Seconds=%.2f Valid=%d Min=%s Max=%s"),
		Seconds, NavBounds.IsValid ? 1 : 0,
		*NavBounds.Min.ToCompactString(), *NavBounds.Max.ToCompactString());
	return NavBounds.IsValid != 0;
}

bool UDSTRNavAudit::ForceBuildNavigation(UObject* WorldContextObject)
{
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!World || !Navigation)
	{
		return false;
	}

	const FNavigationOctree* Octree = Navigation->GetNavOctree();
	UE_LOG(LogDSTR, Log, TEXT("DSTR_NAVFORCE OctreeBefore Present=%d SizeBytes=%u"),
		Octree ? 1 : 0, Octree ? static_cast<uint32>(Octree->GetSizeBytes()) : 0);

	// 시스템이 한 번 완전 초기화된 뒤라야 재등록 요청이 접수된다(선행 Build로 프라이밍).
	Navigation->Build();

	// 커맨드릿은 초기 옥트리 수집이 빠져 있으므로 전 액터를 재등록한다.
	int32 ActorCount = 0;
	for (FActorIterator It(World); It; ++It)
	{
		UNavigationSystemV1::UpdateActorAndComponentsInNavOctree(**It);
		++ActorCount;
	}

	// Build()가 엔진 정식 순서(등록 처리 → 옥트리 갱신 → 리빌드 → 완료 대기)를 수행한다.
	const double Start = FPlatformTime::Seconds();
	Navigation->Build();
	const double Seconds = FPlatformTime::Seconds() - Start;

	Octree = Navigation->GetNavOctree();
	UE_LOG(LogDSTR, Log, TEXT("DSTR_NAVFORCE OctreeAfter Actors=%d SizeBytes=%u"),
		ActorCount, Octree ? static_cast<uint32>(Octree->GetSizeBytes()) : 0);

	FBox NavBounds(ForceInit);
	for (const ANavigationData* NavData : Navigation->NavDataSet)
	{
		if (NavData)
		{
			NavBounds += NavData->GetBounds();
		}
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_NAVFORCE Seconds=%.2f Valid=%d Min=%s Max=%s"),
		Seconds, NavBounds.IsValid ? 1 : 0,
		*NavBounds.Min.ToCompactString(), *NavBounds.Max.ToCompactString());
	return NavBounds.IsValid != 0;
}

int32 UDSTRNavAudit::AuditOctreeAt(UObject* WorldContextObject, const FVector Point)
{
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const FNavigationOctree* Octree = Navigation ? Navigation->GetNavOctree() : nullptr;
	if (!Octree)
	{
		UE_LOG(LogDSTR, Warning, TEXT("DSTR_OCTREE NoOctree"));
		return -1;
	}

	const FBox Query(Point - FVector(200.0f), Point + FVector(600.0f));
	int32 Count = 0;
	Octree->FindElementsWithBoundsTest(Query, [&Count](const FNavigationOctreeElement& Element)
	{
		++Count;
		const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Element.GetOwner());
		const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
		const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
		UE_LOG(LogDSTR, Log,
			TEXT("DSTR_OCTREE Actor=%s Mesh=%s Geo=%d Bytes=%d Flag=%d NavColl=%d Convex=%d Z=[%.0f,%.0f]"),
			*GetNameSafe(MeshComponent ? MeshComponent->GetOwner() : Element.GetOwner()),
			Mesh ? *Mesh->GetName() : TEXT("-"),
			Element.Data->HasGeometry() ? 1 : 0,
			Element.Data->CollisionData.Num(),
			Body ? static_cast<int32>(Body->GetCollisionTraceFlag()) : -1,
			Mesh && Mesh->GetNavCollision() ? 1 : 0,
			Mesh && Mesh->GetNavCollision() && Mesh->GetNavCollision()->HasConvexGeometry() ? 1 : 0,
			Element.Bounds.GetBox().Min.Z, Element.Bounds.GetBox().Max.Z);
	});
	UE_LOG(LogDSTR, Log, TEXT("DSTR_OCTREE At=%s Count=%d"), *Point.ToCompactString(), Count);
	return Count;
}

int32 UDSTRNavAudit::RecreateLevelNavCollision(UObject* WorldContextObject)
{
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	if (!World)
	{
		return -1;
	}

	TSet<UStaticMesh*> Meshes;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		UStaticMeshComponent* Component = It->GetStaticMeshComponent();
		UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
		if (Mesh)
		{
			Meshes.Add(Mesh);
		}
	}
	for (UStaticMesh* Mesh : Meshes)
	{
		Mesh->RecreateNavCollision();
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_NAVCOLL Recreated=%d"), Meshes.Num());
	return Meshes.Num();
}

int32 UDSTRNavAudit::FixFlatMeshNavExport(UObject* WorldContextObject)
{
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	if (!World)
	{
		return -1;
	}

	int32 Fixed = 0;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		UStaticMeshComponent* Component = It->GetStaticMeshComponent();
		UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
		if (!Mesh)
		{
			continue;
		}
		// 두께 없는 평면 메시는 NavCollision convex가 퇴화해 0바이트를 export하면서도
		// true를 반환해 트라이메시 폴백까지 막는다. 컴포넌트에서 커스텀 export를 꺼서
		// BodySetup 트라이메시 경로로 우회시킨다.
		const FBox Bounds = Mesh->GetBoundingBox();
		if ((Bounds.Max.Z - Bounds.Min.Z) >= 10.0)
		{
			continue;
		}
		if (Component->HasCustomNavigableGeometry() != EHasCustomNavigableGeometry::No)
		{
			Component->SetCustomNavigableGeometry(EHasCustomNavigableGeometry::No);
			++Fixed;
			UE_LOG(LogDSTR, Log, TEXT("DSTR_NAVFLAT Component=%s Mesh=%s"),
				*It->GetName(), *Mesh->GetName());
		}
	}
	UE_LOG(LogDSTR, Log, TEXT("DSTR_NAVFLAT Total=%d"), Fixed);
	return Fixed;
}
