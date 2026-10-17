#pragma once

constexpr int CheekRows = 20, CheekColumns = 14;

// Dimensionless geometry is independent of the current frame and mouse position.
struct CheekWarpGrid {
    float vertical[CheekRows + 1], chord[CheekRows + 1];
    float column[CheekColumns + 1], weight[CheekRows + 1][CheekColumns + 1];
    CheekWarpGrid() {
        for (int col = 0; col <= CheekColumns; ++col)
            column[col] = col / static_cast<float>(CheekColumns);
        for (int row = 0; row <= CheekRows; ++row) {
            vertical[row] = -1.0f + 2.0f * row / CheekRows;
            chord[row] = std::sqrt(std::max(0.0f, 1 - vertical[row] * vertical[row]));
            for (int col = 0; col <= CheekColumns; ++col) {
                const float w = 1 - column[col];
                weight[row][col] = w * w * (3 - 2 * w) * chord[row];
            }
        }
    }
};
const CheekWarpGrid cheekWarpGrid;

struct CheekWarpBuffers {
    std::unique_ptr<Bitmap> source, result, composite;
    PointF compositeOffset;
    Bitmap* sourceHead = nullptr;
    UINT sourceFrame = UINT_MAX;
    PointF sourceOffset;
    bool geometryReady = false;
    PointF center;
    float radius = 0;
    PointF original[CheekRows + 1][CheekColumns + 1];

    void Release() {
        source.reset();
        result.reset();
        composite.reset();
        sourceHead = nullptr;
        sourceFrame = UINT_MAX;
        geometryReady = false;
    }

    bool Prepare(UINT width, UINT height, Bitmap& head, PointF offset, UINT frame) {
        if (!source || source->GetWidth() != width || source->GetHeight() != height) {
            Release();
            source = std::make_unique<Bitmap>(width, height, PixelFormat32bppPARGB);
            result = std::make_unique<Bitmap>(width, height, PixelFormat32bppPARGB);
            if (source->GetLastStatus() != Ok || result->GetLastStatus() != Ok) {
                Release();
                return false;
            }
        }
        if (sourceHead != &head || sourceFrame != frame || sourceOffset.X != offset.X || sourceOffset.Y != offset.Y) {
            Graphics graphics(source.get());
            graphics.Clear(Color(0, 0, 0, 0));
            graphics.SetCompositingMode(CompositingModeSourceCopy);
            if (graphics.DrawImage(&head, static_cast<INT>(offset.X), static_cast<INT>(offset.Y),
                head.GetWidth(), head.GetHeight()) != Ok) return false;
            sourceHead = &head;
            sourceFrame = frame;
            sourceOffset = offset;
        }
        return true;
    }

    Bitmap* ComposeLayers(AnimationImage& back, Bitmap* head, AnimationImage& front, PointF frontOffset) {
        const PointF crop = front.FrameOffset();
        frontOffset = PointF(frontOffset.X + crop.X, frontOffset.Y + crop.Y);
        Bitmap* frontFrame = front.Frame();
        Bitmap* backFrame = back.Frame();
        if (!frontFrame || !backFrame) return nullptr;
        const INT left = static_cast<INT>(std::floor(std::min(0.0f, frontOffset.X)));
        const INT top = static_cast<INT>(std::floor(std::min(0.0f, frontOffset.Y)));
        const UINT width = static_cast<UINT>(std::ceil(std::max(static_cast<float>(back.GetWidth()),
            frontOffset.X + frontFrame->GetWidth())) - left);
        const UINT height = static_cast<UINT>(std::ceil(std::max(static_cast<float>(back.GetHeight()),
            frontOffset.Y + frontFrame->GetHeight())) - top);
        if (!composite || composite->GetWidth() != width || composite->GetHeight() != height) {
            composite = std::make_unique<Bitmap>(width, height, PixelFormat32bppPARGB);
            if (composite->GetLastStatus() != Ok) {
                composite.reset();
                return nullptr;
            }
        }
        compositeOffset = PointF(static_cast<float>(left), static_cast<float>(top));
        Graphics graphics(composite.get());
        graphics.Clear(Color(0, 0, 0, 0));
        graphics.SetInterpolationMode(InterpolationModeNearestNeighbor);
        const PointF backOffset = back.FrameOffset();
        if (graphics.DrawImage(backFrame, static_cast<INT>(backOffset.X) - left,
            static_cast<INT>(backOffset.Y) - top, backFrame->GetWidth(), backFrame->GetHeight()) != Ok) return nullptr;
        if (head && graphics.DrawImage(head, -left, -top, head->GetWidth(), head->GetHeight()) != Ok) return nullptr;
        if (graphics.DrawImage(frontFrame, static_cast<INT>(frontOffset.X) - left,
            static_cast<INT>(frontOffset.Y) - top, frontFrame->GetWidth(), frontFrame->GetHeight()) != Ok) return nullptr;
        return composite.get();
    }

    void PrepareGeometry(PointF newCenter, float newRadius) {
        if (geometryReady && center.X == newCenter.X && center.Y == newCenter.Y && radius == newRadius) return;
        center = newCenter;
        radius = newRadius;
        const float x = center.X - .5f, y = center.Y - .5f;
        for (int row = 0; row <= CheekRows; ++row) {
            const float sourceY = y + cheekWarpGrid.vertical[row] * radius;
            const float left = x - radius * cheekWarpGrid.chord[row];
            const float right = x + radius * cheekWarpGrid.chord[row];
            for (int col = 0; col <= CheekColumns; ++col)
                original[row][col] = PointF(left + (right - left) * cheekWarpGrid.column[col], sourceY);
        }
        geometryReady = true;
    }
};
CheekWarpBuffers cheekWarpBuffers;
