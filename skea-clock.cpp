#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <cmath>
#include <climits>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace Gdiplus;

constexpr wchar_t ClassName[] = L"SkeaClockWindow";
constexpr int AppIconResource = 1;
constexpr UINT TrayMessage = WM_APP + 1;
constexpr int Background = 101, Ticks = 103, Numbers = 104;
constexpr int AlwaysOnTop = 105, Exit = 106, EditLayout = 107;
constexpr int DisplayFirst = 200, MaxDisplays = 32;
constexpr float Pi = 3.14159265358979323846f;
bool showDial = true, showTicks = true, showNumbers = true, topmost = true;
int lastStep = -1, sizePercent = 100;
unsigned long long lastAlarmMinuteKey = ULLONG_MAX;
unsigned long long activeAlarmMinuteKey = ULLONG_MAX;
std::wstring settingsPath;
std::wstring selectedDisplay, activeDisplay;
NOTIFYICONDATAW tray = {};

struct DisplayInfo {
    MONITORINFOEXW info;
};

HICON AppIcon() {
    return LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(AppIconResource));
}

BOOL CALLBACK CollectDisplay(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    auto displays = reinterpret_cast<std::vector<DisplayInfo>*>(data);
    DisplayInfo display = { { sizeof(MONITORINFOEXW) } };
    if (GetMonitorInfoW(monitor, &display.info)) displays->push_back(display);
    return TRUE;
}

std::vector<DisplayInfo> GetDisplays() {
    std::vector<DisplayInfo> displays;
    EnumDisplayMonitors(nullptr, nullptr, CollectDisplay, reinterpret_cast<LPARAM>(&displays));
    return displays;
}

const DisplayInfo* ChooseDisplay(const std::vector<DisplayInfo>& displays) {
    for (const auto& display : displays)
        if (!selectedDisplay.empty() && _wcsicmp(display.info.szDevice, selectedDisplay.c_str()) == 0) return &display;
    for (const auto& display : displays)
        if (display.info.dwFlags & MONITORINFOF_PRIMARY) return &display;
    return displays.empty() ? nullptr : &displays.front();
}

std::wstring DisplayLabel(const DisplayInfo& display) {
    std::wstring device = display.info.szDevice;
    const size_t number = device.rfind(L"DISPLAY");
    if (number != std::wstring::npos) device = device.substr(number + 7);
    const RECT& rect = display.info.rcMonitor;
    std::wstring label = L"디스플레이 " + device + L"  (" +
        std::to_wstring(rect.right - rect.left) + L" × " + std::to_wstring(rect.bottom - rect.top) + L")";
    if (display.info.dwFlags & MONITORINFOF_PRIMARY) label += L" · 기본";
    return label;
}

float MinuteTurn(int minute, int second) {
    return (minute + second / 60.0f) / 60.0f;
}

float HourTurn(int hour, int minute, int second) {
    return ((hour % 12) + MinuteTurn(minute, second)) / 12.0f;
}

std::wstring FileName(const std::wstring& path) {
    const size_t separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? path : path.substr(separator + 1);
}

#include "clock-artwork.h"
#include "clock-smash-effect.h"
#include "clock-sound.h"
#include "clock-alarm-validation.h"
#include "clock-window-behavior.h"
#include "clock-exception-menu.h"
#include "clock-layout-editor.h"
#include "alarm-editor.h"
#include "clock-render-buffer.h"

void SaveSettings() {
    WritePrivateProfileStringW(L"clock", L"background", showDial ? L"1" : L"0", settingsPath.c_str());
    WritePrivateProfileStringW(L"clock", L"topmost", topmost ? L"1" : L"0", settingsPath.c_str());
    WritePrivateProfileStringW(L"clock", L"ticks", showTicks ? L"1" : L"0", settingsPath.c_str());
    WritePrivateProfileStringW(L"clock", L"numbers", showNumbers ? L"1" : L"0", settingsPath.c_str());
    WritePrivateProfileStringW(L"clock", L"markings", nullptr, settingsPath.c_str());
    WritePrivateProfileStringW(L"clock", L"display", selectedDisplay.c_str(), settingsPath.c_str());
    WritePrivateProfileStringW(L"clock", L"size", nullptr, settingsPath.c_str());
    SaveSoundSettings(settingsPath);
    SaveWindowBehaviorSettings(settingsPath);
}

