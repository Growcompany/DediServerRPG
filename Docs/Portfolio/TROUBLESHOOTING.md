# 네트워크 트러블슈팅

DediServerRPG는 Dedicated Server가 게임 결과를 확정하고 최대 4개 클라이언트가 복제 상태에 수렴하는 구조입니다. 이 문서는 네트워크 권한, 수명 주기, 예측 정합과 비동기 도착 순서에서 실제로 다룬 문제만 정리합니다.

## 1. 예측 실행과 서버 타격 판정이 겹침

### 문제

GAS 어빌리티를 로컬 예측으로 시작하면 소유 클라이언트와 서버가 모두 활성화 경로를 실행합니다. 양쪽에서 타격 타이머까지 예약하면 같은 입력이 두 번 판정될 위험이 있습니다.

### 해결

- 클라이언트는 입력 반응과 몽타주만 즉시 재생합니다.
- 실제 타격 타이머는 `ShouldScheduleAuthorityImpact`를 통과한 Authority Avatar만 예약합니다.
- 모든 피해는 `UDSTRCombatLibrary::ApplyDamage`에서 Source Avatar의 Authority를 다시 검사합니다.

예측은 반응성을 위한 것이고 체력 변경 권한은 아니라는 경계를 코드 두 단계에서 고정했습니다.

관련 코드: [`DSTRBasicAttackAbility.cpp`](../../Source/DediServerRPG/Private/AbilitySystem/Abilities/DSTRBasicAttackAbility.cpp), [`DSTRCombatLibrary.cpp`](../../Source/DediServerRPG/Private/Combat/DSTRCombatLibrary.cpp)

## 2. Pawn과 PlayerState의 복제 순서가 달라 ASC 연결이 늦어짐

### 문제

클라이언트에서는 Pawn이 생긴 시점과 PlayerState가 복제된 시점이 같지 않습니다. ASC를 Pawn 수명에 묶거나 `BeginPlay` 한 곳에서만 초기화하면 접속 직후 능력 입력이 비거나 Pawn 교체 때 상태가 끊길 수 있습니다.

### 해결

- ASC와 AttributeSet은 복제 수명이 긴 `ADSTRPlayerState`가 소유합니다.
- 서버는 `PossessedBy`, 클라이언트는 `OnRep_PlayerState`에서 `InitAbilityActorInfo(PlayerState, Character)`를 호출합니다.
- 두 경로가 겹쳐도 Gameplay Tag 델리게이트는 이전 핸들을 제거한 뒤 하나만 다시 연결합니다.
- 시작 어빌리티와 초기 효과는 Authority에서만, 내부 가드를 거쳐 한 번 적용합니다.

Owner는 PlayerState, Avatar는 현재 Character라는 GAS 수명 계약을 서버와 클라이언트 양쪽에서 동일하게 복구합니다.

관련 코드: [`DSTRPlayerState.cpp`](../../Source/DediServerRPG/Private/Player/DSTRPlayerState.cpp), [`DediServerRPGCharacter.cpp`](../../Source/DediServerRPG/DediServerRPGCharacter.cpp)

## 3. 순간 Multicast로는 연속 전투 액션을 복구할 수 없음

### 문제

공격 시작을 순간 RPC로만 보내면 늦게 관련성을 얻은 클라이언트는 현재 액션을 복구할 수 없습니다. 같은 기본 공격이 연속 발생하면 액션 값 자체가 같고, 소유 클라이언트의 예측 몽타주와 서버 확인 몽타주가 중복될 수도 있습니다.

### 해결

서버가 다음 상태를 RepNotify로 복제합니다.

```cpp
struct FDSTRReplicatedCombatAction
{
    EDSTRCombatAction Action;
    uint16 Sequence;
    uint8 Variant;
};
```

- `Action`: 현재 동작
- `Sequence`: 같은 동작이 연속 발생해도 복제 변경을 만드는 번호
- `Variant`: 콤보/변형 선택

원격 클라이언트는 RepNotify에서 액션을 재생합니다. 소유 클라이언트는 회복 시간 안에 같은 액션과 변형이 확인되면 로컬 예측 재생을 유지하고 중복 시작만 억제합니다.

