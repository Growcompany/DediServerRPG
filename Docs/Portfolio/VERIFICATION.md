# 검증 기록

- 기준일: 2026-08-29
- 환경: Windows, Unreal Engine 5.4 설치형 엔진

공개 저장소에는 별도 테스트 파일과 자동화 실행 스크립트를 포함하지 않습니다. 명령줄 옵션으로만 붙고 Shipping에서는 비활성화되는 최소 E2E 드라이버와 캡처 훅은 `Source`에 포함합니다. 아래 결과는 전체 로컬 프로젝트에서 빌드, 전용 서버 모드와 4개 클라이언트 프로세스를 실행한 뒤 남긴 로그를 기준으로 합니다.

## 빌드

| 타깃 | 구성 | 결과 |
|---|---|---|
| `DediServerRPGEditor` | Win64 Development | 성공 |
| `DediServerRPG` | Win64 DebugGame | 성공 |

Epic Launcher로 설치한 엔진은 독립 `Server` 타깃 빌드를 지원하지 않습니다. 런타임 E2E는 Editor의 `-server -nullrhi` 모드로 수행했으며, 독립 서버 패키징에는 소스 빌드 엔진이 필요합니다.

## 4프로세스 E2E

서버 1개와 자동 플레이 클라이언트 4개로 로비부터 보스 처치까지 실행했습니다. 드라이버는 로컬 제어 Pawn에만 붙고 사람 플레이와 같은 입력 Gameplay Tag와 컨트롤 회전을 사용합니다.

```text
DSTR_4P_SERVER_CLEAR=True
DSTR_4P_CLIENT_CLEAR_COUNT=4
DSTR_4P_REVIVE_TRIGGERED=True
DSTR_4P_REVIVE_SUCCEEDED=True
DSTR_4P_GATE_SEQUENCE=True
DSTR_4P_GATE_ENTER_COUNT=1
DSTR_4P_GATE_BYPASS_COUNT=0
DSTR_4P_BOSS_AWAKE_COUNT=1
DSTR_4P_ENEMY_DEFEATED_COUNT=12
DSTR_4P_MINION_MELEE_HITS=75
```

이 결과가 확인하는 범위는 다음과 같습니다.

- 네 클라이언트의 접속과 로비 진행
- 웨이브, 전진/관문, 보스, Clear 상태 전이
- 다운 대상 발생과 부활 성공
- 관문 진입 후 보스 기상, 관문 우회 없음
- 일반 적의 접근·근접 공격과 적 처치
- 서버/클라이언트 결과 상태 일치

## 맵 정적 검사

```text
DSTR_MAP_VALID STATIC_MESHES=751 PLAYER_STARTS=4 NAV_BOUNDS=1 SLABS=10 TUNED_LIGHTS=34 GAP_TILES=1 WALL_PATCHES=1 HIDDEN_PLATES=0 KILL_Z=-2500 SPAWN_DOORS=10 MINIMAP_FLOORS=145
```

태그 기반 내비게이션 프로브의 현재 기준선은 다음과 같습니다.

```text
DSTR_SPAWN_POINTS Doors=10 Usable=2 Rule=Tag
DSTR_NAV_PROBE_DONE Doors=10 Usable=2 Attempts=1
```

`Usable=2`는 성공 기준이 아니라 확인된 레벨 한계입니다. 자세한 비교는 [NavMesh 트러블슈팅](TROUBLESHOOTING.md#8-출시-맵-navmesh가-여러-섬으로-분리)에 기록했습니다.

## 최종 로그 검사

| 검사 항목 | 건수 |
|---|---:|
| 적 전투 애니메이션 누락 경고 | 0 |
| 프로젝트 소유 자산 누락 | 0 |
| AI 런타임 실패 | 0 |
| 크래시 신호 | 0 |
| 이전 ThirdPerson 경로 참조 | 0 |

확인된 비치명 경고도 숨기지 않고 구분했습니다.

- Paragon Minions의 UE4 리그 메타데이터 경고: 외부 자산 팩의 `Orion_Proto_Retarget` 관련 경고이며 프로젝트 자산 누락은 아닙니다.
- 클라이언트 4개를 동시에 시작했을 때 로비 프레젠테이션 준비 watchdog 3건: 10초 경계에서 기록됐고 약 0.2초 뒤 모두 프리로드를 완료했습니다.

## 코드/저장소 검사

- `Source` 파일 149개
- `Source` 아래 테스트 디렉터리 0개
- 프로젝트 소유 코드 주석은 필요한 의도만 짧은 한국어로 유지
- 토큰, 비밀번호, 개인 키 패턴 없음
- 공개 문서에 개인 로컬 경로 없음
- 생성물, 상용 자산, 내부 메모와 자동화 스크립트는 추적 대상에서 제외

## 아직 검증하지 못한 항목

- 사람 4인 파티 플레이: 0회
- 출혈 45초 만료의 실제 화면 확인
- HUD 카운트다운의 실제 화면 확인
- 분리된 NavMesh를 레벨 아트로 수정한 뒤 문 10개 전체 경로
- 연결된 내비 기준점에서 관문 닫힘/열림 카빙 전이
- 소스 빌드 엔진을 사용한 독립 서버 패키징

자동 플레이 결과는 네트워크 흐름과 회귀를 확인하는 자료이며, 사람 플레이의 조작성·가독성·밸런스를 대신하지 않습니다.