void RenderClock(HWND window) {
    RECT rect;
    GetClientRect(window, &rect);
    const int width = rect.right, height = rect.bottom;
    if (!renderBuffer.Ensure(width, height)) return;
    {
        Bitmap surface(width, height, width * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(renderBuffer.pixels));
        Graphics g(&surface);
        g.Clear(Color(0, 0, 0, 0));
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.ScaleTransform(width / static_cast<float>(artworkWidth), height / static_cast<float>(artworkHeight));
        SYSTEMTIME now;
        GetLocalTime(&now);
        DrawArtwork(g, now, showDial, showTicks, showNumbers);
        DrawSmashStarEffect(g);
        g.ResetTransform();
        DrawLayoutEditor(g, width, height);
        g.Flush(FlushIntentionSync);
    }
    SIZE dimensions = { width, height };
    POINT origin = {};
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(window, nullptr, nullptr, &dimensions, renderBuffer.memory,
        &origin, 0, &blend, ULW_ALPHA);
}

// Use elapsed time so delayed window messages do not slow down the GIF.
void AnimateBackground(HWND window) {
    KillTimer(window, 2);
    if (frameEnds.size() < 2) return;
    const ULONGLONG now = GetTickCount64();
    ULONGLONG elapsed = now - animationStart;
    if (alarmActive) {
        if (!alarmPanicActive) {
            if (elapsed < AlarmHandGatherMs) {
                RenderClock(window);
                SetTimer(window, 2, 16, nullptr);
                return;
            }
            alarmPanicActive = true;
            animationStart = now;
            elapsed = 0;
            activeFrame = 0;
            panicImage->SelectActiveFrame(&FrameDimensionTime, 0);
            PlaySoundFile(L"alert.wav");
            RenderClock(window);
        }
        const UINT frame = FrameAt(panicFrameEnds, elapsed);
        if (frame != activeFrame && panicImage->SelectActiveFrame(&FrameDimensionTime, frame) == Ok) {
            activeFrame = frame;
            RenderClock(window);
        }
        const UINT phase = static_cast<UINT>(elapsed % panicFrameEnds.back());
        SetTimer(window, 2, std::max(1u, panicFrameEnds[frame] - phase), nullptr);
        return;
    }
    if (touchReleaseActive) {
        if (elapsed < touchEndFrameEnds.back()) {
            const UINT frame = FrameAt(touchEndFrameEnds, elapsed);
            if (frame != activeFrame && SelectTouchReleaseFrame(frame)) {
                activeFrame = frame;
                RenderClock(window);
            }
            SetTimer(window, 2, std::max<UINT>(1,
                touchEndFrameEnds[frame] - static_cast<UINT>(elapsed)), nullptr);
            return;
        }
        CompleteTouchRelease(now);
        elapsed = 0;
        RenderClock(window);
    }
    if (cheekDragging) {
        const UINT frame = FrameAt(touchFrameEnds, elapsed);
        if (frame != activeFrame &&
            touchBackImage->SelectActiveFrame(&FrameDimensionTime, frame) == Ok &&
            touchHeadImage->SelectActiveFrame(&FrameDimensionTime, frame) == Ok &&
            touchFrontImage->SelectActiveFrame(&FrameDimensionTime, frame) == Ok) {
            activeFrame = frame;
            RenderClock(window);
        }
        const UINT phase = static_cast<UINT>(elapsed % touchFrameEnds.back());
        SetTimer(window, 2, std::max(1u, touchFrameEnds[frame] - phase), nullptr);
        return;
    }
    Bitmap* previousAnimation = CurrentGif();
    Bitmap* animation = gifImage.get();
    const std::vector<UINT>* ends = &frameEnds;
    ULONGLONG localElapsed = elapsed + idlePhaseOffset;
    UINT frameOffset = 0;
    if (blankPlaybackActive) {
        animation = blankImage.get();
        ends = &blankFrameEnds;
        localElapsed = elapsed;
    } else if (reactionActive) {
        const ULONGLONG angryDuration = angryFrameEnds.back();
        const ReactionStage stage = ReactionStageAt(elapsed);
        if (stage == ReactionStage::Smash) {
            showingAngry = false;
            showingAngryAlt = false;
            showingTransition = false;
            animation = smashImage.get();
            ends = &smashFrameEnds;
            localElapsed = elapsed + SmashStartOffset();
        } else if (stage == ReactionStage::Angry) {
            showingAngry = true;
            showingAngryAlt = false;
            showingTransition = false;
            animation = angryImage.get();
            ends = &angryFrameEnds;
            localElapsed = (elapsed - AngryStart()) % angryDuration;
        } else if (stage == ReactionStage::AngryAlt) {
            showingAngry = false;
            showingAngryAlt = true;
            showingTransition = false;
            animation = angryAltImage.get();
            ends = &angryAltFrameEnds;
            localElapsed = elapsed - AngryAltStart();
            frameOffset = AngryAltStartFrame;
        } else if (stage == ReactionStage::Transition) {
            if (!showingTransition) {
                handReturnActive = true;
                handReturnStartTurns[0] = handReturnStartTurns[1] = .5f;
                handReturnStartLengthScales[0] = handReturnStartLengthScales[1] = 1.0f;
                handReturnStart = now;
            }
            showingAngry = false;
            showingAngryAlt = false;
            showingTransition = true;
            animation = transitionImage.get();
            ends = &transitionFrameEnds;
            localElapsed = elapsed - TransitionStart();
            frameOffset = TransitionStartFrame;
        } else {
            reactionActive = false;
            showingAngry = false;
            showingAngryAlt = false;
            showingTransition = false;
            animationStart = now;
            idlePhaseOffset = frameEnds[IdleReturnFrame - 1];
            animation = gifImage.get();
            localElapsed = idlePhaseOffset;
        }
    }
    const UINT localFrame = FrameAt(*ends, localElapsed);
    const UINT frame = localFrame + frameOffset;
    bool rendered = false;
    if ((animation != previousAnimation || frame != activeFrame) &&
        animation->SelectActiveFrame(&FrameDimensionTime, frame) == Ok) {
        activeFrame = frame;
        RenderClock(window);
        rendered = true;
    }
    const bool smashEffectFrame = AdvanceSmashStarEffect(now);
    if (smashEffectFrame && !rendered) RenderClock(window);
    const UINT phase = static_cast<UINT>(localElapsed % ends->back());
    UINT nextFrameDelay = std::max(1u, (*ends)[localFrame] - phase);
    if (smashStarActive) nextFrameDelay = std::min(16u, nextFrameDelay);
    SetTimer(window, 2, nextFrameDelay, nullptr);
}

