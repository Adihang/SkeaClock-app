#pragma once
#include <algorithm>
#include <commdlg.h>
#include <iterator>

constexpr int Taskbar = 109;
constexpr int TopmostExceptionAdd = 350;
constexpr int TopmostExceptionFirst = 4000;
constexpr size_t MaximumTopmostExceptions = 32;
constexpr size_t MaximumTopmostMenuEntries = 512;
constexpr UINT ForegroundChangedMessage = WM_APP + 3;
constexpr UINT TaskSwitchChangedMessage = WM_APP + 4;

bool showTaskbar = false;
std::vector<std::wstring> topmostExceptions;
std::vector<std::wstring> topmostExceptionMenuPaths;
HWINEVENTHOOK foregroundEventHook = nullptr;
HWINEVENTHOOK taskSwitchEventHook = nullptr;
HWND foregroundEventClock = nullptr;

enum class ForegroundKind { None, Clock, Exception, Normal };
enum class ForegroundAction { None, MakeTopmost, MakeNotTopmost, PlaceBehind };

ForegroundKind previousForegroundKind = ForegroundKind::None;
bool recoverClockTopmost = false;

bool SameExecutablePath(const std::wstring& left, const std::wstring& right) {
    return _wcsicmp(left.c_str(), right.c_str()) == 0;
}

bool IsTopmostExceptionPath(const std::wstring& path) {
    for (const auto& exception : topmostExceptions)
        if (SameExecutablePath(exception, path)) return true;
    return false;
}

std::wstring WindowExecutablePath(HWND window) {
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (!processId || processId == GetCurrentProcessId()) return L"";
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) return L"";
    wchar_t path[32768] = {};
    DWORD length = static_cast<DWORD>(std::size(path));
    const bool found = QueryFullProcessImageNameW(process, 0, path, &length) != FALSE;
    CloseHandle(process);
    return found ? std::wstring(path, length) : L"";
}

void AddUniqueExecutablePath(std::vector<std::wstring>& paths, const std::wstring& path) {
    if (path.empty()) return;
    for (const auto& existing : paths)
        if (SameExecutablePath(existing, path)) return;
    paths.emplace_back(path);
}

BOOL CALLBACK CollectVisibleApplication(HWND window, LPARAM data) {
    if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER) ||
        (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) ||
        GetWindowTextLengthW(window) == 0) return TRUE;
    auto paths = reinterpret_cast<std::vector<std::wstring>*>(data);
    AddUniqueExecutablePath(*paths, WindowExecutablePath(window));
    return paths->size() < MaximumTopmostMenuEntries;
}

std::vector<std::wstring> BuildTopmostExceptionMenuPaths(
    const std::vector<std::wstring>& runningApplications, const std::vector<std::wstring>& savedExceptions) {
    std::vector<std::wstring> paths;
    for (const auto& path : runningApplications) AddUniqueExecutablePath(paths, path);
    for (const auto& path : savedExceptions) AddUniqueExecutablePath(paths, path);
    std::sort(paths.begin(), paths.end(), [](const auto& left, const auto& right) {
        const int byName = _wcsicmp(FileName(left).c_str(), FileName(right).c_str());
        return byName == 0 ? _wcsicmp(left.c_str(), right.c_str()) < 0 : byName < 0;
    });
    if (paths.size() > MaximumTopmostMenuEntries) paths.resize(MaximumTopmostMenuEntries);
    return paths;
}

std::vector<std::wstring> CurrentApplicationPaths() {
    std::vector<std::wstring> paths;
    EnumWindows(CollectVisibleApplication, reinterpret_cast<LPARAM>(&paths));
    return paths;
}

bool ToggleTopmostException(const std::wstring& path) {
    for (auto item = topmostExceptions.begin(); item != topmostExceptions.end(); ++item) {
        if (!SameExecutablePath(*item, path)) continue;
        topmostExceptions.erase(item);
        return true;
    }
    if (path.empty() || topmostExceptions.size() >= MaximumTopmostExceptions) return false;
    topmostExceptions.emplace_back(path);
    return true;
}

bool IsShellSurfaceClass(const wchar_t* className) {
    return _wcsicmp(className, L"Shell_TrayWnd") == 0 ||
        _wcsicmp(className, L"Shell_SecondaryTrayWnd") == 0 ||
        _wcsicmp(className, L"Progman") == 0 ||
        _wcsicmp(className, L"WorkerW") == 0;
}