Dedicated Server가 애니메이션 자산을 로드하지 않는 경계를 지키기 위해 몽타주 인스턴스가 아닌 작은 의미 상태만 전송합니다.

관련 코드: [`DSTRCombatPresentation.h`](../../Source/DediServerRPG/Public/Presentation/DSTRCombatPresentation.h), [`DSTRCombatActionReconciliation.cpp`](../../Source/DediServerRPG/Private/Combat/DSTRCombatActionReconciliation.cpp)

## 4. 서버가 거부한 공격에도 클라이언트 피드백이 보임

### 문제

공격 시도만으로 타격 FX와 사운드를 보내면 잠든 보스, 무적, 사망 대상처럼 서버가 피해를 거부한 경우에도 성공한 공격처럼 보입니다.

### 해결

`UDSTRCombatLibrary::ApplyDamage`가 실제 GameplayEffect 적용 여부를 `bool`로 반환합니다. Authority 어빌리티는 반환값이 `true`일 때만 위협도와 `HitDealt` 피드백을 발생시킵니다.

피드백은 서버 확정 결과의 소비자이며 피해 판정의 근거가 아닙니다. 일회성 FX·SFX는 유실돼도 게임 상태가 달라지지 않으므로 unreliable multicast를 유지합니다.

관련 코드: [`DSTRCombatLibrary.cpp`](../../Source/DediServerRPG/Private/Combat/DSTRCombatLibrary.cpp), [`DSTRBasicAttackAbility.cpp`](../../Source/DediServerRPG/Private/AbilitySystem/Abilities/DSTRBasicAttackAbility.cpp)

## 5. 부활 요청 시점과 서버 상태가 달라질 수 있음

### 문제

클라이언트가 상호작용을 시작한 뒤 서버에서 대상이 탈락하거나 거리가 벌어질 수 있습니다. 클라이언트가 선택한 대상과 상태를 그대로 신뢰하면 오래된 요청이 적용될 수 있습니다.

### 해결

- 부활 실행 시 서버가 가까운 대상을 다시 찾습니다.
- 자기 자신 여부, 시전자 다운 여부, 대상 다운 여부, 탈락 여부와 실제 거리를 다시 검사합니다.
- 검사를 통과한 서버만 Dead 효과를 제거하고 체력 회복 GameplayEffect를 적용합니다.
- 부활 직후 겹쳐 들어온 공격으로 재다운되지 않도록 2.5초 무적 GameplayEffect도 서버가 부여합니다.

클라이언트 요청은 의도만 전달하고 대상 선택과 상태 변경은 서버가 현재 월드 상태로 다시 계산합니다.

관련 코드: [`DediServerRPGCharacter.cpp`](../../Source/DediServerRPG/DediServerRPGCharacter.cpp), [`DSTRGameplayEffects.cpp`](../../Source/DediServerRPG/Private/AbilitySystem/DSTRGameplayEffects.cpp)

## 6. 남은 초를 계속 복제하면 드리프트와 갱신 비용이 생김

### 문제

로비 카운트다운과 45초 출혈 시간을 매초 값으로 복제하면 클라이언트별 도착 시점에 따라 표시가 어긋나고 같은 상태를 반복 전송하게 됩니다.

### 해결

- 서버는 `CountdownEndServerTime`과 `BleedOutEndServerTime`이라는 절대 마감 시각만 복제합니다.
- 클라이언트는 `GameState::GetServerWorldTimeSeconds()`와 마감 시각의 차이로 남은 시간을 계산합니다.
- 시작, 취소, 다운 해제처럼 의미 있는 상태 변경 때만 마감 시각을 갱신하고 `ForceNetUpdate`합니다.

UI 갱신 주기와 네트워크 복제 주기를 분리하면서 모든 클라이언트가 같은 서버 시각을 기준으로 표시합니다.

관련 코드: [`DSTRGameState.cpp`](../../Source/DediServerRPG/Private/Game/DSTRGameState.cpp), [`DSTRPlayerState.cpp`](../../Source/DediServerRPG/Private/Player/DSTRPlayerState.cpp)

## 7. 접속 완료와 플레이 준비 완료는 같은 시점이 아님

### 문제

