# 트러블슈팅

이 문서는 최종 코드에 남긴 해결책과 아직 해결하지 못한 범위를 함께 기록합니다. 수치는 자동화 테스트 개수가 아니라 실제 빌드, 다중 프로세스 실행과 로그에서 확인한 결과만 사용했습니다.

## 1. 예측 클라이언트가 서버 타격 타이머까지 실행

### 증상

로컬 예측으로 어빌리티가 즉시 시작될 때 클라이언트와 서버가 같은 타격 타이머를 예약할 수 있었습니다. 표현은 빨라지지만 피해 판정 경계가 흐려지고 중복 실행 위험이 생깁니다.

### 원인

어빌리티 활성화와 실제 타격을 같은 실행 단계로 취급했습니다. GAS의 예측 실행이 곧 게임 결과를 확정할 권한을 의미하지는 않습니다.

### 해결

공유 프로필은 타격 시각만 제공하고, `FDSTRCombatPresentation::ShouldScheduleAuthorityImpact`가 Authority인 Avatar에만 판정 타이머를 허용합니다. 클라이언트는 몽타주와 입력 반응만 예측합니다. 모든 피해는 다시 `UDSTRCombatLibrary::ApplyDamage`의 Authority 검사도 통과해야 합니다.

### 결과

타격 시각은 서버와 클라이언트가 같은 프로필을 쓰되, 체력 변화는 서버 한 곳에서만 발생합니다. 표현 지연과 권한 판정을 분리했습니다.

관련 코드: [`DSTRBasicAttackAbility.cpp`](../../Source/DediServerRPG/Private/AbilitySystem/Abilities/DSTRBasicAttackAbility.cpp), [`DSTRCombatLibrary.cpp`](../../Source/DediServerRPG/Private/Combat/DSTRCombatLibrary.cpp)

## 2. 순간 Multicast만으로는 전투 액션 상태를 복구할 수 없음

### 증상

빠른 연속 공격이나 액터가 막 관련성 범위에 들어온 시점에서 몽타주가 누락될 수 있었습니다. 소유 클라이언트의 예측 재생과 서버 재생이 겹치면 같은 동작이 두 번 시작되기도 했습니다.

### 원인

순간 RPC는 늦게 관련성을 얻은 클라이언트가 현재 상태를 복원할 자료가 없습니다. 액션 이름만 복제하면 같은 공격이 연속으로 발생할 때 속성 값이 바뀌지 않을 수도 있고, 콤보 변형도 일치시킬 수 없습니다.

### 해결

서버가 `FDSTRReplicatedCombatAction`을 복제합니다.

- `Action`: 현재 동작
- `Sequence`: 동일 액션의 연속 발생도 변경으로 만드는 번호
- `Variant`: 콤보/변형 선택

원격 클라이언트는 RepNotify에서 재생합니다. 소유 클라이언트는 같은 액션과 변형이 회복 시간 안에 확인되면 서버 상태에 맞추되 로컬 몽타주 재시작은 억제합니다.

### 선택 근거

몽타주 재생을 `PlayMontageAndWait` 중심의 GAS 흐름으로 완전히 옮기면 서버도 애니메이션 자산 수명에 관여해야 합니다. 이 프로젝트는 Dedicated Server가 시각 자산을 로드하지 않는 경계를 우선하므로, GAS는 판정과 상태를 맡고 작은 액션 구조체가 표현 동기화를 맡습니다.

일회성 타격 이펙트·사운드는 유실돼도 게임 규칙을 바꾸지 않으므로 unreliable multicast를 유지했습니다.

관련 코드: [`DSTRCombatPresentation.h`](../../Source/DediServerRPG/Public/Presentation/DSTRCombatPresentation.h), [`DSTRCombatActionReconciliation.cpp`](../../Source/DediServerRPG/Private/Combat/DSTRCombatActionReconciliation.cpp)

## 3. 부활 직후 후속 공격으로 즉시 재다운

### 증상

부활로 체력이 복구된 프레임 근처에 이미 진행 중인 광역 공격이나 근접 타격이 들어오면 플레이어가 조작할 틈 없이 다시 다운될 수 있었습니다.

