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

## 4. 접속 완료와 플레이 준비 완료는 같은 시점이 아님

### 문제

PlayerController가 생성됐더라도 PlayerState 복제와 클라이언트 비주얼 프리로드는 아직 끝나지 않을 수 있습니다. 접속 인원만 보고 매치를 시작하면 일부 클라이언트가 준비되지 않은 상태로 전환됩니다.

### 해결

1. 로컬 PlayerController가 PlayerState 존재와 비주얼 프리로드 완료를 모두 확인합니다.
2. `BeginPlay`, `OnRep_PlayerState`, 비동기 로드 콜백에서 같은 준비 확인 함수를 호출합니다.
3. 두 조건이 충족되면 한 번만 `Server_ReportPresentationReady`를 보냅니다.
4. 서버는 `PlayerState::bPresentationReady`를 변경하고, GameMode가 방장·페이즈·인원·전원 준비를 다시 확인한 뒤 카운트다운을 시작합니다.
5. watchdog은 강제 진행하지 않고 어떤 조건이 늦었는지만 로그로 남깁니다.

전투 액션이 AnimBP보다 먼저 도착한 경우에는 비주얼 적용 직후 현재 `ReplicatedCombatAction`을 다시 확인합니다. 활성 액션이 남아 있으면 로컬 재생 상태를 초기화하고 같은 `Action / Variant`를 적용합니다. 순간 RPC를 다시 보내지 않고 복제된 현재 상태에서 표현을 복구하므로 접속, PlayerState, 자산 로드의 도착 순서에 의존하지 않습니다.

관련 코드: [`DSTRPlayerController.cpp`](../../Source/DediServerRPG/Private/Player/DSTRPlayerController.cpp), [`DediServerRPGGameMode.cpp`](../../Source/DediServerRPG/DediServerRPGGameMode.cpp), [`DSTREnemyCharacter.cpp`](../../Source/DediServerRPG/Private/Enemy/DSTREnemyCharacter.cpp)

## 전송 방식 선택 기준

| 데이터 | 방식 | 이유 |
|---|---|---|
| 체력, 상태 태그, 쿨다운 | GAS 복제 | 게임 결과에 영향을 주는 지속 상태 |
| 매치 페이즈, 목표, 서버 마감 시각 | GameState/PlayerState 복제 | 늦게 도착해도 현재 상태 복구 필요 |
| 전투 동작 | `Action / Sequence / Variant` RepNotify | 연속 액션과 예측 정합 필요 |
| 로비 시작·상호작용 요청 | 서버 실행 경로에서 재검사 | 클라이언트 값을 그대로 신뢰하지 않음 |
| 일회성 타격 FX·SFX | unreliable multicast | 유실돼도 게임 상태에 영향 없음 |