void StartAlarm(HWND window, const SYSTEMTIME& now) {
    if (GetCapture() == window) ReleaseCapture();
    activeAlarmMinuteKey = AlarmMinuteKey(now);
    BeginAlarm(now);
    StartAlarmSound();
    RenderClock(window);
    AnimateBackground(window);
}

void DismissAlarm(HWND window) {
    if (!alarmActive) return;
    StopAlarmSound();
    DismissAlarmToIdle();
    activeAlarmMinuteKey = ULLONG_MAX;
    RenderClock(window);
    AnimateBackground(window);
}

bool AlarmMinuteHasEnded(const SYSTEMTIME& now) {
    return alarmActive && alarmPanicActive &&
        AlarmMinuteKey(now) != activeAlarmMinuteKey;
}

void StartBlankPlayback(HWND window) {
    StopAlarmSound();
    BeginBlankPlayback();
    PlaySoundFile(L"blank1.wav");
    RenderClock(window);
    AnimateBackground(window);
}

void StopBlankPlayback(HWND window) {
    EndBlankPlayback();
    RenderClock(window);
    AnimateBackground(window);
}

void StartReaction(HWND window) {
    StopAlarmSound();
    BeginReaction();
    PlaySoundFile(L"smash.wav");
    RenderClock(window);
    AnimateBackground(window);
}

