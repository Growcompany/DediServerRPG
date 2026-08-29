#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DSTREvidenceCaptureComponent.generated.h"

UCLASS()
class DEDISERVERRPG_API UDSTREvidenceCaptureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDSTREvidenceCaptureComponent();

	static bool IsFrameDumpEnabled(const FString& CommandLine);
	static bool IsVisualEvidenceEnabled(const FString& CommandLine);
	static bool ShouldAttach(const FString& CommandLine, bool bRenderingClient);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void CaptureVisualEvidence();
	void DumpClipFrame();

	FTimerHandle VisualEvidenceTimerHandle;
	int32 VisualEvidenceShotIndex = 0;
	bool bClipFrameDumpEnabled = false;
	int32 ClipFrameIndex = 0;
	static constexpr float EvidenceIntervalSeconds = 8.0f;
	static constexpr float EvidenceFirstShotSeconds = 15.0f;
	static constexpr int32 EvidenceShotCount = 8;
};
