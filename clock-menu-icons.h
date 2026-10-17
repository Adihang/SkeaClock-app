#pragma once

HBITMAP CreateMenuIconBitmap(HICON icon, int width, int height) {
    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HDC screen = GetDC(nullptr);
    HBITMAP bitmap = CreateDIBSection(screen, &bitmapInfo, DIB_RGB_COLORS, &pixels, nullptr, 0);
    HDC memory = bitmap ? CreateCompatibleDC(screen) : nullptr;
    if (memory) {
        HGDIOBJ previous = SelectObject(memory, bitmap);
        DrawIconEx(memory, 0, 0, icon, width, height, 0, nullptr, DI_NORMAL);
        SelectObject(memory, previous);
        DeleteDC(memory);
    }
    ReleaseDC(nullptr, screen);
    if (!memory && bitmap) {
        DeleteObject(bitmap);
        bitmap = nullptr;
    }
    return bitmap;
}

struct MenuIcons {
    UINT dpi;
    std::vector<HBITMAP> bitmaps;
    ~MenuIcons() { for (HBITMAP bitmap : bitmaps) DeleteObject(bitmap); }

    void SetLast(HMENU menu, const wchar_t* resource) {
        int size = std::max(1, std::min(GetSystemMetricsForDpi(SM_CXSMICON, dpi),
            GetSystemMetricsForDpi(SM_CYSMICON, dpi)) / 2);
        NONCLIENTMETRICSW metrics = { sizeof(metrics) };
        if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi))
            size = std::min(size, std::max(1, std::abs(static_cast<int>(metrics.lfMenuFont.lfHeight))));
        HICON icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), resource, IMAGE_ICON,
            size, size, 0));
        if (!icon) return;
        HBITMAP bitmap = CreateMenuIconBitmap(icon, size, size);
        DestroyIcon(icon);
        if (!bitmap) return;
        bitmaps.push_back(bitmap);

        // Match native menu text, including Windows high contrast colors.
        BITMAP data = {};
        GetObjectW(bitmap, sizeof(data), &data);
        auto pixels = static_cast<BYTE*>(data.bmBits);
        const COLORREF color = GetSysColor(COLOR_MENUTEXT);
        for (int i = 0; i < data.bmWidth * data.bmHeight; ++i) {
            const BYTE alpha = pixels[i * 4 + 3];
            pixels[i * 4] = GetBValue(color) * alpha / 255;
            pixels[i * 4 + 1] = GetGValue(color) * alpha / 255;
            pixels[i * 4 + 2] = GetRValue(color) * alpha / 255;
        }
        MENUITEMINFOW item = { sizeof(item) };
        item.fMask = MIIM_BITMAP;
        item.hbmpItem = bitmap;
        SetMenuItemInfoW(menu, GetMenuItemCount(menu) - 1, TRUE, &item);
    }
};
