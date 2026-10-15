# 스키아 시계 · Android

기존 Windows 앱과 별개로 실행되는 Java 기반 Android 네이티브 앱입니다. Android 8.0(API 26) 이상을 지원하며, compile/target SDK는 36입니다.

## 화면

| 앱 시계 | 알람 편집 |
| --- | --- |
| ![앱 시계](docs/clock.png) | ![알람 편집](docs/alarm-editor.png) |

| 설정 | 홈 화면 위젯 |
| --- | --- |
| ![설정](docs/settings.png) | ![시계만 표시하고 입력 버튼은 숨긴 홈 화면 위젯](docs/widget.png) |

## 설치 및 사용

개발용 APK: `app/build/outputs/apk/debug/app-debug.apk`

1. APK를 Android 기기로 옮겨 설치합니다. 개발용 디버그 서명이며 스토어 배포용 서명은 아닙니다.
2. **알람** 탭에서 정확한 알람, 알림, 잠금 화면 알람 권한을 허용합니다. Android가 관리하는 권한 화면만 시스템 설정으로 이동합니다.
3. 앱 안에서 알람 시간·이름·반복 요일을 설정합니다. 요일을 선택하지 않으면 한 번 울리는 알람입니다.
4. **설정** 탭에서 배경·눈금·숫자, 12/24시간 표시, 알람·터치 효과음 음량, 진동, 오디오 파일을 변경합니다. 시계 모양은 설치된 모든 위젯에 함께 적용됩니다.
5. **홈 화면에 시계 위젯 추가**를 누릅니다. 위젯에는 시계만 보이며, 실제 시계 문자판과 같은 크기로 맞춘 투명 5×5 클릭 영역이 입력을 받습니다. 가운데와 좌우 3칸, 그 바로 위 3칸으로 된 머리 영역 6칸은 위젯 안에서 꿀밤을, 나머지 19칸은 최대 16방향 볼 당기기를 앱 화면으로 전환하지 않고 실행합니다. Android 런처 위젯은 연속 드래그 입력 대신 클릭별 `PendingIntent`를 전달하므로, 이 세분화된 방향 맵이 드래그 방향을 선택하는 입력 역할을 합니다. 지원하지 않는 런처에서는 홈 화면을 길게 눌러 위젯 목록에서 스키아 시계를 추가합니다.
6. 알람이 울리면 알림 또는 잠금 화면에서 해제하거나 5분 후 다시 울리도록 설정합니다. 10분이 지나면 자동 종료됩니다.

Galaxy Z Flip5 커버 화면에서는 기기를 펼쳐 **설정 → 커버 화면 → 위젯**에서 **스키아 시계 커버 위젯**을 켭니다. 이 위젯은 커버 화면의 스와이프 가능한 위젯 패널에 표시되며, 투명한 위젯 레이아웃을 사용합니다. 앱 위젯은 Android와 삼성의 Flex Window 위젯 제공자 규격(`keyguard`, `display="sub_screen"`)으로 등록되어 있어 이 목록에서 선택할 수 있습니다. 제공자 리시버는 시스템 위젯 호스트가 검색·갱신할 수 있도록 내보내고, `BIND_APPWIDGET` 권한으로 시스템 외 브로드캐스트를 제한합니다.

Z Flip5의 기본 커버 배경화면 편집 화면은 배경·시계 디자인·바로가기 등을 꾸미는 화면이고, 앱이 그 편집 화면에 별도 터치 위젯을 삽입하는 공개 확장 지점은 제공하지 않습니다. 따라서 시계 위젯은 배경화면의 시계 레이어 위에 고정되는 형태가 아니라 삼성 커버 화면 위젯 패널에서 실행됩니다. 배경화면 자체의 캐릭터 시계 디자인에는 [Flip5 ClockFace 안내](watchface-studio/flip5/README.md)의 Good Lock ClockFace용 Watch Face Studio 프로젝트를 사용합니다. 커버 화면의 앱 위젯 목록은 One UI 버전·기기 설정에 따라 달라질 수 있습니다.