### 원인

부활은 체력만 즉시 복구했고, 전투 공간으로 돌아오는 짧은 전환 구간을 상태 규칙으로 표현하지 않았습니다.

### 해결

`UDSTRReviveProtectionEffect`를 2.5초 지속 GameplayEffect로 만들고 `State.Invulnerable` 태그를 부여했습니다. 공통 피해 진입점이 이 상태를 확인하므로 모든 공격 타입에 같은 규칙이 적용됩니다.

### 결과

개별 공격마다 부활 예외 분기를 추가하지 않고, GAS 상태 하나로 부활 보호 규칙을 일관되게 유지합니다.

관련 코드: [`DSTRGameplayEffects.cpp`](../../Source/DediServerRPG/Private/AbilitySystem/DSTRGameplayEffects.cpp), [`DediServerRPGCharacter.cpp`](../../Source/DediServerRPG/DediServerRPGCharacter.cpp)

## 4. 거부된 공격에도 타격 피드백이 출력

### 증상

잠든 보스, 무적 대상, 이미 사망한 대상처럼 피해가 거부된 경우에도 타격 이펙트나 사운드가 재생될 수 있었습니다.

### 원인

공격 시도와 서버가 확정한 피해 결과를 같은 사건으로 취급했습니다.

### 해결

`UDSTRCombatLibrary::ApplyDamage`가 실제 GameplayEffect 적용 여부를 `bool`로 반환합니다. 어빌리티는 반환값이 `true`일 때만 위협도와 `HitDealt` 피드백을 발생시킵니다.

### 결과

보이는 타격과 서버 체력 변화의 의미가 일치합니다. 피드백은 권한 결과의 소비자가 되고 판정 자체에는 영향을 주지 않습니다.

관련 코드: [`DSTRCombatLibrary.cpp`](../../Source/DediServerRPG/Private/Combat/DSTRCombatLibrary.cpp), [`DSTRBasicAttackAbility.cpp`](../../Source/DediServerRPG/Private/AbilitySystem/Abilities/DSTRBasicAttackAbility.cpp)

## 5. 몽타주는 재생되지만 최종 포즈에 반영되지 않음

### 증상

몽타주 호출과 자산 로드는 성공했지만 캐릭터가 로코모션 포즈만 유지했습니다.

### 원인

AnimBP의 최종 포즈 경로에 몽타주가 사용하는 `FullBody` 슬롯 노드가 없었습니다. 몽타주 자산의 슬롯 이름만 맞추는 것으로는 AnimGraph 출력에 합성되지 않습니다.

### 해결

프로젝트 소유 Greystone/Sevarog AnimBP에 다음 경로를 구성했습니다.

`Locomotion Pose → FullBody Slot → Output Pose`

에디터 Python API는 필요한 AnimGraph 노드와 포즈 핀 연결을 안전하게 조작할 바인딩이 부족했습니다. `UDSTRAnimationAuthoringLibrary::AddFullBodySlot`을 에디터 전용 C++로 구현해 기존 최종 포즈 연결 확인, 슬롯 삽입, 재연결, 컴파일과 저장을 한 번에 수행하도록 했습니다.

### 결과

런타임 역할은 AnimBP에 남고 C++은 에셋 작성 절차만 자동화합니다. 전신 기술은 `FullBody`, 기본 로코모션은 상태 머신이라는 명확한 계약을 갖습니다.

관련 코드: [`DSTRAnimationAuthoringLibrary.cpp`](../../Source/DediServerRPG/Private/Presentation/DSTRAnimationAuthoringLibrary.cpp), [`DSTRCombatPresentation.cpp`](../../Source/DediServerRPG/Private/Presentation/DSTRCombatPresentation.cpp)

## 6. 스폰 직후 적의 첫 공격 애니메이션 누락

### 증상

런타임에 생성된 적의 첫 공격 판정은 정상인데 일부 클라이언트에서 첫 몽타주만 보이지 않았습니다.

### 원인

복제 액션의 RepNotify가 메시와 AnimBP의 비동기 로드 완료보다 먼저 도착할 수 있었습니다. 당시에는 재생할 AnimInstance가 없어 요청이 끝났고, 자산 준비 뒤 다시 시도하는 경로가 없었습니다.

