#pragma once

constexpr UINT_PTR HeadHoldTimer = 3;
constexpr UINT PatHoldMs = 200;
constexpr float PatMoveTolerancePx = 5.0f;
bool headPressPending = false;
ULONGLONG headPressStart = 0;
PointF headPressPoint;
PointF headPressMouse;
POINT headPressClientPoint = {};

void StartPat(HWND window) {
    KillTimer(window, HeadHoldTimer);
    headPressPending = false;
    StopAlarmSound();
    smashStarActive = false;
    BeginPat();
    BeginPatHeartEffect(window);
    PlaySoundFile(L"pat_start.wav", EffectSoundCategory::Start);
    patPull = ClampedPatPull(headPressMouse);
    RenderClock(window);
    AnimateBackground(window);
}

void UpdateHeadPress(HWND window, LPARAM position) {
    if (!headPressPending && !patActive) return;
    headPressMouse = ClickPoint(window, position);
    if (headPressPending && std::hypot(GET_X_LPARAM(position) - headPressClientPoint.x,
        GET_Y_LPARAM(position) - headPressClientPoint.y) > PatMoveTolerancePx) {
        StartPat(window);
        return;
    }
    if (patActive) {
        patPull = ClampedPatPull(headPressMouse);
        RenderClock(window);
    }
}

#ifdef SKEA_TEST_BUILD
void DrawHeadPressRange(Graphics& graphics) {
    if (!headPressPending && !patActive) return;
    SolidBrush fill(Color(90, 255, 215, 0));
    Pen border(Color(255, 255, 215, 0), 1.0f);
    const float left = static_cast<float>(headPressClientPoint.x) - PatMoveTolerancePx;
    const float top = static_cast<float>(headPressClientPoint.y) - PatMoveTolerancePx;
    graphics.FillEllipse(&fill, left, top, PatMoveTolerancePx * 2, PatMoveTolerancePx * 2);
    graphics.DrawEllipse(&border, left, top, PatMoveTolerancePx * 2, PatMoveTolerancePx * 2);
}
#endif

void FinishHeadPress(HWND window, bool released) {
    KillTimer(window, HeadHoldTimer);
    const bool pending = headPressPending;
    const ULONGLONG elapsed = GetTickCount64() - headPressStart;
    headPressPending = false;
    if (pending && released) {
        if (elapsed < PatHoldMs) {
            BeginSmashStarEffect(headPressPoint);
            StartReaction(window);
            return;
        }
        StartPat(window);
    }
    if (!patActive) return;
    EndPat();
    PlayInteractionEndSound(L"pat");
    RenderClock(window);
    AnimateBackground(window);
}