bool IsFileExplorerClass(const wchar_t* className) {
    return _wcsicmp(className, L"CabinetWClass") == 0 ||
        _wcsicmp(className, L"ExploreWClass") == 0;
}

bool IsBuiltInShellProcess(const std::wstring& path) {
    const std::wstring name = FileName(path);
    return _wcsicmp(name.c_str(), L"explorer.exe") == 0 ||
        _wcsicmp(name.c_str(), L"ShellExperienceHost.exe") == 0 ||
        _wcsicmp(name.c_str(), L"StartMenuExperienceHost.exe") == 0 ||
        _wcsicmp(name.c_str(), L"SearchHost.exe") == 0 ||
        _wcsicmp(name.c_str(), L"SearchApp.exe") == 0;
}

bool IsShellSurface(HWND window) {
    const HWND root = GetAncestor(window, GA_ROOT);
    wchar_t className[256] = {};
    if (!root || !GetClassNameW(root, className, static_cast<int>(std::size(className))))
        return false;
    if (IsShellSurfaceClass(className)) return true;
    return !IsFileExplorerClass(className) &&
        IsBuiltInShellProcess(WindowExecutablePath(root));
}

ForegroundKind ClassifyForeground(HWND clock, HWND foreground) {
    if (!foreground || !IsWindow(foreground)) return ForegroundKind::None;
    if (IsShellSurface(foreground)) return ForegroundKind::None;
    DWORD processId = 0;
    GetWindowThreadProcessId(foreground, &processId);
    if (foreground == clock || processId == GetCurrentProcessId()) return ForegroundKind::Clock;
    return IsTopmostExceptionPath(WindowExecutablePath(foreground)) ?
        ForegroundKind::Exception : ForegroundKind::Normal;
}

ForegroundAction ActionForForegroundTransition(ForegroundKind previous,
    ForegroundKind current, bool force, bool clockIsTopmost) {
    if (current == ForegroundKind::None) return ForegroundAction::None;
    if (!topmost) return ForegroundAction::MakeNotTopmost;
    if (current == ForegroundKind::Clock)
        return force ? ForegroundAction::MakeTopmost : ForegroundAction::None;
    if (current == ForegroundKind::Normal) return ForegroundAction::MakeTopmost;
    if (force || previous != ForegroundKind::Exception || clockIsTopmost)
        return ForegroundAction::PlaceBehind;
    return ForegroundAction::None;
}

