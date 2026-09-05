#pragma once

#include "Math/Color.h"

// 파일별 익명 네임스페이스에 두면 unity 빌드가 같은 TU로 합칠 때 재정의 충돌(C2374)이 난다.
namespace DSTRUIStyle
{
	constexpr FLinearColor PanelColor(0.015f, 0.025f, 0.045f, 0.92f);
	constexpr FLinearColor HUDPanelColor(0.015f, 0.025f, 0.045f, 0.88f);
	constexpr FLinearColor AccentColor(0.05f, 0.75f, 1.0f, 1.0f);
	constexpr FLinearColor MutedColor(0.62f, 0.72f, 0.82f, 1.0f);
	constexpr FLinearColor DangerColor(1.0f, 0.18f, 0.14f, 1.0f);
	constexpr FLinearColor ButtonColor(0.08f, 0.32f, 0.50f, 1.0f);
}
