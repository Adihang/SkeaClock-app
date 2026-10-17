#pragma once
#include <random>

constexpr UINT_PTR PatHeartTimer = 4;
constexpr int PatHeartCount = 3;
constexpr UINT PatHeartSpawnMs = 650;
constexpr UINT PatHeartFadeInMs = 400;
constexpr UINT PatHeartFadeOutMs = PatHeartFadeInMs * 2;
constexpr UINT PatHeartDurationMs = PatHeartFadeInMs + PatHeartFadeOutMs;

struct PatHeart {
    PointF origin;
    float radius = 0;
    ULONGLONG start = 0;
    bool active = false;
};
PatHeart patHearts[PatHeartCount];
ULONGLONG nextPatHeartSpawn = 0;

bool AdvancePatHeartEffect(ULONGLONG now) {
    bool active = false;
    for (auto& heart : patHearts) {
        if (heart.active && now - heart.start >= PatHeartDurationMs) heart.active = false;
        active = active || heart.active;
    }
    if (patActive && now >= nextPatHeartSpawn) {
        static std::mt19937 generator(std::random_device{}());
        static std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        for (auto& heart : patHearts) {
            if (heart.active) continue;
            const float angle = unit(generator) * Pi * 2;
            const float radial = std::sqrt(unit(generator));
            const float dx = std::sin(angle) * radial * HeadTouchRadiusX * 1.5f;
            const float dy = -std::cos(angle) * radial * TouchRadius;
            heart = { PointF(clockCenter.X + dx, clockCenter.Y + dy),
                17.5f * (.75f + .5f * unit(generator)), now, true };
            nextPatHeartSpawn = now + PatHeartSpawnMs;
            active = true;
            break;
        }
    }
    return active || patActive;
}

void BeginPatHeartEffect(HWND window) {
    nextPatHeartSpawn = GetTickCount64();
    AdvancePatHeartEffect(nextPatHeartSpawn);
    SetTimer(window, PatHeartTimer, 33, nullptr);
}

void DrawPatHeartEffect(Graphics& graphics) {
    const ULONGLONG now = GetTickCount64();
    for (const auto& heart : patHearts) {
        if (!heart.active) continue;
        const ULONGLONG elapsed = now - heart.start;
        if (elapsed >= PatHeartDurationMs) continue;
        const float age = static_cast<float>(elapsed);
        const float opacity = elapsed < PatHeartFadeInMs ? age / PatHeartFadeInMs :
            1 - (age - PatHeartFadeInMs) / PatHeartFadeOutMs;
        const float x = heart.origin.X, y = heart.origin.Y - age * .044f;
        const float radius = heart.radius;
        GraphicsPath path;
        path.AddBezier(PointF(x, y + radius * .75f), PointF(x - radius * 1.4f, y - radius * .1f),
            PointF(x - radius * .8f, y - radius * 1.3f), PointF(x, y - radius * .45f));
        path.AddBezier(PointF(x, y - radius * .45f), PointF(x + radius * .8f, y - radius * 1.3f),
            PointF(x + radius * 1.4f, y - radius * .1f), PointF(x, y + radius * .75f));
        path.CloseFigure();
        SolidBrush fill(Color(static_cast<BYTE>(255 * opacity), 255, 95, 145));
        graphics.FillPath(&fill, &path);
    }
}