void SetClockTopmost(HWND clock, bool enabled, bool force = false) {
    const bool applied = (GetWindowLongPtrW(clock, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    if (!force && applied == enabled) return;
    SetWindowPos(clock, enabled ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void PlaceClockBehind(HWND clock, HWND foreground) {
    const HWND target = GetAncestor(foreground, GA_ROOT);
    if (!target || target == clock) return;
    SetClockTopmost(clock, false);
    SetWindowPos(clock, target, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void ApplyForegroundTransition(HWND clock, HWND foreground, bool force = false) {
    const ForegroundKind current = ClassifyForeground(clock, foreground);
    if (current == ForegroundKind::None) return;
    const bool clockIsTopmost = (GetWindowLongPtrW(clock, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    const ForegroundAction action = ActionForForegroundTransition(
        previousForegroundKind, current, force, clockIsTopmost);
    if (current == ForegroundKind::Normal) recoverClockTopmost = topmost;
    else if (current == ForegroundKind::Exception) recoverClockTopmost = false;
    else if (force) recoverClockTopmost = topmost;

    if (action == ForegroundAction::MakeTopmost) SetClockTopmost(clock, true, force);
    else if (action == ForegroundAction::MakeNotTopmost) SetClockTopmost(clock, false, force);
    else if (action == ForegroundAction::PlaceBehind) PlaceClockBehind(clock, foreground);
    previousForegroundKind = current;
}

void ApplyTopmostState(HWND clock, bool force) {
    ApplyForegroundTransition(clock, GetForegroundWindow(), force);
}

void RecoverTopmostState(HWND clock) {
    if (topmost && recoverClockTopmost &&
        (GetWindowLongPtrW(clock, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0)
        SetClockTopmost(clock, true);
}

void CALLBACK ForegroundEventProc(HWINEVENTHOOK, DWORD event, HWND foreground,
    LONG, LONG, DWORD, DWORD) {
    if (!foregroundEventClock) return;
    if (event == EVENT_SYSTEM_FOREGROUND)
        PostMessageW(foregroundEventClock, ForegroundChangedMessage,
            reinterpret_cast<WPARAM>(foreground), 0);
    else if (event == EVENT_SYSTEM_SWITCHSTART)
        PostMessageW(foregroundEventClock, TaskSwitchChangedMessage, TRUE,
            reinterpret_cast<LPARAM>(foreground));
    else if (event == EVENT_SYSTEM_SWITCHEND)
        PostMessageW(foregroundEventClock, TaskSwitchChangedMessage, FALSE,
            reinterpret_cast<LPARAM>(foreground));
}

void StopForegroundMonitor();

void StartForegroundMonitor(HWND clock) {
    foregroundEventClock = clock;
    foregroundEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, ForegroundEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    taskSwitchEventHook = SetWinEventHook(EVENT_SYSTEM_SWITCHSTART, EVENT_SYSTEM_SWITCHEND,
        nullptr, ForegroundEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (foregroundEventHook && taskSwitchEventHook) return;
    StopForegroundMonitor();
}

void StopForegroundMonitor() {
    if (foregroundEventHook) UnhookWinEvent(foregroundEventHook);
    if (taskSwitchEventHook) UnhookWinEvent(taskSwitchEventHook);
    foregroundEventHook = nullptr;
    taskSwitchEventHook = nullptr;
    foregroundEventClock = nullptr;
}

DWORD ClockExtendedStyle() {
    return WS_EX_LAYERED | WS_EX_NOACTIVATE | (topmost ? WS_EX_TOPMOST : 0) |
        (showTaskbar ? WS_EX_APPWINDOW : WS_EX_TOOLWINDOW);
}

void ApplyTaskbarSetting(HWND window) {
    LONG_PTR style = GetWindowLongPtrW(window, GWL_EXSTYLE);
    style &= ~(WS_EX_APPWINDOW | WS_EX_TOOLWINDOW);
    style |= showTaskbar ? WS_EX_APPWINDOW : WS_EX_TOOLWINDOW;
    const bool visible = IsWindowVisible(window) != FALSE;
    if (visible) ShowWindow(window, SW_HIDE);
    SetWindowLongPtrW(window, GWL_EXSTYLE, style);
    SetWindowPos(window, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED);
    if (visible) ShowWindow(window, SW_SHOWNOACTIVATE);
    ApplyTopmostState(window, true);
}

void SaveWindowBehaviorSettings(const std::wstring& path) {
    WritePrivateProfileStringW(L"clock", L"taskbar", showTaskbar ? L"1" : L"0", path.c_str());
    const int previousCount = GetPrivateProfileIntW(L"topmostExceptions", L"count", 0, path.c_str());
    WritePrivateProfileStringW(L"topmostExceptions", L"count",
        std::to_wstring(topmostExceptions.size()).c_str(), path.c_str());
    for (size_t i = 0; i < topmostExceptions.size(); ++i) {
        const std::wstring key = L"path" + std::to_wstring(i);
        WritePrivateProfileStringW(L"topmostExceptions", key.c_str(),
            topmostExceptions[i].c_str(), path.c_str());
    }
    for (int i = static_cast<int>(topmostExceptions.size()); i < previousCount; ++i) {
        const std::wstring key = L"path" + std::to_wstring(i);
        WritePrivateProfileStringW(L"topmostExceptions", key.c_str(), nullptr, path.c_str());
    }
}

void LoadWindowBehaviorSettings(const std::wstring& path) {
    showTaskbar = GetPrivateProfileIntW(L"clock", L"taskbar", 0, path.c_str()) != 0;
    topmostExceptions.clear();
    const int count = std::clamp(static_cast<int>(GetPrivateProfileIntW(
        L"topmostExceptions", L"count", 0, path.c_str())),
        0, static_cast<int>(MaximumTopmostExceptions));
    for (int i = 0; i < count; ++i) {
        wchar_t value[32768] = {};
        const std::wstring key = L"path" + std::to_wstring(i);
        GetPrivateProfileStringW(L"topmostExceptions", key.c_str(), L"", value,
            static_cast<DWORD>(std::size(value)), path.c_str());
        if (*value && !IsTopmostExceptionPath(value)) topmostExceptions.emplace_back(value);
    }
}

bool ChooseTopmostException(HWND owner) {
    wchar_t file[32768] = {};
    OPENFILENAMEW dialog = { sizeof(dialog) };
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"프로그램\0*.exe\0모든 파일\0*.*\0\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = static_cast<DWORD>(std::size(file));
    dialog.lpstrTitle = L"최상단 고정 예외 프로그램 선택";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) return false;
    return ToggleTopmostException(file);
}