void StartCheekPull(HWND window) {
    StopAlarmSound();
    BeginCheekPull();
    PlaySoundFile(L"touch_start.wav");
    SetCapture(window);
    RenderClock(window);
    AnimateBackground(window);
}

void UpdateCheekPull(HWND window, PointF mouse) {
    if (!cheekDragging) return;
    cheekPull = ClampedCheekPull(mouse);
    RenderClock(window);
}

void FinishCheekPull(HWND window) {
    if (!cheekDragging) return;
    EndCheekPull();
    PlaySoundFile(L"touch_end.wav");
    RenderClock(window);
    AnimateBackground(window);
}

void FinishLayoutDrag(HWND window, bool releaseCapture) {
    if (layoutDrag == LayoutHandle::None) return;
    layoutDrag = LayoutHandle::None;
    if (releaseCapture && GetCapture() == window) ReleaseCapture();
    SaveCurrentLayout(window);
    RenderClock(window);
}

bool IsReactionPoint(float x, float y) {
    return std::hypot(x - clockCenter.X, y - clockCenter.Y) <= TouchRadius;
}

bool IsCheekPoint(float x, float y) {
    const PointF center = CheekTouchCenter();
    return std::hypot(x - center.X, y - center.Y) <= CheekTouchRadius;
}

PointF ClickPoint(HWND window, LPARAM position) {
    RECT rect;
    GetClientRect(window, &rect);
    if (rect.right <= 0 || rect.bottom <= 0) return PointF(-1, -1);
    return PointF(GET_X_LPARAM(position) * artworkWidth / static_cast<float>(rect.right),
        GET_Y_LPARAM(position) * artworkHeight / static_cast<float>(rect.bottom));
}

void UpdateClock(HWND window) {
    SYSTEMTIME now;
    GetLocalTime(&now);
    if (AlarmMinuteHasEnded(now)) DismissAlarm(window);
    const unsigned long long minuteKey = AlarmMinuteKey(now);
    if (minuteKey != lastAlarmMinuteKey) {
        for (const Alarm& alarm : alarms) {
            if (AlarmMatches(alarm, now)) {
                lastAlarmMinuteKey = minuteKey;
                if (!alarmActive) StartAlarm(window, now);
                break;
            }
        }
    }
    int step = now.wHour * 3600 + now.wMinute * 60 + now.wSecond;
    if (step == lastStep) return;
    lastStep = step;
    swprintf_s(tray.szTip, L"Skea clock · %04u-%02u-%02u %02u:%02u\n우클릭: 설정 및 종료",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute);
    Shell_NotifyIconW(NIM_MODIFY, &tray);
    RenderClock(window);
}

HMENU CreateVolumeMenu(int firstCommand, int selectedVolume) {
    HMENU menu = CreatePopupMenu();
    for (int volume = 0; volume <= 100; volume += 10) {
        const std::wstring label = std::to_wstring(volume) + L"%";
        AppendMenuW(menu, MF_STRING | (volume == selectedVolume ? MF_CHECKED : 0),
            firstCommand + volume / 10, label.c_str());
    }
    return menu;
}

void AppendSoundMenus(HMENU menu) {
    HMENU effectVolumeMenu = CreateVolumeMenu(EffectVolumeFirst, effectSoundVolume);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(effectVolumeMenu), L"효과음 크기 조절");

    HMENU alarmSoundMenu = CreatePopupMenu();
    AppendMenuW(alarmSoundMenu, MF_STRING, AlarmFileChoose, L"알람 파일 선택...");
    if (!customAlarmSoundPath.empty()) {
        const std::wstring name = FileName(customAlarmSoundPath);
        AppendMenuW(alarmSoundMenu, MF_STRING | (useCustomAlarmSound ? MF_CHECKED : 0),
            AlarmSoundCustom, name.c_str());
    }
    AppendMenuW(alarmSoundMenu, MF_STRING | (!useCustomAlarmSound ? MF_CHECKED : 0),
        AlarmSoundDefault, L"기본 알람");
    AppendMenuW(alarmSoundMenu, MF_SEPARATOR, 0, nullptr);
    HMENU alarmVolumeMenu = CreateVolumeMenu(AlarmVolumeFirst, alarmSoundVolume);
    AppendMenuW(alarmSoundMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(alarmVolumeMenu), L"크기 조절");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(alarmSoundMenu), L"알람 소리 조절");
}

