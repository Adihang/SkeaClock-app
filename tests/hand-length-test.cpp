#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>

static ULONGLONG testTick = 1000;
static ULONGLONG TestTickCount64() { return testTick; }
#define GetTickCount64 TestTickCount64
#include "../skea-clock.cpp"
#undef GetTickCount64

#include <cassert>
#include <cstdio>

void CheckNear(float actual, float expected) {
    assert(std::fabs(actual - expected) < .0001f);
}

void CheckLengths(ULONGLONG tick, float hour, float minute) {
    testTick = tick;
    CheckNear(HandLengthScale(0, tick), hour);
    CheckNear(HandLengthScale(1, tick), minute);
}

void ResetHands() {
    reactionActive = showingAngry = showingAngryAlt = showingTransition = false;
    cheekDragging = touchReleaseActive = blankPlaybackActive = false;
    alarmActive = alarmPanicActive = handReturnActive = false;
    testTick = animationStart = handReturnStart = 1000;
    for (int i = 0; i < 2; ++i) {
        handReturnStartLengthScales[i] = alarmHandStartLengthScales[i] = 1.0f;
    }
}

void CheckTransforms() {
    ClockHand hand;
    hand.center = PointF(25, 30);
    hand.tip = PointF(13, 14);
    hand.nativeAngle = std::atan2(-12.0f, 16.0f) * 180 / Pi;
    Matrix original;
    HandTransform(original, hand, .25f, clockCenter);
    PointF originalPoints[] = { hand.center, hand.tip };
    original.TransformPoints(originalPoints, 2);
    CheckNear(originalPoints[0].X, clockCenter.X);
    CheckNear(originalPoints[0].Y, clockCenter.Y);
    CheckNear(originalPoints[1].X, clockCenter.X + 18);
    CheckNear(originalPoints[1].Y, clockCenter.Y);

    const PointF centers[] = { clockCenter, PointF(117, 124), PointF(126, 112) };
    for (float length : { .95f, 1.0f, 1.05f }) {
        const PointF fixedTip(clockCenter.X + 18 * length, clockCenter.Y);
        for (PointF center : centers) {
            Matrix transform;
            StretchedHandTransform(transform, hand, center, fixedTip);
            PointF points[] = { hand.center, hand.tip,
                PointF(hand.center.X + .8f, hand.center.Y - .6f) };
            transform.TransformPoints(points, 3);
            CheckNear(points[0].X, center.X);
            CheckNear(points[0].Y, center.Y);
            CheckNear(points[1].X, fixedTip.X);
            CheckNear(points[1].Y, fixedTip.Y);
            CheckNear(std::hypot(points[2].X - points[0].X,
                points[2].Y - points[0].Y), .9f);
        }
    }
}

int main() {
    GdiplusStartupInput input;
    ULONG_PTR token = 0;
    assert(GdiplusStartup(&token, &input, nullptr) == Ok);
    gifImage = std::make_unique<Bitmap>(240, 240, PixelFormat32bppPARGB);
    assert(gifImage->GetLastStatus() == Ok);
    SYSTEMTIME now = {};
    now.wHour = 10;
    now.wMinute = 10;

    ResetHands();
    CheckLengths(1000, .95f, 1.05f);
    BeginAlarm(now);
    CheckLengths(1000, .95f, 1.05f);
    CheckLengths(1050, .975f, 1.025f);
    CheckLengths(1100, 1.0f, 1.0f);
    CheckLengths(1200, 1.0f, 1.0f);
    alarmPanicActive = true;
    CheckLengths(1200, 1.0f, 1.0f);
    assert(!HandsVisible());

    testTick = 2000;
    DismissAlarmToIdle();
    CheckLengths(2000, 1.0f, 1.0f);
    CheckLengths(2375, .975f, 1.025f);
    CheckLengths(2750, .95f, 1.05f);
    CheckLengths(3000, .95f, 1.05f);

    // An alarm interrupts a return without changing the current lengths.
    ResetHands();
    alarmActive = alarmPanicActive = true;
    testTick = 2000;
    DismissAlarmToIdle();
    testTick = 2375;
    BeginAlarm(now);
    CheckLengths(2375, .975f, 1.025f);
    CheckLengths(2425, .9875f, 1.0125f);
    CheckLengths(2475, 1.0f, 1.0f);

    // Dismissing before the hands gather returns from the visible lengths.
    ResetHands();
    BeginAlarm(now);
    CheckLengths(1050, .975f, 1.025f);
    DismissAlarmToIdle();
    CheckLengths(1050, .975f, 1.025f);
    CheckLengths(1425, .9625f, 1.0375f);
    CheckLengths(1800, .95f, 1.05f);

    ResetHands();
    cheekDragging = true;
    CheckLengths(1000, 1.0f, 1.0f);
    BeginAlarm(now);
    CheckLengths(1000, 1.0f, 1.0f);
    CheckLengths(1050, 1.0f, 1.0f);
    ResetHands();
    touchReleaseActive = true;
    CheckLengths(1000, 1.0f, 1.0f);
    CompleteTouchRelease(testTick);
    assert(handReturnActive && !touchReleaseActive);
    CheckLengths(1000, 1.0f, 1.0f);
    CheckLengths(1375, .975f, 1.025f);
    CheckLengths(1750, .95f, 1.05f);

    ResetHands();
    reactionActive = true;
    CheckLengths(1000, 1.0f, 1.0f);
    reactionActive = false;
    blankPlaybackActive = true;
    CheckLengths(1000, 1.0f, 1.0f);

    CheckNear(HandAnimationProgress(-.5f), 0);
    CheckNear(HandAnimationProgress(.25f), .15625f);
    CheckNear(HandAnimationProgress(.5f), .5f);
    CheckNear(HandAnimationProgress(.75f), .84375f);
    CheckNear(HandAnimationProgress(1.5f), 1);
    CheckNear(AnimatedHandTurn(.2f, .4f, -.5f), .2f);
    CheckNear(AnimatedHandTurn(.2f, .4f, .25f), .23125f);
    CheckNear(AnimatedHandTurn(.2f, .4f, .75f), .36875f);
    CheckNear(AnimatedHandTurn(.2f, .4f, 1.5f), .4f);
    CheckNear(AnimatedHandTurn(.95f, .05f, .5f), 1.0f);
    CheckNear(AnimatedHandTurn(.1f, .8f, .5f), -.05f);
    CheckTransforms();

    ReleaseArtwork();
    GdiplusShutdown(token);
    std::puts("Hand length checks passed.");
}
