#pragma once
#include <commctrl.h>
#include <atomic>
#include <cstdlib>
#include "clock-update-release.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

constexpr int AutoUpdateCheck = 112, GithubVisit = 113;
bool autoCheckUpdates = true;

struct UpdateCheck {
    std::wstring path, tag;
    std::atomic<bool> complete{ false };
};
std::shared_ptr<UpdateCheck> updateCheck;

void CALLBACK LoadLatestUpdate(PTP_CALLBACK_INSTANCE, void* context) {
    std::unique_ptr<std::shared_ptr<UpdateCheck>> holder(static_cast<std::shared_ptr<UpdateCheck>*>(context));
    const auto job = *holder;
    try {
        job->tag = RequestLatestUpdateTag(job->path);
    } catch (...) {
        job->tag.clear();
    }
    job->complete.store(true, std::memory_order_release);
}

void BeginUpdateCheck() {
    if (!autoCheckUpdates || updateCheck) return;
    wchar_t previous[32] = {};
    GetPrivateProfileStringW(L"updates", L"lastCheck", L"0", previous, 32, settingsPath.c_str());
    const long long lastCheck = _wcstoi64(previous, nullptr, 10);
    if (lastCheck > 0 && UpdateCheckTime() - lastCheck < UpdateCheckInterval) return;
    auto job = std::make_shared<UpdateCheck>();
    job->path = settingsPath;
    auto holder = std::make_unique<std::shared_ptr<UpdateCheck>>(job);
    updateCheck = job;
    if (TrySubmitThreadpoolCallback(LoadLatestUpdate, holder.get(), nullptr)) holder.release();
    else updateCheck.reset();
}

void CheckUpdateResult(HWND window) {
    if (!updateCheck || !updateCheck->complete.load(std::memory_order_acquire)) return;
    const auto job = std::move(updateCheck);
    if (!autoCheckUpdates || job->tag.empty()) return;
    wchar_t ignored[2048] = {};
    GetPrivateProfileStringW(L"updates", L"ignoredTag", L"", ignored, 2048, settingsPath.c_str());
    if (job->tag == ignored) return;
    UpdateVersion latest = {}, current = {};
    if (!ParseUpdateVersion(job->tag, latest) || !ReadProductVersion(current) || latest <= current) return;

    const TASKDIALOG_BUTTON buttons[] = {
        { IDYES, UiText().updateVisitGithub },
        { IDNO, UiText().updateIgnore },
        { IDCANCEL, UiText().updateCancel },
    };
    TASKDIALOGCONFIG dialog = {};
    dialog.cbSize = sizeof(dialog);
    dialog.hwndParent = window;
    dialog.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    dialog.pszWindowTitle = L"Skea clock";
    dialog.pszMainIcon = TD_INFORMATION_ICON;
    dialog.pszContent = UiText().updateAvailable;
    dialog.cButtons = static_cast<UINT>(std::size(buttons));
    dialog.pButtons = buttons;
    dialog.nDefaultButton = IDCANCEL;
    int button = IDCANCEL;
    if (FAILED(TaskDialogIndirect(&dialog, &button, nullptr, nullptr))) return;
    if (button == IDYES) {
        ShellExecuteW(window, L"open", GithubLatestReleaseUrl, nullptr, nullptr, SW_SHOWNORMAL);
    } else if (button == IDNO) {
        WritePrivateProfileStringW(L"updates", L"ignoredTag", job->tag.c_str(), settingsPath.c_str());
    }
}