void ShowMenu(HWND window, POINT point) {
    DismissAlarm(window);
    if (exceptionMenu) return;
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, L"Skea clock");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (showDial ? MF_CHECKED : 0), Background, L"배경 표시");
    AppendMenuW(menu, MF_STRING | (showTicks ? MF_CHECKED : 0), Ticks, L"눈금 표시");
    AppendMenuW(menu, MF_STRING | (showNumbers ? MF_CHECKED : 0), Numbers, L"숫자 표시");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, AlarmAddCommand, L"알람 추가");
    for (size_t i = 0; i < alarms.size(); ++i) {
        HMENU alarmMenu = CreatePopupMenu();
        AppendMenuW(alarmMenu, MF_STRING, AlarmEditFirst + static_cast<UINT>(i), L"알람 편집");
        AppendMenuW(alarmMenu, MF_STRING, AlarmDeleteFirst + static_cast<UINT>(i), L"알람 삭제");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(alarmMenu), AlarmMenuLabel(alarms[i]).c_str());
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendSoundMenus(menu);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (topmost ? MF_CHECKED : 0), AlwaysOnTop, L"항상 위에 표시");
    AppendTopmostExceptionMenu(menu);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (showTaskbar ? MF_CHECKED : 0), Taskbar, L"작업 표시줄 표시");
    AppendMenuW(menu, MF_STRING | (layoutEditing ? MF_CHECKED : 0), EditLayout, L"크기·위치 이동");
    HMENU displayMenu = CreatePopupMenu();
    const auto displays = GetDisplays();
    for (size_t i = 0; i < displays.size() && i < MaxDisplays; ++i) {
        const UINT flags = MF_STRING | (_wcsicmp(displays[i].info.szDevice, activeDisplay.c_str()) == 0 ? MF_CHECKED : 0);
        AppendMenuW(displayMenu, flags, DisplayFirst + static_cast<UINT>(i), DisplayLabel(displays[i]).c_str());
    }
    AppendMenuW(menu, MF_POPUP | (displays.empty() ? MF_GRAYED : 0),
        reinterpret_cast<UINT_PTR>(displayMenu), L"표시 디스플레이");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, Exit, L"종료");
    SetForegroundWindow(window);
    int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
        point.x, point.y, 0, window, nullptr);
    DestroyMenu(menu);
    CloseExceptionMenu();
    if (command) SendMessageW(window, WM_COMMAND, command, 0);
    PostMessageW(window, WM_NULL, 0, 0);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    static const UINT taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    if (message == taskbarCreated) {
        Shell_NotifyIconW(NIM_ADD, &tray);
        return 0;
    }
    if (message == AlarmEditorClosedMessage) {
        if (!alarmEditorWindow && !layoutEditing) StopBlankPlayback(window);
        return 0;
    }
    if (message == ExceptionMenuReadyMessage) {
        ApplyExceptionMenuResult(window);
        return 0;
    }
    if (message == WM_INITMENUPOPUP) {
        OpenExceptionMenu(window, reinterpret_cast<HMENU>(wParam));
        return 0;
    }
    if (message == ForegroundChangedMessage) {
        HWND foreground = reinterpret_cast<HWND>(wParam);
        const ForegroundKind kind = ClassifyForeground(window, foreground);
        if (kind != ForegroundKind::None) ApplyForegroundTransition(window, foreground);
        return 0;
    }
    if (message == TaskSwitchChangedMessage) {
        if (wParam == FALSE) {
            HWND foreground = reinterpret_cast<HWND>(lParam);
            if (!foreground || !IsWindow(foreground)) foreground = GetForegroundWindow();
            if (ClassifyForeground(window, foreground) != ForegroundKind::None)
                ApplyForegroundTransition(window, foreground);
        }
        return 0;
    }
    switch (message) {
    case WM_CREATE:
        tray.cbSize = sizeof(tray);
        tray.hWnd = window;
        tray.uID = 1;
        tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        tray.uCallbackMessage = TrayMessage;
        tray.hIcon = AppIcon();
        wcscpy_s(tray.szTip, L"Skea clock");
        Shell_NotifyIconW(NIM_ADD, &tray);
        SetTimer(window, 1, 100, nullptr);
        AnimateBackground(window);
        return 0;
    case WM_TIMER:
        if (wParam == 2) { AnimateBackground(window); return 0; }
        RecoverTopmostState(window);
        UpdateClock(window);
        CheckAlarmSoundValidation(window);
        return 0;
    case WM_TIMECHANGE:
        lastStep = -1;
        lastAlarmMinuteKey = ULLONG_MAX;
        UpdateClock(window);
        return 0;
    case WM_LBUTTONDOWN:
        {
            if (layoutEditing) {
                RECT client;
                GetClientRect(window, &client);
                const LayoutHandle handle = LayoutHitAt(client.right, client.bottom,
                    static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
                if (handle == LayoutHandle::Close) {
                    layoutEditing = false;
                    SaveCurrentLayout(window);
                    StopBlankPlayback(window);
                    PlaySoundFile(L"answer.wav");
                } else if (handle != LayoutHandle::None) {
                    BeginLayoutDrag(window, handle);
                }
                return 0;
            }
            const PointF point = ClickPoint(window, lParam);
            if (IsReactionPoint(point.X, point.Y)) {
                BeginSmashStarEffect(point);
                StartReaction(window);
            }
            else if (IsCheekPoint(point.X, point.Y)) StartCheekPull(window);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (layoutEditing) {
            if (layoutDrag != LayoutHandle::None) {
                UpdateLayoutDrag(window);
                RenderClock(window);
            } else {
                RECT client;
                GetClientRect(window, &client);
                SetCursor(LayoutCursor(LayoutHitAt(client.right, client.bottom,
                    static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)))));
            }
            return 0;
        }
        if (cheekDragging) UpdateCheekPull(window, ClickPoint(window, lParam));
        return 0;
    case WM_LBUTTONUP:
        if (layoutEditing && layoutDrag != LayoutHandle::None) {
            FinishLayoutDrag(window, true);
            return 0;
        }
        if (cheekDragging) {
            ReleaseCapture();
            FinishCheekPull(window);
        }
        return 0;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        if (layoutDrag != LayoutHandle::None) FinishLayoutDrag(window, false);
        FinishCheekPull(window);
        return 0;
    case WM_SETCURSOR:
        if (layoutEditing && LOWORD(lParam) == HTCLIENT) {
            POINT cursor;
            RECT client;
            GetCursorPos(&cursor);
            ScreenToClient(window, &cursor);
            GetClientRect(window, &client);
            SetCursor(LayoutCursor(LayoutHitAt(client.right, client.bottom,
                static_cast<float>(cursor.x), static_cast<float>(cursor.y))));
            return TRUE;
        }
        break;
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
    case WM_DPICHANGED:
        PositionClock(window);
        RenderClock(window);
        return 0;
    case WM_CONTEXTMENU: {
        POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (point.x == -1 && point.y == -1) GetCursorPos(&point);
        ShowMenu(window, point);
        return 0;
    }
    case TrayMessage:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            POINT point;
            GetCursorPos(&point);
            ShowMenu(window, point);
        } else if (lParam == WM_LBUTTONDBLCLK) {
            ShowWindow(window, SW_SHOWNOACTIVATE);
            ApplyTopmostState(window, true);
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == TopmostExceptionAdd) {
            if (ChooseTopmostException(window)) {
                SaveSettings();
                ApplyTopmostState(window, true);
            }
            return 0;
        }
        if (LOWORD(wParam) >= TopmostExceptionFirst &&
            LOWORD(wParam) < TopmostExceptionFirst + static_cast<int>(topmostExceptionMenuPaths.size())) {
            const size_t index = LOWORD(wParam) - TopmostExceptionFirst;
            if (ToggleTopmostException(topmostExceptionMenuPaths[index])) {
                SaveSettings();
                ApplyTopmostState(window, true);
            }
            return 0;
        }
        if (LOWORD(wParam) >= EffectVolumeFirst && LOWORD(wParam) <= EffectVolumeFirst + 10) {
            effectSoundVolume = (LOWORD(wParam) - EffectVolumeFirst) * 10;
            SaveSettings();
            return 0;
        }
        if (LOWORD(wParam) >= AlarmVolumeFirst && LOWORD(wParam) <= AlarmVolumeFirst + 10) {
            alarmSoundVolume = (LOWORD(wParam) - AlarmVolumeFirst) * 10;
            SaveSettings();
            return 0;
        }
        if (LOWORD(wParam) == AlarmFileChoose) {
            if (ChooseAlarmSoundFile(window)) {
                SaveSettings();
                BeginAlarmSoundValidation(window);
            }
            return 0;
        }
        if (LOWORD(wParam) == AlarmSoundDefault) {
            useCustomAlarmSound = false;
            SaveSettings();
            BeginAlarmSoundValidation(window);
            return 0;
        }
        if (LOWORD(wParam) == AlarmSoundCustom) {
            useCustomAlarmSound = !customAlarmSoundPath.empty();
            SaveSettings();
            BeginAlarmSoundValidation(window);
            return 0;
        }
        if (LOWORD(wParam) == AlarmAddCommand) {
            layoutEditing = false;
            StartBlankPlayback(window);
            OpenAlarmEditor(window);
            if (!alarmEditorWindow) StopBlankPlayback(window);
            return 0;
        }
        if (LOWORD(wParam) >= AlarmEditFirst &&
            LOWORD(wParam) < AlarmEditFirst + static_cast<int>(alarms.size())) {
            const size_t index = LOWORD(wParam) - AlarmEditFirst;
            const Alarm alarm = alarms[index];
            alarms.erase(alarms.begin() + index);
            SaveAlarms();
            layoutEditing = false;
            StartBlankPlayback(window);
            OpenAlarmEditor(window, &alarm);
            if (!alarmEditorWindow) StopBlankPlayback(window);
            return 0;
        }
        if (LOWORD(wParam) >= AlarmDeleteFirst &&
            LOWORD(wParam) < AlarmDeleteFirst + static_cast<int>(alarms.size())) {
            const size_t index = LOWORD(wParam) - AlarmDeleteFirst;
            alarms.erase(alarms.begin() + index);
            SaveAlarms();
            return 0;
        }
        if (LOWORD(wParam) == EditLayout) {
            layoutEditing = true;
            StartBlankPlayback(window);
            return 0;
        }
        if (LOWORD(wParam) >= DisplayFirst && LOWORD(wParam) < DisplayFirst + MaxDisplays) {
            const auto displays = GetDisplays();
            const int index = LOWORD(wParam) - DisplayFirst;
            if (index < static_cast<int>(displays.size())) {
                SaveCurrentLayout(window);
                selectedDisplay = displays[index].info.szDevice;
                PositionClock(window);
                SaveSettings();
                RenderClock(window);
            }
            return 0;
        }
        switch (LOWORD(wParam)) {
        case Background: showDial = !showDial; break;
        case Ticks: showTicks = !showTicks; break;
        case Numbers: showNumbers = !showNumbers; break;
        case AlwaysOnTop: topmost = !topmost; ApplyTopmostState(window, true); break;
        case Taskbar: showTaskbar = !showTaskbar; ApplyTaskbarSetting(window); break;
        case Exit: DestroyWindow(window); return 0;
        default: return 0;
        }
        SaveSettings();
        RenderClock(window);
        return 0;
    case WM_CLOSE:
        SaveCurrentLayout(window);
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        KillTimer(window, 1);
        KillTimer(window, 2);
        StopForegroundMonitor();
        CancelAlarmSoundValidation();
        ShutdownSoundSystem();
        Shell_NotifyIconW(NIM_DELETE, &tray);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\SkeaClock.SingleInstance");
    if (!mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(ClassName, L"Skea clock");
        if (existing) ShowWindow(existing, SW_SHOWNOACTIVATE);
        CloseHandle(mutex);
        return 0;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    wchar_t executable[MAX_PATH];
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    settingsPath = executable;
    settingsPath = settingsPath.substr(0, settingsPath.find_last_of(L"\\/") + 1) + L"skea-clock.ini";
    showDial = GetPrivateProfileIntW(L"clock", L"background", 1, settingsPath.c_str()) != 0;
    topmost = GetPrivateProfileIntW(L"clock", L"topmost", 1, settingsPath.c_str()) != 0;
    showTicks = GetPrivateProfileIntW(L"clock", L"ticks", 1, settingsPath.c_str()) != 0;
    showNumbers = GetPrivateProfileIntW(L"clock", L"numbers", 1, settingsPath.c_str()) != 0;
    LoadSoundSettings(settingsPath);
    LoadWindowBehaviorSettings(settingsPath);
    wchar_t display[MAX_PATH] = {};
    GetPrivateProfileStringW(L"clock", L"display", L"", display, MAX_PATH, settingsPath.c_str());
    selectedDisplay = display;
    LoadAlarms();

    GdiplusStartupInput input;
    ULONG_PTR token;
    if (GdiplusStartup(&token, &input, nullptr) != Ok) {
        CloseHandle(mutex);
        return 1;
    }
    std::wstring directory = settingsPath.substr(0, settingsPath.find_last_of(L"\\/") + 1);
    std::wstring sourceDirectory = directory + L"source\\";
    if (GetFileAttributesW((sourceDirectory + L"locate data\\clock-layout.ini").c_str()) == INVALID_FILE_ATTRIBUTES)
        sourceDirectory = directory + L"..\\source\\";
    SetSoundDirectory(sourceDirectory);
    if (!LoadArtwork(sourceDirectory)) {
        MessageBoxW(nullptr, L"GIF, 바늘 이미지 또는 locate data의 좌표 설정을 불러오지 못했습니다.\n\nbuild.cmd를 다시 실행하고, bin 폴더와 source 폴더를 함께 보관해 주세요.", L"Skea clock", MB_OK | MB_ICONERROR);
        ReleaseArtwork();
        GdiplusShutdown(token);
        CloseHandle(mutex);
        return 1;
    }
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = AppIcon();
    windowClass.lpszClassName = ClassName;
    RegisterClassW(&windowClass);
    HWND window = CreateWindowExW(ClockExtendedStyle(),
        ClassName, L"Skea clock", WS_POPUP, 0, 0, 240, 240, nullptr, nullptr, instance, nullptr);
    int result = 1;
    if (window) {
        PositionClock(window);
        SYSTEMTIME startupTime;
        GetLocalTime(&startupTime);
        InitializeAlarmMinuteTracking(startupTime);
        PlayRandomIdleSound();
        UpdateClock(window);
        ShowWindow(window, SW_SHOWNOACTIVATE);
        StartForegroundMonitor(window);
        ApplyTopmostState(window, true);
        BeginAlarmSoundValidation(window);
        MSG message;
        int status;
        while ((status = GetMessageW(&message, nullptr, 0, 0)) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        result = status == -1 ? 1 : static_cast<int>(message.wParam);
    } else {
        MessageBoxW(nullptr, L"시계 창을 만들지 못했습니다.", L"Skea clock", MB_OK | MB_ICONERROR);
    }
    renderBuffer.Release();
    ReleaseArtwork();
    GdiplusShutdown(token);
    CloseHandle(mutex);
    return result;
}