PlayerController가 생성됐더라도 PlayerState 복제와 클라이언트 비주얼 프리로드는 아직 끝나지 않을 수 있습니다. 접속 인원만 보고 매치를 시작하면 일부 클라이언트가 준비되지 않은 상태로 전환됩니다.

### 해결

1. 로컬 PlayerController가 PlayerState 존재와 비주얼 프리로드 완료를 모두 확인합니다.
2. `BeginPlay`, `OnRep_PlayerState`, 비동기 로드 콜백에서 같은 준비 확인 함수를 호출합니다.
3. 두 조건이 충족되면 한 번만 `Server_ReportPresentationReady`를 보냅니다.
4. 서버는 `PlayerState::bPresentationReady`를 변경하고, GameMode가 방장·페이즈·인원·전원 준비를 다시 확인한 뒤 카운트다운을 시작합니다.
5. 10초 watchdog은 강제 진행하지 않고 어떤 조건이 늦었는지만 로그로 남깁니다.

최신 4클라이언트 실행에서는 동시에 시작한 클라이언트 3개가 10초 경계의 watchdog을 기록했지만 약 0.2초 뒤 모두 준비를 보고했고, 서버와 네 클라이언트가 Clear에 도달했습니다.

관련 코드: [`DSTRPlayerController.cpp`](../../Source/DediServerRPG/Private/Player/DSTRPlayerController.cpp), [`DediServerRPGGameMode.cpp`](../../Source/DediServerRPG/DediServerRPGGameMode.cpp)

## 8. RepNotify가 클라이언트 자산 로드보다 먼저 도착함

### 문제

런타임에 생성된 적의 전투 액션은 정상 복제됐지만, 메시와 AnimBP의 비동기 로드가 끝나기 전에 RepNotify가 실행되면 재생할 AnimInstance가 없어 첫 몽타주가 누락됐습니다.

### 해결

적의 `ApplyVisualAssets`가 메시와 AnimBP 적용을 마친 뒤 현재 `ReplicatedCombatAction`을 다시 확인합니다. 활성 액션이 남아 있으면 로컬 재생 상태를 초기화하고 동일한 `Action / Variant`를 재생합니다.

순간 RPC를 재전송하는 대신 이미 복제된 현재 상태에서 표현을 복구하므로 네트워크와 자산 로드의 도착 순서에 의존하지 않습니다. 최종 4클라이언트 로그에서 적 전투 애니메이션 누락 경고는 0건이었습니다.

관련 코드: [`DSTREnemyCharacter.cpp`](../../Source/DediServerRPG/Private/Enemy/DSTREnemyCharacter.cpp), [`DSTRVisualAssetSubsystem.cpp`](../../Source/DediServerRPG/Private/Presentation/DSTRVisualAssetSubsystem.cpp)

## 전송 방식 선택 기준

| 데이터 | 방식 | 이유 |
|---|---|---|
| 체력, 상태 태그, 쿨다운 | GAS 복제 | 게임 결과에 영향을 주는 지속 상태 |
| 매치 페이즈, 목표, 서버 마감 시각 | GameState/PlayerState 복제 | 늦게 도착해도 현재 상태 복구 필요 |
| 전투 동작 | `Action / Sequence / Variant` RepNotify | 연속 액션과 예측 정합 필요 |
| 로비 시작·상호작용 요청 | 서버 실행 경로에서 재검사 | 클라이언트 값을 그대로 신뢰하지 않음 |
| 일회성 타격 FX·SFX | unreliable multicast | 유실돼도 게임 상태에 영향 없음 |

## 검증 범위

- 서버 1개와 자동 플레이 클라이언트 4개가 모두 `Clear`에 도달했습니다.
- 부활 트리거/성공, 관문 진입, 보스 기상과 적 근접 공격을 로그로 확인했습니다.
- 사람 4인 파티 검증은 아직 0회이며, 출혈 만료와 HUD 카운트다운은 실제 화면으로 수동 확인하지 못했습니다.

AnimBP 그래프 제작, E2E 드라이버 행동, NavMesh 섬 문제는 네트워크 트러블슈팅 본문에서 제외했습니다. 해당 사실은 [아키텍처](ARCHITECTURE.md)와 [검증 기록](VERIFICATION.md)에 필요한 범위만 남겼습니다.