### 해결

적의 `ApplyVisualAssets`가 메시와 AnimBP 적용을 마친 뒤 `ReplicatedCombatAction.Action`을 확인합니다. 활성 액션이 남아 있으면 로컬 재생 상태를 초기화하고 동일한 `Action / Variant`를 다시 재생합니다.

### 결과

네트워크 도착 순서와 자산 로드 순서가 달라도 현재 복제 상태에서 표현을 복구합니다. 최종 4클라이언트 로그에서 적 애니메이션 누락 경고는 0건이었습니다.

관련 코드: [`DSTREnemyCharacter.cpp`](../../Source/DediServerRPG/Private/Enemy/DSTREnemyCharacter.cpp), [`DSTRVisualAssetSubsystem.cpp`](../../Source/DediServerRPG/Private/Presentation/DSTRVisualAssetSubsystem.cpp)

## 7. E2E 드라이버가 전투 중 픽업을 추적해 진행 정체

### 증상

자동 플레이 클라이언트가 적과 교전 중에도 가까운 공격력 버프를 우선해 전투 지역을 이탈했고, 적 추적과 픽업 추적 사이에서 방향을 반복 변경할 수 있었습니다.

### 원인

픽업의 거리 우선순위만 있었고 현재 교전 여부가 행동 선택 조건에 포함되지 않았습니다.

### 해결

가까운 유효 적이 있어 `bEnemyEngaged`인 동안 픽업 분기를 차단했습니다. 전투가 끝난 뒤에만 픽업을 탐색합니다.

### 결과

복잡한 행동 계획기를 추가하지 않고 한 개의 명시적 우선순위 조건으로 정체를 제거했습니다. 최신 E2E에서 서버와 네 클라이언트가 모두 Clear에 도달했습니다.

관련 코드: [`DSTRBotDriverComponent.cpp`](../../Source/DediServerRPG/Private/Player/DSTRBotDriverComponent.cpp)

## 8. 출시 맵 NavMesh가 여러 섬으로 분리

### 증상

태그가 지정된 스폰 문 10개 중 조우 원점까지 완전한 내비게이션 경로를 얻는 문은 2개뿐입니다.

### 실측

| 시도 | 결과 | 판단 |
|---|---|---|
| 계단을 내비 대상에 포함 | 계단은 이미 포함되어 있었고 사용 가능 문 수 변화 없음 | 설정 누락 아님 |
| NavLinkProxy 추가 | 이 맵의 베이크 결과와 완전 경로 수 변화 없음 | 섬의 근본 원인 해결 못함 |
| 전체 NavMesh 재베이크 | 아레나 바닥의 내비 영역이 사라짐 | 기존 결과보다 퇴행 |
| `RuntimeGeneration=DynamicModifiersOnly` | 맵 준비 절차를 다시 수행하면 저장된 베이크 내비가 사라짐 | 설정 되돌림 |

관문의 초기 내비 카빙은 타일 빌드가 0건이라 실제로는 충돌만 막고 있었습니다. 현재 배리어에는 `bDynamicObstacle = true`, `NavArea_Null`, 충돌과 내비 관련성의 동시 전환을 적용했습니다.

### 현재 결정

런타임은 완전 경로가 확인된 문만 스폰 후보로 사용하고, 경로가 없으면 `Wave → Boss` 안전 전이를 사용합니다. 사용 가능 문 2/10은 코드로 숨겨 해결할 문제가 아니라 바닥·계단·충돌을 함께 조정해야 하는 레벨 아트 작업으로 기록합니다.

관문 카빙도 양쪽이 연결된 기준점에서 닫힘/열림 전이를 측정해야 완결되지만, 현재 NavMesh 섬 상태에서는 전체 검증할 수 없습니다.

관련 코드: [`DSTRBossGate.cpp`](../../Source/DediServerRPG/Private/World/DSTRBossGate.cpp), [`DediServerRPGGameMode.cpp`](../../Source/DediServerRPG/DediServerRPGGameMode.cpp)