알람 소리는 앱 음량과 기기의 알람 스트림 음량이 함께 적용됩니다. 선택한 오디오 파일을 읽지 못하면 기본 음성으로 복구합니다. 앱을 강제 종료하면 Android가 알람을 차단하므로 앱을 다시 열어야 합니다. 제조사별 절전 정책과 잠금 화면 동작은 실제 기기에서 추가 검증이 필요합니다.

## 빌드

JDK 17 이상, Android SDK Platform 36이 필요합니다. Android Studio에서 이 `android` 폴더를 프로젝트로 열거나 아래 명령을 실행합니다.

```sh
# SDK는 ANDROID_HOME 또는 local.properties의 sdk.dir로 지정
./gradlew assembleDebug lintDebug
./tools/test-time.sh
```

macOS에 Android Studio가 설치되어 있다면:

```sh
export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
export ANDROID_HOME="$HOME/Library/Android/sdk"
./gradlew assembleDebug lintDebug
```

## 구현 구조와 기술

- **MainActivity**: 시계·알람·설정의 세 탭을 구성합니다. `TimePicker`로 시간, 체크박스로 월~일 비트마스크를 입력합니다. `WindowInsets`를 적용해 상태 표시줄과 제스처 영역을 피합니다.
- **ClockView**: `Canvas` 좌표계를 480×480으로 정규화하고 바보털 PNG를 중심점 기준으로 회전합니다. 시침은 분을, 분침은 초를 반영합니다. 꿀밤·볼 당기기는 원본 프레임 타이밍과 효과음을 재생합니다. 볼 당기기는 얼굴 레이어를 별도 `Bitmap`으로 분리한 뒤 패딩된 입력에 24×24 `drawBitmapMesh`를 적용하고, cubic smoothstep 변위 감쇠로 당김 영역 가장자리의 좌표를 부드럽게 연결합니다.
- **ClockWidget / WidgetActionReceiver**: `AppWidgetProvider`와 `RemoteViews`를 사용합니다. 고정 문자판 `ImageView`, 캐릭터 이미지 레이어, 시스템 `AnalogClock`의 시침·분침을 겹치고, 문자판과 같은 정사각형 크기의 투명 5×5 `GridLayout`을 최상단에 둡니다. 각 `TextView`는 명시적 브로드캐스트 `PendingIntent`에 연결합니다. 버튼 배경과 라벨은 그리지 않으므로 화면에는 시계만 보입니다. 머리 쪽 가운데·좌우 3칸과 바로 위 3칸, 총 6칸은 꿀밤을, 나머지 19칸은 최대 16방향 볼 당기기를 선택합니다. 액션 리시버는 `BroadcastReceiver.goAsync()`로 짧은 애니메이션 동안 작업을 유지하고, 화면에 붙이지 않은 480×480 `ClockView`를 400×400 재사용 `Bitmap`에 축소 렌더링해 50ms 간격으로 캐릭터 레이어에 적용합니다. 정적 문자판 레이어는 계속 유지하므로 애니메이션 중 눈금·숫자가 사라지거나 시계 크기가 바뀌지 않습니다. 위젯 애니메이션의 228px 문자판 반지름과 눈금·숫자 위치·폰트 크기는 정적 기준에 맞추되, 캐릭터의 1.6배 원본 스케일은 유지합니다. 프레임마다 비트맵을 새로 할당하지 않고 같은 캐릭터 `ImageView`의 이미지 데이터만 갱신해 가비지 컬렉션과 런처 전달 데이터를 줄입니다. 애니메이션이 끝나면 캐릭터 레이어를 반복 idle 이미지로 복원하고 시스템 시침·분침을 다시 표시합니다. 홈 위젯과 커버 화면 위젯은 각각 별도 `AppWidgetProvider`로 등록하며, 외부 시스템 호스트가 제공자를 찾을 수 있도록 receiver를 공개하고 `BIND_APPWIDGET` 서명 권한으로 수신 브로드캐스트를 제한합니다. 커버 제공자는 삼성 `display="sub_screen"` metadata, `keyguard` category, 352×339dp 최소 크기와 투명 레이아웃을 사용해 Flex Window에 노출됩니다. Android 위젯 호스트는 클릭 이벤트만 전달하므로 투명 영역은 방향 입력을 이산화해 당김 방향을 선택합니다. 시간 갱신은 런처의 시계 컴포넌트가 수행하므로 주기적인 시계 서비스가 필요하지 않습니다.
- **AlarmTime / Scheduler**: `java.time`의 로컬 날짜·시간 및 `ZoneId`로 다음 실행 시각을 계산합니다. `AlarmManager.setAlarmClock()`에 알람 ID별 immutable `PendingIntent`를 전달합니다. 반복 알람은 매번 다음 실행을 다시 예약하여 부정확한 반복 타이머를 피합니다. 서머타임 결손 시간은 앞으로 이동하고, 중복 시간에는 첫 번째 시각을 선택해 하루 한 번 울립니다.
- **AlarmReceiver / AlarmService**: 예약 수신 후 `mediaPlayback` 유형의 포그라운드 서비스를 시작합니다. 알람 용도의 `AudioAttributes`, 일시적인 오디오 포커스, 반복 재생 `MediaPlayer`, 진동 및 제한 시간이 있는 `WakeLock`을 사용합니다. 종료 시 미디어·진동·오디오 포커스·웨이크락을 해제합니다. 알림에는 해제·스누즈 액션과 full-screen intent가 포함됩니다.
- **Store / RestoreReceiver**: 앱 전용 `SharedPreferences`에 JSON 알람 목록, 외형 설정, 음성 URI, 스누즈 시각을 저장합니다. 알람 상태는 프로세스 종료 전에 기록되도록 동기 커밋합니다. 재부팅 후 잠금 해제, 시간·시간대 변경, 앱 업데이트, 정확한 알람 권한 승인 시 예약을 복원합니다. 기기 재부팅 직후 최초 잠금 해제 전 알람은 지원하지 않습니다.
- **오디오 선택**: Storage Access Framework의 `ACTION_OPEN_DOCUMENT`와 영구 URI 읽기 권한을 사용합니다. 광범위한 저장소 접근 권한이 필요 없습니다.

