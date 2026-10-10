#pragma once

struct ClockRenderBuffer {
    HDC memory = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previous = nullptr;
    void* pixels = nullptr;
    int width = 0, height = 0;

    void Release() {
        if (previous) SelectObject(memory, previous);
        if (bitmap) DeleteObject(bitmap);
        if (memory) DeleteDC(memory);
        memory = nullptr;
        bitmap = nullptr;
        previous = nullptr;
        pixels = nullptr;
        width = height = 0;
    }

    bool Ensure(int newWidth, int newHeight) {
        if (newWidth <= 0 || newHeight <= 0) return false;
        if (bitmap && width == newWidth && height == newHeight) return true;
        Release();
        memory = CreateCompatibleDC(nullptr);
        if (!memory) return false;
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = newWidth;
        info.bmiHeader.biHeight = -newHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(memory, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap) { Release(); return false; }
        previous = SelectObject(memory, bitmap);
        if (!previous || previous == HGDI_ERROR) {
            previous = nullptr;
            Release();
            return false;
        }
        width = newWidth;
        height = newHeight;
        return true;
    }
};

ClockRenderBuffer renderBuffer;
