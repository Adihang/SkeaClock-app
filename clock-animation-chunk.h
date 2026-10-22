#pragma once

struct AnimationFrameRegion { Rect bounds; PointF offset; };

struct AnimationChunk {
    // Pixel storage outlives the sheet and all of its frame views.
    std::vector<BYTE> pixels;
    std::unique_ptr<Bitmap> sheet;
    std::vector<std::unique_ptr<Bitmap>> views;

    bool Load(const std::wstring& path, const std::vector<AnimationFrameRegion>& regions, UINT first, UINT count) {
        Bitmap encoded(path.c_str());
        const UINT w = encoded.GetWidth(), h = encoded.GetHeight();
        if (encoded.GetLastStatus() != Ok || !w || !h || w > INT_MAX / 4 || h > INT_MAX / (w * 4)) return false;
        for (UINT i = first; i < first + count; ++i) {
            const Rect& bounds = regions[i].bounds;
            if (bounds.X > static_cast<INT>(w) - bounds.Width ||
                bounds.Y > static_cast<INT>(h) - bounds.Height) return false;
        }
        // A spare row also covers the trailing stride of the final frame view.
        pixels.resize(static_cast<size_t>(w) * (h + 1) * 4);
        sheet = std::make_unique<Bitmap>(w, h, static_cast<INT>(w * 4), PixelFormat32bppPARGB, pixels.data());
        if (sheet->GetLastStatus() != Ok) return false;
        {
            Graphics graphics(sheet.get());
            graphics.SetCompositingMode(CompositingModeSourceCopy);
            if (graphics.DrawImage(&encoded, Rect(0, 0, w, h), 0, 0, w, h, UnitPixel) != Ok) return false;
        }
        const UINT stride = w * 4;
        for (UINT i = first; i < first + count; ++i) {
            const Rect& bounds = regions[i].bounds;
            const size_t offset = static_cast<size_t>(bounds.Y) * stride + bounds.X * 4;
            auto view = std::make_unique<Bitmap>(bounds.Width, bounds.Height, static_cast<INT>(stride),
                PixelFormat32bppPARGB, pixels.data() + offset);
            if (view->GetLastStatus() != Ok) return false;
            views.push_back(std::move(view));
        }
        return true;
    }
};