[Android 공식 알람 예약 문서](https://developer.android.com/develop/background-work/services/alarms)의 정확한 알람 및 백그라운드 실행 정책에 맞추어 구성했습니다.

## 원본 리소스 재사용

저장소에는 `source` 원본 폴더가 없어 `distribution/SkeaClock.exe`의 PE `RT_RCDATA`에서 이미 포함된 캐릭터·바보털·음성을 추출했습니다. 실행 파일은 실행하지 않습니다.

`tools/extract_assets.py`는 Pillow와 pefile을 사용하며 Windows 실행 파일의 코드 실행 없이 PE `RT_RCDATA` 리소스와 프레임 정렬 INI를 읽습니다. 원본 초록 매트를 GIF 투명 인덱스로 바꾸고 원본 지연시간을 JSON 메타데이터로 기록합니다. 위젯용 8개 다이얼 이미지를 만들고 시계 손 그림의 축을 12시 기준으로 정렬합니다. 일반 Android 빌드는 생성된 PNG·GIF·WAV 리소스를 사용하므로 Python이 필요하지 않습니다. 재생성 시 폰트는 macOS Helvetica 또는 Linux DejaVu Sans를 사용합니다.

`tools/generate_widget_assets.py`는 체크인된 idle GIF와 중심점·프레임 시간 JSON에서 캐릭터 없는 8개 위젯 문자판과 원형 마스크가 적용된 103프레임 반복 WebP를 생성합니다. API 28 이상은 `AnimatedImageDrawable`로 재생하고, API 26·27용 정적 첫 프레임도 함께 생성합니다. 이 스크립트는 Pillow만 필요하며 PE 파일을 다시 읽지 않습니다.

앱 시계에는 idle·전환·꿀밤·분노·놀람·알람 panic·볼 당김·튕김에 사용하는 Windows 원본 GIF 프레임과 음성을 이식했습니다. 얼굴 레이어의 정렬, 원본 GIF 프레임 시간, 캐릭터의 프레임별 중심점도 유지합니다. 시계 가운데를 탭하면 별 효과와 꿀밤 반응을 재생하고, 얼굴 왼쪽 볼을 누른 채 끌면 얼굴 메시를 변형해 손가락을 뗄 때 놓은 방향으로 바로 튕깁니다. 위젯 입력은 클릭을 브로드캐스트로 받아 `ClockView`의 오프스크린 프레임을 위젯 이미지에 렌더링하므로 앱 화면으로 전환하지 않고 애니메이션을 보여줍니다. 위젯 볼 당기기에서는 방향을 입력한 뒤 당김 자세를 2초 유지하고 튕김 반응을 재생합니다. 정지 상태에서 캐릭터는 별도 반복 애니메이션 이미지로 흔들리고, 시계 문자판·눈금·숫자는 고정 레이어로 유지합니다. API 28 이상은 시스템 `AnimatedImageDrawable`을 사용하고 API 26·27은 정적 첫 프레임을 표시합니다. 정적 위젯 상태에서는 시스템 시침·분침을 표시합니다. Windows의 작업 표시줄, 다른 창 위 고정, 디스플레이 선택, 예외 프로그램 목록은 Android 런처 환경에 해당 기능이 없어 적용하지 않았습니다.

## 검증

- `assembleDebug`, `assembleDebugAndroidTest`, `lintDebug` 성공. Lint 오류 0개. 한국어 고정 문자열, 이전 버전에서 무시되는 일부 위젯 속성 등 경고는 남아 있습니다.
- JVM 계산 테스트 8개: 당일 예약, 정확한 시각 경계, 주간 순환, 평일 반복, 일요일 비트, 연도 변경, 서머타임 결손·중복.
- Android 15/API 35 에뮬레이터 instrumentation: 영속화, 실제 AlarmManager 예약·전달, 포그라운드 서비스 시작, 단발 알람 비활성화, 스누즈·복원·삭제, `RemoteViews` 시계 inflate, 외형 8개 조합 확인.
- Android 15/API 35 에뮬레이터에서 위젯의 꿀밤·볼 당기기를 눌러 캐릭터가 위젯 안에서 반응하고 애니메이션 종료 후 정적 시계로 돌아오는 것을 확인했습니다. 두 동작 모두 런처가 최상위 Activity로 유지되며 앱 Activity를 열지 않습니다.
- Android 15/API 35 에뮬레이터에서 idle 이미지의 반복 재생과 정시 눈금이 표시되는 것을 확인했습니다. API 28 이상에서 `RemoteViews`가 `AnimatedImageDrawable`을 로드하는 계측 확인도 통과했습니다.
- 앱 화면에서는 손가락을 뗀 직후 튕김 반응으로 전환되는 것을 확인했습니다. 위젯 볼 당기기는 앱 Activity 전환 없이 당김 자세를 2초 유지한 뒤 튕김 반응으로 전환되는 것을 확인했습니다.
- `assembleDebug`, `assembleDebugAndroidTest`, `lintDebug`가 통과했습니다. 실제 기기의 스피커, 재부팅, 제조사 절전 정책과 Android 8~14 및 16에서의 기기별 동작은 추가 검증이 필요합니다.

Instrumentation은 앱 데이터를 초기화하므로 테스트 전용 에뮬레이터에서 실행합니다.

```sh
./gradlew assembleDebug assembleDebugAndroidTest
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb install -r app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
adb shell appops set com.skeaclock.app SCHEDULE_EXACT_ALARM allow
adb shell pm grant com.skeaclock.app android.permission.POST_NOTIFICATIONS
adb shell am instrument -w com.skeaclock.app.test/com.skeaclock.app.SmokeInstrumentation
```
