#pragma once
#include <atomic>

constexpr UINT ExceptionMenuReadyMessage = WM_APP + 5;

struct ExceptionMenuLoad {
    HWND window = nullptr;
    UINT dpi = 96;
    std::vector<std::wstring> paths;
    std::vector<HBITMAP> icons;
    std::atomic<bool> cancelled{ false }, complete{ false };
    ~ExceptionMenuLoad() { for (HBITMAP icon : icons) if (icon) DeleteObject(icon); }
};

HMENU exceptionMenu = nullptr;
bool exceptionMenuRequested = false;
std::shared_ptr<ExceptionMenuLoad> exceptionMenuPending, exceptionMenuData;

HBITMAP CreateMenuIconBitmap(const std::wstring& path, UINT dpi);

void CALLBACK LoadExceptionMenu(PTP_CALLBACK_INSTANCE, void* context) {
    std::unique_ptr<std::shared_ptr<ExceptionMenuLoad>> holder(
        static_cast<std::shared_ptr<ExceptionMenuLoad>*>(context));
    const auto job = *holder;
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        if (!job->cancelled) {
            job->paths = BuildTopmostExceptionMenuPaths(CurrentApplicationPaths(), job->paths);
            job->icons.reserve(job->paths.size());
            for (const auto& path : job->paths) {
                if (job->cancelled) break;
                job->icons.push_back(CreateMenuIconBitmap(path, job->dpi));
            }
        }
    } catch (...) {
        job->paths.clear();
    }
    if (SUCCEEDED(com)) CoUninitialize();
    job->complete.store(true, std::memory_order_release);
    PostMessageW(job->window, ExceptionMenuReadyMessage, 0, 0);
}

void StartExceptionMenuLoad(HWND window) {
    // At most one outstanding query, including work from a menu that was closed.
    if (!exceptionMenu || exceptionMenuPending || exceptionMenuData) return;
    auto job = std::make_shared<ExceptionMenuLoad>();
    job->window = window;
    job->dpi = GetDpiForWindow(window);
    job->paths = topmostExceptions;
    auto holder = std::make_unique<std::shared_ptr<ExceptionMenuLoad>>(job);
    exceptionMenuPending = job;
    if (TrySubmitThreadpoolCallback(LoadExceptionMenu, holder.get(), nullptr)) holder.release();
    else exceptionMenuPending.reset();
}

void OpenExceptionMenu(HWND window, HMENU menu) {
    if (!exceptionMenu || menu != exceptionMenu || exceptionMenuRequested) return;
    exceptionMenuRequested = true;
    StartExceptionMenuLoad(window);
}

void ApplyExceptionMenuResult(HWND window) {
    if (!exceptionMenuPending || !exceptionMenuPending->complete.load(std::memory_order_acquire)) return;
    auto job = std::move(exceptionMenuPending);
    if (job->cancelled || !exceptionMenu) {
        if (exceptionMenu && exceptionMenuRequested) StartExceptionMenuLoad(window);
        return;
    }
    exceptionMenuData = job;
    topmostExceptionMenuPaths = job->paths;
    if (job->paths.empty()) return; // Keep the disabled placeholder.
    DeleteMenu(exceptionMenu, 0, MF_BYPOSITION);
    for (size_t i = 0; i < job->paths.size(); ++i) {
        const bool selected = IsTopmostExceptionPath(job->paths[i]);
        const std::wstring name = (selected ? L"✓ " : L"") + FileName(job->paths[i]);
        MENUITEMINFOW item = { sizeof(item) };
        item.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE | MIIM_BITMAP;
        item.wID = TopmostExceptionFirst + static_cast<UINT>(i);
        item.fState = selected ? MFS_CHECKED : MFS_ENABLED;
        item.dwTypeData = const_cast<wchar_t*>(name.c_str());
        item.hbmpItem = job->icons[i];
        InsertMenuItemW(exceptionMenu, static_cast<UINT>(i), TRUE, &item);
    }
}

void AppendTopmostExceptionMenu(HMENU menu) {
    exceptionMenu = CreatePopupMenu();
    exceptionMenuRequested = false;
    topmostExceptionMenuPaths.clear();
    AppendMenuW(exceptionMenu, MF_STRING | MF_GRAYED, 0, UiText().noPrograms);
    AppendMenuW(exceptionMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(exceptionMenu, MF_STRING, TopmostExceptionAdd, UiText().programChoose);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(exceptionMenu), UiText().topmostExceptions);
}

// Call after DestroyMenu: item bitmaps must outlive the menu that refers to them.
void CloseExceptionMenu() {
    exceptionMenu = nullptr;
    exceptionMenuRequested = false;
    if (exceptionMenuPending) exceptionMenuPending->cancelled = true;
    exceptionMenuData.reset();
}

HBITMAP CreateMenuIconBitmap(const std::wstring& path, UINT dpi) {
    SHFILEINFOW fileInfo = {};
    HICON icon = nullptr;
    bool destroyIcon = false;
    if (SHGetFileInfoW(path.c_str(), 0, &fileInfo, sizeof(fileInfo),
        SHGFI_ICON | SHGFI_SMALLICON)) {
        icon = fileInfo.hIcon;
        destroyIcon = true;
    } else {
        icon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    if (!icon) return nullptr;

    HBITMAP bitmap = CreateMenuIconBitmap(icon,
        std::max(16, GetSystemMetricsForDpi(SM_CXSMICON, dpi)),
        std::max(16, GetSystemMetricsForDpi(SM_CYSMICON, dpi)));
    if (destroyIcon) DestroyIcon(icon);
    return bitmap;
}
