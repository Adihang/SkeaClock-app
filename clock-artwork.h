#pragma once
#include <algorithm>
#include <climits>
#include <memory>

constexpr float DialRadius = 108.0f;
constexpr float TouchRadius = DialRadius * 0.3f;
constexpr float CheekTouchRadius = TouchRadius * 0.6f;
constexpr float CharacterScale = 0.8f;
constexpr float HandScale = 0.9f;
constexpr float MaxInwardCheekPull = 0.3f;
constexpr float CheekSeamOverlap = 1.0f;
constexpr UINT HandReturnMs = 750;
constexpr UINT SmashStartFrame = 4;
constexpr UINT AngryCutFrame = 14;
constexpr UINT AngryAltStartFrame = 23;
constexpr UINT TransitionStartFrame = 86;
constexpr UINT TransitionEndFrame = 109;
constexpr UINT IdleReturnFrame = 71;
constexpr UINT TouchBounceFrameCount = 6;
constexpr UINT AlarmHandGatherMs = 100;
constexpr float AlarmHandTurn = 1.0f / 12.0f;

#include "clock-dial.h"
#include "clock-cheek-warp.h"

struct ClockHand {
    std::unique_ptr<Bitmap> image;
    PointF center, tip;
    float nativeAngle = 0;
};
std::unique_ptr<Bitmap> gifImage;
std::unique_ptr<Bitmap> transitionImage;
std::unique_ptr<Bitmap> smashImage;
std::unique_ptr<Bitmap> angryImage;
std::unique_ptr<Bitmap> angryAltImage;
std::unique_ptr<Bitmap> panicImage;
std::unique_ptr<Bitmap> blankImage;
std::unique_ptr<Bitmap> touchBackImage;
std::unique_ptr<Bitmap> touchHeadImage;
std::unique_ptr<Bitmap> touchFrontImage;
std::unique_ptr<Bitmap> touchEndBackImage;
std::unique_ptr<Bitmap> touchEndHeadImage;
std::unique_ptr<Bitmap> touchEndFrontImage;
std::unique_ptr<Bitmap> touchEndImage;
ClockHand clockHands[2];
PointF clockCenter(120, 120);
PointF smashOffset, angryOffset, angryAltOffset;
PointF panicOffset;
PointF blankOffset;
PointF touchHeadOffset, touchFrontOffset;
PointF touchEndHeadOffset, touchEndFrontOffset;
PointF cheekTouchSource;
PointF cheekPull;
PointF touchReleasePullSource;
PointF touchReleaseWarpCenter;
float touchWarpBaseRadius = 0;
int artworkWidth = 240, artworkHeight = 240;
std::vector<UINT> frameEnds;
std::vector<UINT> transitionSourceFrameEnds;
std::vector<UINT> transitionFrameEnds;
std::vector<UINT> smashFrameEnds;
std::vector<UINT> angryFrameEnds;
std::vector<UINT> angryAltSourceFrameEnds;
std::vector<UINT> angryAltFrameEnds;
std::vector<UINT> panicFrameEnds;
std::vector<UINT> blankFrameEnds;
std::vector<UINT> touchFrameEnds;
std::vector<UINT> touchEndFrameEnds;
std::vector<PointF> frameCenters;
std::vector<PointF> touchWarpCenters;
UINT activeFrame = 0;
ULONGLONG animationStart = 0;
ULONGLONG idlePhaseOffset = 0;
bool reactionActive = false;
bool showingAngry = false;
bool showingAngryAlt = false;
bool showingTransition = false;
bool handReturnActive = false;
bool cheekDragging = false;
bool touchReleaseActive = false;
bool alarmActive = false;
bool alarmPanicActive = false;
bool blankPlaybackActive = false;
ULONGLONG handReturnStart = 0;
float handReturnStartTurns[2] = { .5f, .5f };
float alarmHandStartTurns[2] = {};

bool ReadFrameEnds(Bitmap& image, std::vector<UINT>& ends) {
    const UINT count = image.GetFrameCount(&FrameDimensionTime);
    const UINT bytes = image.GetPropertyItemSize(PropertyTagFrameDelay);
    std::vector<BYTE> storage(bytes);
    auto delays = reinterpret_cast<PropertyItem*>(storage.data());
    if (!bytes || image.GetPropertyItem(PropertyTagFrameDelay, bytes, delays) != Ok ||
        delays->type != PropertyTagTypeLong || delays->length / sizeof(ULONG) < count) return false;
    ends.clear();
    UINT duration = 0;
    for (UINT i = 0; i < count; ++i) {
        const ULONG delay = static_cast<ULONG*>(delays->value)[i];
        if (delay > 60000 || duration > MAXUINT - std::max(1UL, delay) * 10) return false;
        duration += std::max(1UL, delay) * 10;
        ends.push_back(duration);
    }
    return !ends.empty();
}

void BuildTransitionFrameEnds() {
    transitionFrameEnds.clear();
    const UINT sourceStart = TransitionStartFrame ? transitionSourceFrameEnds[TransitionStartFrame - 1] : 0;
    for (UINT i = TransitionStartFrame; i <= TransitionEndFrame; ++i) {
        const UINT elapsed = transitionSourceFrameEnds[i] - sourceStart;
        transitionFrameEnds.push_back(static_cast<UINT>(std::lround(elapsed / .75f)));
    }
}

void BuildAngryAltFrameEnds() {
    angryAltFrameEnds.clear();
    UINT sourceStart = angryAltSourceFrameEnds[AngryAltStartFrame - 1], duration = 0;
    const UINT lastFrame = static_cast<UINT>(angryAltSourceFrameEnds.size() - 1);
    for (UINT i = AngryAltStartFrame; i <= lastFrame; ++i) {
        const UINT sourceDelay = angryAltSourceFrameEnds[i] - sourceStart;
        sourceStart = angryAltSourceFrameEnds[i];
        const float progress = (i - AngryAltStartFrame) / static_cast<float>(lastFrame - AngryAltStartFrame);
        const float speed = 1.0f - progress * .5f;
        duration += static_cast<UINT>(std::lround(sourceDelay / speed));
        angryAltFrameEnds.push_back(duration);
    }
}

void ReleaseArtwork() {
    cheekWarpBuffers.Release();
    clockDial.reset();
    for (auto& hand : clockHands) hand.image.reset();
    gifImage.reset();
    transitionImage.reset();
    smashImage.reset();
    angryImage.reset();
    angryAltImage.reset();
    panicImage.reset();
    blankImage.reset();
    touchBackImage.reset();
    touchHeadImage.reset();
    touchFrontImage.reset();
    touchEndBackImage.reset();
    touchEndHeadImage.reset();
    touchEndFrontImage.reset();
    touchEndImage.reset();
    frameEnds.clear();
    transitionSourceFrameEnds.clear();
    transitionFrameEnds.clear();
    smashFrameEnds.clear();
    angryFrameEnds.clear();
    angryAltSourceFrameEnds.clear();
    angryAltFrameEnds.clear();
    panicFrameEnds.clear();
    blankFrameEnds.clear();
    touchFrameEnds.clear();
    touchEndFrameEnds.clear();
    frameCenters.clear();
    touchWarpCenters.clear();
    idlePhaseOffset = 0;
    reactionActive = false;
    showingAngry = false;
    showingAngryAlt = false;
    showingTransition = false;
    handReturnActive = false;
    cheekDragging = false;
    touchReleaseActive = false;
    alarmActive = false;
    alarmPanicActive = false;
    blankPlaybackActive = false;
    smashOffset = PointF();
    angryOffset = PointF();
    angryAltOffset = PointF();
    panicOffset = PointF();
    blankOffset = PointF();
    touchHeadOffset = PointF();
    touchFrontOffset = PointF();
    touchEndHeadOffset = PointF();
    touchEndFrontOffset = PointF();
    cheekTouchSource = PointF();
    cheekPull = PointF();
    touchReleasePullSource = PointF();
    touchReleaseWarpCenter = PointF();
    touchWarpBaseRadius = 0;
}

bool LoadArtwork(const std::wstring& directory) {
    const std::wstring config = directory + L"locate data\\clock-layout.ini";
    auto read = [&](const wchar_t* section, const wchar_t* key) {
        return static_cast<int>(GetPrivateProfileIntW(section, key, -1, config.c_str()));
    };
    auto readString = [&](const wchar_t* section, const wchar_t* key) {
        wchar_t value[MAX_PATH] = {};
        GetPrivateProfileStringW(section, key, L"", value, MAX_PATH, config.c_str());
        return std::wstring(value);
    };
    auto point = [&](const wchar_t* section, const wchar_t* x, const wchar_t* y) {
        return PointF(read(section, x) + .5f, read(section, y) + .5f);
    };
    auto valid = [](PointF p, Bitmap& image) {
        return p.X >= .5f && p.Y >= .5f && p.X < image.GetWidth() && p.Y < image.GetHeight();
    };
    const std::wstring gifPath = readString(L"background", L"file");
    if (gifPath.empty()) return false;
    gifImage.reset(Bitmap::FromFile((directory + gifPath).c_str()));
    if (!gifImage || gifImage->GetLastStatus() != Ok) return false;
    const std::wstring touchFile = readString(L"touch", L"file");
    if (touchFile.empty()) return false;
    transitionImage.reset(Bitmap::FromFile((directory + touchFile).c_str()));
    if (!transitionImage || transitionImage->GetLastStatus() != Ok ||
        read(L"touch", L"width") != static_cast<int>(transitionImage->GetWidth()) ||
        read(L"touch", L"height") != static_cast<int>(transitionImage->GetHeight())) return false;
    cheekTouchSource = point(L"touch", L"x", L"y");
    if (!valid(cheekTouchSource, *transitionImage)) return false;
    auto loadAligned = [&](const wchar_t* section, std::unique_ptr<Bitmap>& image, PointF& offset) {
        const std::wstring file = readString(section, L"file");
        const int x = GetPrivateProfileIntW(section, L"offsetX", INT_MAX, config.c_str());
        const int y = GetPrivateProfileIntW(section, L"offsetY", INT_MAX, config.c_str());
        if (file.empty() || x == INT_MAX || y == INT_MAX) return false;
        image.reset(Bitmap::FromFile((directory + file).c_str()));
        if (!image || image->GetLastStatus() != Ok ||
            read(section, L"width") != static_cast<int>(image->GetWidth()) ||
            read(section, L"height") != static_cast<int>(image->GetHeight())) return false;
        offset = PointF(static_cast<float>(x), static_cast<float>(y));
        return true;
    };
    auto loadImage = [&](const wchar_t* section, std::unique_ptr<Bitmap>& image) {
        const std::wstring file = readString(section, L"file");
        if (file.empty()) return false;
        image.reset(Bitmap::FromFile((directory + file).c_str()));
        return image && image->GetLastStatus() == Ok &&
            read(section, L"width") == static_cast<int>(image->GetWidth()) &&
            read(section, L"height") == static_cast<int>(image->GetHeight());
    };
    if (!loadAligned(L"smash", smashImage, smashOffset) || !loadAligned(L"angry", angryImage, angryOffset) ||
        !loadAligned(L"angry_alt", angryAltImage, angryAltOffset) ||
        !loadAligned(L"panic", panicImage, panicOffset) ||
        !loadAligned(L"blank", blankImage, blankOffset) ||
        !loadImage(L"touch_back", touchBackImage) ||
        !loadAligned(L"touch_head", touchHeadImage, touchHeadOffset) ||
        !loadAligned(L"touch_front", touchFrontImage, touchFrontOffset) ||
        !loadImage(L"touch_end_back", touchEndBackImage) ||
        !loadImage(L"touch_end", touchEndImage) ||
        !loadAligned(L"touch_end_head", touchEndHeadImage, touchEndHeadOffset) ||
        !loadAligned(L"touch_end_front", touchEndFrontImage, touchEndFrontOffset)) return false;
    const int touchWarpFrameCount = read(L"touch_head", L"frameCount");
    touchWarpBaseRadius = static_cast<float>(read(L"touch_head", L"radius"));
    if (touchWarpFrameCount <= 0 || touchWarpBaseRadius <= 0 ||
        touchWarpFrameCount != static_cast<int>(touchHeadImage->GetFrameCount(&FrameDimensionTime))) return false;
    touchWarpCenters.clear();
    for (int i = 0; i < touchWarpFrameCount; ++i) {
        const std::wstring key = L"frame" + std::to_wstring(i);
        wchar_t value[64] = {};
        GetPrivateProfileStringW(L"touch_head", key.c_str(), L"", value, 64, config.c_str());
        int x = -1, y = -1;
        if (swscanf_s(value, L"%d,%d", &x, &y) != 2) return false;
        PointF center(x + .5f, y + .5f);
        if (!valid(center, *touchHeadImage)) return false;
        touchWarpCenters.push_back(center);
    }
    const int frameCount = read(L"background", L"frameCount");
    if (read(L"background", L"width") != static_cast<int>(gifImage->GetWidth()) ||
        read(L"background", L"height") != static_cast<int>(gifImage->GetHeight()) || frameCount <= 0 ||
        frameCount != static_cast<int>(gifImage->GetFrameCount(&FrameDimensionTime))) return false;

    frameCenters.clear();
    float left = -DialRadius, top = -DialRadius, right = DialRadius, bottom = DialRadius;
    for (int i = 0; i < frameCount; ++i) {
        const std::wstring key = L"frame" + std::to_wstring(i);
        wchar_t value[64] = {};
        GetPrivateProfileStringW(L"background", key.c_str(), L"", value, 64, config.c_str());
        float x = -1, y = -1;
        if (swscanf_s(value, L"%f,%f", &x, &y) != 2) return false;
        PointF center(x + .5f, y + .5f);
        if (!valid(center, *gifImage)) return false;
        frameCenters.push_back(center);
    }

    const wchar_t* roles[] = { L"hour", L"minute" };
    for (int i = 0; i < 2; ++i) {
        auto& hand = clockHands[i];
        const std::wstring file = readString(roles[i], L"file");
        if (file.empty()) return false;
        hand.image.reset(Bitmap::FromFile((directory + file).c_str()));
        if (!hand.image || hand.image->GetLastStatus() != Ok ||
            read(roles[i], L"width") != static_cast<int>(hand.image->GetWidth()) ||
            read(roles[i], L"height") != static_cast<int>(hand.image->GetHeight())) return false;
        hand.center = point(roles[i], L"centerX", L"centerY");
        hand.tip = point(roles[i], L"tipX", L"tipY");
        if (!valid(hand.center, *hand.image) || !valid(hand.tip, *hand.image)) return false;
        const float dx = hand.tip.X - hand.center.X, dy = hand.tip.Y - hand.center.Y;
        if (dx == 0 && dy == 0) return false;
        hand.nativeAngle = std::atan2(dx, -dy) * 180 / Pi;
        const float radius = std::hypot(std::max(hand.center.X, hand.image->GetWidth() - hand.center.X),
            std::max(hand.center.Y, hand.image->GetHeight() - hand.center.Y)) * HandScale;
        left = std::min(left, -radius);
        top = std::min(top, -radius);
        right = std::max(right, radius);
        bottom = std::max(bottom, radius);
    }
    constexpr float margin = 12;
    clockCenter = PointF(margin - left, margin - top);
    artworkWidth = static_cast<int>(std::ceil(right - left + margin * 2));
    artworkHeight = static_cast<int>(std::ceil(bottom - top + margin * 2));

    std::vector<UINT> touchHeadFrameEnds, touchFrontFrameEnds;
    std::vector<UINT> touchEndBackFrameEnds, touchEndHeadFrameEnds, touchEndFrontFrameEnds;
    if (!ReadFrameEnds(*gifImage, frameEnds) || !ReadFrameEnds(*transitionImage, transitionSourceFrameEnds) ||
        !ReadFrameEnds(*smashImage, smashFrameEnds) || !ReadFrameEnds(*angryImage, angryFrameEnds) ||
        !ReadFrameEnds(*angryAltImage, angryAltSourceFrameEnds) || !ReadFrameEnds(*panicImage, panicFrameEnds) ||
        !ReadFrameEnds(*blankImage, blankFrameEnds) ||
        !ReadFrameEnds(*touchBackImage, touchFrameEnds) ||
        !ReadFrameEnds(*touchHeadImage, touchHeadFrameEnds) || !ReadFrameEnds(*touchFrontImage, touchFrontFrameEnds) ||
        !ReadFrameEnds(*touchEndImage, touchEndFrameEnds) ||
        !ReadFrameEnds(*touchEndBackImage, touchEndBackFrameEnds) ||
        !ReadFrameEnds(*touchEndHeadImage, touchEndHeadFrameEnds) ||
        !ReadFrameEnds(*touchEndFrontImage, touchEndFrontFrameEnds) ||
        touchHeadFrameEnds != touchFrameEnds || touchFrontFrameEnds != touchFrameEnds ||
        touchEndHeadFrameEnds != touchEndFrameEnds || touchEndFrontFrameEnds != touchEndFrameEnds ||
        touchEndBackFrameEnds != touchEndFrameEnds ||
        touchEndImage->GetWidth() != touchEndBackImage->GetWidth() ||
        touchEndImage->GetHeight() != touchEndBackImage->GetHeight() ||
        touchEndFrameEnds.size() <= TouchBounceFrameCount ||
        touchWarpCenters.size() != touchFrameEnds.size() ||
        smashFrameEnds.size() <= SmashStartFrame || transitionSourceFrameEnds.size() <= TransitionEndFrame ||
        angryFrameEnds.size() <= AngryCutFrame || angryAltSourceFrameEnds.size() <= AngryAltStartFrame ||
        frameEnds.size() <= IdleReturnFrame) return false;
    BuildTransitionFrameEnds();
    BuildAngryAltFrameEnds();
    activeFrame = 0;
    animationStart = GetTickCount64();
    idlePhaseOffset = 0;
    reactionActive = false;
    showingAngry = false;
    showingAngryAlt = false;
    showingTransition = false;
    handReturnActive = false;
    cheekDragging = false;
    touchReleaseActive = false;
    alarmActive = false;
    alarmPanicActive = false;
    blankPlaybackActive = false;
    cheekPull = PointF();
    touchReleasePullSource = PointF();
    touchReleaseWarpCenter = PointF();
    clockDial = std::make_unique<ClockDial>();
    if (!clockDial->Initialize(clockCenter)) return false;
    return gifImage->SelectActiveFrame(&FrameDimensionTime, 0) == Ok &&
        transitionImage->SelectActiveFrame(&FrameDimensionTime, TransitionStartFrame) == Ok &&
        smashImage->SelectActiveFrame(&FrameDimensionTime, 0) == Ok &&
        angryImage->SelectActiveFrame(&FrameDimensionTime, 0) == Ok &&
        angryAltImage->SelectActiveFrame(&FrameDimensionTime, AngryAltStartFrame) == Ok &&
        panicImage->SelectActiveFrame(&FrameDimensionTime, 0) == Ok &&
        blankImage->SelectActiveFrame(&FrameDimensionTime, 0) == Ok &&
        touchBackImage->SelectActiveFrame(&FrameDimensionTime, 0) == Ok &&
        touchHeadImage->SelectActiveFrame(&FrameDimensionTime, 0) == Ok &&
        touchFrontImage->SelectActiveFrame(&FrameDimensionTime, 0) == Ok &&
        touchEndBackImage->SelectActiveFrame(&FrameDimensionTime, 0) == Ok &&
        touchEndHeadImage->SelectActiveFrame(&FrameDimensionTime, 0) == Ok &&
        touchEndImage->SelectActiveFrame(&FrameDimensionTime, TouchBounceFrameCount) == Ok &&
        touchEndFrontImage->SelectActiveFrame(&FrameDimensionTime, 0) == Ok;
}

UINT FrameAt(const std::vector<UINT>& ends, ULONGLONG elapsed) {
    const UINT phase = static_cast<UINT>(elapsed % ends.back());
    return static_cast<UINT>(std::upper_bound(ends.begin(), ends.end(), phase) - ends.begin());
}

enum class ReactionStage { Smash, Angry, AngryAlt, Transition, Finished };

UINT SmashStartOffset() {
    return SmashStartFrame ? smashFrameEnds[SmashStartFrame - 1] : 0;
}

ULONGLONG AngryStart() {
    return smashFrameEnds.back() - SmashStartOffset();
}

ULONGLONG AngryAltStart() {
    return AngryStart() + static_cast<ULONGLONG>(angryFrameEnds.back()) * 2 + angryFrameEnds[AngryCutFrame];
}

ULONGLONG TransitionStart() {
    return AngryAltStart() + angryAltFrameEnds.back();
}

ULONGLONG ReactionDuration() {
    return TransitionStart() + transitionFrameEnds.back();
}

ReactionStage ReactionStageAt(ULONGLONG elapsed) {
    if (elapsed < AngryStart()) return ReactionStage::Smash;
    if (elapsed < AngryAltStart()) return ReactionStage::Angry;
    if (elapsed < TransitionStart()) return ReactionStage::AngryAlt;
    if (elapsed < ReactionDuration()) return ReactionStage::Transition;
    return ReactionStage::Finished;
}

PointF GifPosition() {
    return PointF(clockCenter.X - frameCenters[0].X * CharacterScale,
        clockCenter.Y - frameCenters[0].Y * CharacterScale);
}

PointF CheekTouchCenter() {
    const PointF idle = GifPosition();
    return PointF(idle.X + cheekTouchSource.X * CharacterScale,
        idle.Y + cheekTouchSource.Y * CharacterScale);
}

UINT TouchWarpFrame() {
    return cheekDragging && activeFrame < touchWarpCenters.size() ? activeFrame : 0;
}

PointF TouchWarpCenterSource() {
    const PointF point = touchWarpCenters[TouchWarpFrame()];
    return PointF(touchHeadOffset.X + point.X, touchHeadOffset.Y + point.Y);
}

PointF ClampedCheekPull(PointF mouse) {
    const PointF center = CheekTouchCenter();
    float dx = mouse.X - center.X, dy = mouse.Y - center.Y;
    const float distance = std::hypot(dx, dy);
    const float limit = CheekTouchRadius * 1.5f;
    if (distance > limit) {
        dx *= limit / distance;
        dy *= limit / distance;
    }
    return PointF(dx, dy);
}

Bitmap* CurrentGif() {
    if (touchReleaseActive && activeFrame >= TouchBounceFrameCount) return touchEndImage.get();
    if (alarmActive && alarmPanicActive) return panicImage.get();
    if (blankPlaybackActive) return blankImage.get();
    if (!reactionActive) return gifImage.get();
    if (showingTransition) return transitionImage.get();
    if (showingAngryAlt) return angryAltImage.get();
    return showingAngry ? angryImage.get() : smashImage.get();
}

bool HandsVisible() {
    if (blankPlaybackActive) return false;
    if (alarmActive) return !alarmPanicActive;
    return !cheekDragging && !touchReleaseActive && (!reactionActive || showingTransition);
}

void BeginAlarm(const SYSTEMTIME& now) {
    cheekWarpBuffers.Release();
    blankPlaybackActive = false;
    reactionActive = false;
    showingAngry = false;
    showingAngryAlt = false;
    showingTransition = false;
    cheekDragging = false;
    touchReleaseActive = false;
    cheekPull = PointF();
    handReturnActive = false;
    alarmActive = true;
    alarmPanicActive = false;
    alarmHandStartTurns[0] = HourTurn(now.wHour, now.wMinute, now.wSecond);
    alarmHandStartTurns[1] = MinuteTurn(now.wMinute, now.wSecond);
    activeFrame = 0;
    animationStart = GetTickCount64();
    idlePhaseOffset = 0;
    gifImage->SelectActiveFrame(&FrameDimensionTime, 0);
}

void BeginBlankPlayback() {
    cheekWarpBuffers.Release();
    alarmActive = false;
    alarmPanicActive = false;
    reactionActive = false;
    showingAngry = false;
    showingAngryAlt = false;
    showingTransition = false;
    cheekDragging = false;
    touchReleaseActive = false;
    cheekPull = PointF();
    handReturnActive = false;
    blankPlaybackActive = true;
    activeFrame = 0;
    animationStart = GetTickCount64();
    blankImage->SelectActiveFrame(&FrameDimensionTime, 0);
}

void EndBlankPlayback() {
    if (!blankPlaybackActive) return;
    blankPlaybackActive = false;
    activeFrame = 0;
    animationStart = GetTickCount64();
    idlePhaseOffset = 0;
    gifImage->SelectActiveFrame(&FrameDimensionTime, 0);
}

void DismissAlarmToIdle() {
    if (!alarmActive) return;
    alarmActive = false;
    alarmPanicActive = false;
    animationStart = GetTickCount64();
    idlePhaseOffset = 0;
    activeFrame = 0;
    gifImage->SelectActiveFrame(&FrameDimensionTime, 0);
    handReturnActive = true;
    handReturnStartTurns[0] = handReturnStartTurns[1] = AlarmHandTurn;
    handReturnStart = animationStart;
}

void BeginReaction() {
    cheekWarpBuffers.Release();
    blankPlaybackActive = false;
    alarmActive = false;
    alarmPanicActive = false;
    cheekDragging = false;
    touchReleaseActive = false;
    cheekPull = PointF();
    reactionActive = true;
    showingAngry = false;
    showingAngryAlt = false;
    showingTransition = false;
    handReturnActive = false;
    activeFrame = SmashStartFrame;
    animationStart = GetTickCount64();
    smashImage->SelectActiveFrame(&FrameDimensionTime, SmashStartFrame);
}

void BeginCheekPull() {
    cheekWarpBuffers.Release();
    blankPlaybackActive = false;
    alarmActive = false;
    alarmPanicActive = false;
    reactionActive = false;
    showingAngry = false;
    showingAngryAlt = false;
    showingTransition = false;
    handReturnActive = false;
    cheekDragging = true;
    touchReleaseActive = false;
    cheekPull = PointF();
    activeFrame = 0;
    animationStart = GetTickCount64();
    touchBackImage->SelectActiveFrame(&FrameDimensionTime, 0);
    touchHeadImage->SelectActiveFrame(&FrameDimensionTime, 0);
    touchFrontImage->SelectActiveFrame(&FrameDimensionTime, 0);
}

PointF EffectiveCheekWarpPull();

bool SelectTouchReleaseFrame(UINT frame) {
    if (frame >= TouchBounceFrameCount) {
        if (touchEndImage->SelectActiveFrame(&FrameDimensionTime, frame) != Ok) return false;
        cheekWarpBuffers.Release();
        return true;
    }
    return touchEndBackImage->SelectActiveFrame(&FrameDimensionTime, frame) == Ok &&
        touchEndHeadImage->SelectActiveFrame(&FrameDimensionTime, frame) == Ok &&
        touchEndFrontImage->SelectActiveFrame(&FrameDimensionTime, frame) == Ok;
}

void EndCheekPull() {
    if (!cheekDragging) return;
    const PointF headPoint = touchWarpCenters[TouchWarpFrame()];
    touchReleaseWarpCenter = PointF(touchEndHeadOffset.X + headPoint.X,
        touchEndHeadOffset.Y + headPoint.Y);
    touchReleasePullSource = EffectiveCheekWarpPull();
    cheekPull = PointF();
    cheekDragging = false;
    touchReleaseActive = true;
    activeFrame = 0;
    animationStart = GetTickCount64();
    handReturnActive = false;
    touchEndBackImage->SelectActiveFrame(&FrameDimensionTime, 0);
    touchEndHeadImage->SelectActiveFrame(&FrameDimensionTime, 0);
    touchEndFrontImage->SelectActiveFrame(&FrameDimensionTime, 0);
}

float TouchReleasePullScale(UINT frame) {
    static constexpr float scales[TouchBounceFrameCount] = {
        1.0f, .55f, .08f, -.14f, .07f, 0.0f
    };
    return frame < TouchBounceFrameCount ? scales[frame] : 0.0f;
}

void CompleteTouchRelease(ULONGLONG now) {
    cheekWarpBuffers.Release();
    touchReleaseActive = false;
    cheekPull = PointF();
    touchReleasePullSource = PointF();
    animationStart = now;
    idlePhaseOffset = 0;
    activeFrame = 0;
    handReturnActive = true;
    handReturnStartTurns[0] = handReturnStartTurns[1] = .5f;
    handReturnStart = now;
    gifImage->SelectActiveFrame(&FrameDimensionTime, 0);
}

PointF AnimationPosition(Bitmap& image) {
    const PointF idle = GifPosition();
    PointF offset;
    if (&image == smashImage.get()) offset = smashOffset;
    else if (&image == angryImage.get()) offset = angryOffset;
    else if (&image == angryAltImage.get()) offset = angryAltOffset;
    else if (&image == panicImage.get()) offset = panicOffset;
    else if (&image == blankImage.get()) offset = blankOffset;
    return PointF(idle.X + offset.X * CharacterScale, idle.Y + offset.Y * CharacterScale);
}

void HandTransform(Matrix& transform, const ClockHand& hand, float turns, PointF center) {
    transform.Translate(center.X, center.Y);
    transform.Rotate(turns * 360 - hand.nativeAngle);
    transform.Scale(HandScale, HandScale);
    transform.Translate(-hand.center.X, -hand.center.Y);
}

PointF IdleHandCenterAt(float frame) {
    if (frameCenters.empty()) return clockCenter;
    frame = std::clamp(frame, 0.0f, static_cast<float>(frameCenters.size() - 1));
    const size_t first = static_cast<size_t>(std::floor(frame));
    const size_t second = std::min(first + 1, frameCenters.size() - 1);
    const float blend = frame - first;
    const PointF center(
        frameCenters[first].X + (frameCenters[second].X - frameCenters[first].X) * blend,
        frameCenters[first].Y + (frameCenters[second].Y - frameCenters[first].Y) * blend);
    const PointF position = GifPosition();
    return PointF(position.X + center.X * CharacterScale,
        position.Y + center.Y * CharacterScale);
}

PointF IdleHandCenter(UINT frame) {
    return IdleHandCenterAt(static_cast<float>(frame));
}

float TransitionIdleFrame(UINT frame) {
    if (transitionSourceFrameEnds.size() < 2 || frameCenters.size() < 2) return 0;
    return frame * static_cast<float>(frameCenters.size() - 1) /
        static_cast<float>(transitionSourceFrameEnds.size() - 1);
}

PointF TransitionHandCenter(UINT frame) {
    return IdleHandCenterAt(TransitionIdleFrame(frame));
}

void StretchedHandTransform(Matrix& transform, const ClockHand& hand,
    PointF movingCenter, PointF fixedTip) {
    const float vx = hand.tip.X - hand.center.X;
    const float vy = hand.tip.Y - hand.center.Y;
    const float sourceLength = std::hypot(vx, vy);
    const float dx = fixedTip.X - movingCenter.X;
    const float dy = fixedTip.Y - movingCenter.Y;
    const float targetLength = std::hypot(dx, dy);
    if (sourceLength <= .001f || targetLength <= .001f) {
        transform.SetElements(HandScale, 0, 0, HandScale,
            movingCenter.X - hand.center.X * HandScale,
            movingCenter.Y - hand.center.Y * HandScale);
        return;
    }

    const float ux = vx / sourceLength;
    const float uy = vy / sourceLength;
    const float ax = dx / sourceLength;
    const float ay = dy / sourceLength;
    const float bx = -dy / targetLength * HandScale;
    const float by = dx / targetLength * HandScale;
    const float m11 = ax * ux - bx * uy;
    const float m12 = ay * ux - by * uy;
    const float m21 = ax * uy + bx * ux;
    const float m22 = ay * uy + by * ux;
    transform.SetElements(m11, m12, m21, m22,
        movingCenter.X - m11 * hand.center.X - m21 * hand.center.Y,
        movingCenter.Y - m12 * hand.center.X - m22 * hand.center.Y);
}

float AnimatedHandTurn(float start, float target, float progress) {
    progress = std::clamp(progress, 0.0f, 1.0f);
    const float eased = progress * progress * (3 - 2 * progress);
    const float shortest = std::fmod(target - start + 1.5f, 1.0f) - 0.5f;
    return start + shortest * eased;
}

bool ShouldDrawTick(int index, bool drawTicks, bool drawNumbers) {
    return drawTicks && !(drawNumbers && index % 5 == 0);
}

PointF EffectiveCheekWarpPull() {
    PointF pull(cheekPull.X / CharacterScale, cheekPull.Y / CharacterScale);
    const float inwardLimit = touchWarpBaseRadius * MaxInwardCheekPull;
    if (pull.X > inwardLimit) {
        const float scale = inwardLimit / pull.X;
        pull.X *= scale;
        pull.Y *= scale;
    }
    return pull;
}

Bitmap* WarpedHead(Bitmap& back, Bitmap& head, PointF headOffset,
    PointF warpCenter, PointF effectivePull) {
    const UINT width = back.GetWidth(), height = back.GetHeight();
    if (!cheekWarpBuffers.Prepare(back, head, headOffset, activeFrame)) return nullptr;
    Bitmap* source = cheekWarpBuffers.source.get();
    Bitmap* result = cheekWarpBuffers.result.get();
    float pullX = effectivePull.X, pullY = effectivePull.Y;
    if (std::hypot(pullX, pullY) < .01f) return source;
    const float warpCenterX = warpCenter.X - .5f, warpCenterY = warpCenter.Y - .5f;
    const float warpRadius = touchWarpBaseRadius;
    {
        Graphics graphics(result);
        graphics.Clear(Color(0, 0, 0, 0));
        graphics.SetCompositingMode(CompositingModeSourceCopy);
        graphics.DrawImage(source, 0, 0, width, height);
    }
    Rect bounds(0, 0, width, height);
    BitmapData sourceData = {}, resultData = {};
    if (source->LockBits(&bounds, ImageLockModeRead, PixelFormat32bppPARGB, &sourceData) != Ok ||
        result->LockBits(&bounds, ImageLockModeWrite, PixelFormat32bppPARGB, &resultData) != Ok) {
        if (sourceData.Scan0) source->UnlockBits(&sourceData);
        return nullptr;
    }
    auto pixel = [&](int x, int y, int channel) -> float {
        if (x < 0 || y < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(height)) return 0;
        const BYTE* row = static_cast<const BYTE*>(sourceData.Scan0) + y * sourceData.Stride;
        return row[x * 4 + channel];
    };
    if (pullX <= 0 || std::abs(pullY) > .01f) {
        for (int y = std::max(0, static_cast<int>(std::floor(warpCenterY - warpRadius)));
            y <= std::min(static_cast<int>(height) - 1, static_cast<int>(std::ceil(warpCenterY + warpRadius))); ++y) {
            const float vertical = (y - warpCenterY) / warpRadius;
            if (std::abs(vertical) >= 1) continue;
            const float chord = std::sqrt(1 - vertical * vertical);
            const int first = std::max(0, static_cast<int>(std::floor(warpCenterX - warpRadius * chord)));
            const int last = std::min(static_cast<int>(width) - 1,
                static_cast<int>(std::floor(warpCenterX + warpRadius * chord - CheekSeamOverlap)));
            BYTE* output = static_cast<BYTE*>(resultData.Scan0) + y * resultData.Stride;
            for (int x = first; x <= last; ++x)
                output[x * 4] = output[x * 4 + 1] = output[x * 4 + 2] = output[x * 4 + 3] = 0;
        }
    }
    struct WarpVertex { float x, y, sourceX, sourceY; };
    constexpr int Rows = CheekRows, Columns = CheekColumns;
    WarpVertex mesh[Rows + 1][Columns + 1] = {};
    cheekWarpBuffers.PrepareGeometry(warpCenter, warpRadius);
    for (int row = 0; row <= Rows; ++row) {
        for (int column = 0; column <= Columns; ++column) {
            const PointF point = cheekWarpBuffers.original[row][column];
            const float weight = cheekWarpGrid.weight[row][column];
            mesh[row][column] = { point.X + pullX * weight, point.Y + pullY * weight, point.X, point.Y };
        }
    }
    auto edge = [](const WarpVertex& a, const WarpVertex& b, float x, float y) {
        return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
    };
    auto drawTriangle = [&](const WarpVertex& a, const WarpVertex& b, const WarpVertex& c) {
        const float area = edge(a, b, c.x, c.y);
        if (std::abs(area) < .001f) return;
        const int firstX = std::max(0, static_cast<int>(std::floor(std::min({ a.x, b.x, c.x }))));
        const int firstY = std::max(0, static_cast<int>(std::floor(std::min({ a.y, b.y, c.y }))));
        const int lastX = std::min(static_cast<int>(width) - 1,
            static_cast<int>(std::ceil(std::max({ a.x, b.x, c.x }))));
        const int lastY = std::min(static_cast<int>(height) - 1,
            static_cast<int>(std::ceil(std::max({ a.y, b.y, c.y }))));
        for (int y = firstY; y <= lastY; ++y) {
            BYTE* output = static_cast<BYTE*>(resultData.Scan0) + y * resultData.Stride;
            for (int x = firstX; x <= lastX; ++x) {
                const float px = static_cast<float>(x), py = static_cast<float>(y);
                const float wa = edge(b, c, px, py) / area;
                const float wb = edge(c, a, px, py) / area;
                const float wc = 1 - wa - wb;
                if (wa < -.001f || wb < -.001f || wc < -.001f) continue;
                const float sourceX = a.sourceX * wa + b.sourceX * wb + c.sourceX * wc;
                const float sourceY = a.sourceY * wa + b.sourceY * wb + c.sourceY * wc;
                const int left = static_cast<int>(std::floor(sourceX));
                const int top = static_cast<int>(std::floor(sourceY));
                const float fx = sourceX - left, fy = sourceY - top;
                float sample[4];
                for (int channel = 0; channel < 4; ++channel) {
                    const float upper = pixel(left, top, channel) * (1 - fx) + pixel(left + 1, top, channel) * fx;
                    const float lower = pixel(left, top + 1, channel) * (1 - fx) + pixel(left + 1, top + 1, channel) * fx;
                    sample[channel] = std::clamp(upper * (1 - fy) + lower * fy, 0.0f, 255.0f);
                }
                const float remaining = 1 - sample[3] / 255.0f;
                for (int channel = 0; channel < 4; ++channel)
                    output[x * 4 + channel] = static_cast<BYTE>(std::clamp(
                        sample[channel] + output[x * 4 + channel] * remaining, 0.0f, 255.0f));
            }
        }
    };
    for (int row = 0; row < Rows; ++row) for (int column = 0; column < Columns; ++column) {
        drawTriangle(mesh[row][column], mesh[row + 1][column], mesh[row + 1][column + 1]);
        drawTriangle(mesh[row][column], mesh[row + 1][column + 1], mesh[row][column + 1]);
    }
    source->UnlockBits(&sourceData);
    result->UnlockBits(&resultData);
    return result;
}

Bitmap* WarpedTouchHead() {
    return WarpedHead(*touchBackImage, *touchHeadImage, touchHeadOffset,
        TouchWarpCenterSource(), EffectiveCheekWarpPull());
}

Bitmap* WarpedTouchEndHead() {
    const float scale = TouchReleasePullScale(activeFrame);
    return WarpedHead(*touchEndBackImage, *touchEndHeadImage, touchEndHeadOffset,
        touchReleaseWarpCenter, PointF(touchReleasePullSource.X * scale,
            touchReleasePullSource.Y * scale));
}

void DrawArtwork(Graphics& g, const SYSTEMTIME& now, bool drawDial, bool drawTicks, bool drawNumbers) {
    auto& dial = *clockDial;
    if (drawDial) g.FillEllipse(&dial.background, clockCenter.X - DialRadius, clockCenter.Y - DialRadius, DialRadius * 2, DialRadius * 2);
    g.SetInterpolationMode(InterpolationModeBilinear);
    GraphicsState clipState = g.Save();
    g.SetClip(&dial.clip, CombineModeReplace);
    if (cheekDragging || (touchReleaseActive && activeFrame < TouchBounceFrameCount)) {
        const PointF position = GifPosition();
        auto head = cheekDragging ? WarpedTouchHead() : WarpedTouchEndHead();
        auto drawLayer = [&](Bitmap& image, PointF offset) {
            g.DrawImage(&image, RectF(position.X + offset.X * CharacterScale, position.Y + offset.Y * CharacterScale,
                image.GetWidth() * CharacterScale, image.GetHeight() * CharacterScale),
                0, 0, static_cast<float>(image.GetWidth()), static_cast<float>(image.GetHeight()), UnitPixel);
        };
        drawLayer(cheekDragging ? *touchBackImage : *touchEndBackImage, PointF());
        if (head) drawLayer(*head, PointF());
        drawLayer(cheekDragging ? *touchFrontImage : *touchEndFrontImage,
            cheekDragging ? touchFrontOffset : touchEndFrontOffset);
    } else {
        Bitmap* animation = CurrentGif();
        const PointF position = AnimationPosition(*animation);
        g.DrawImage(animation, RectF(position.X, position.Y, animation->GetWidth() * CharacterScale,
            animation->GetHeight() * CharacterScale), 0, 0, static_cast<float>(animation->GetWidth()),
            static_cast<float>(animation->GetHeight()), UnitPixel);
    }
    g.Restore(clipState);
    for (int i = 0; i < 60; ++i) {
        if (ShouldDrawTick(i, drawTicks, drawNumbers)) {
            const bool major = i % 5 == 0;
            g.DrawLine(major ? &dial.majorOutline : &dial.minorOutline, dial.tickStart[i], dial.tickEnd[i]);
            g.DrawLine(major ? &dial.majorTick : &dial.minorTick, dial.tickStart[i], dial.tickEnd[i]);
        }
    }
    if (drawNumbers) {
        for (auto& number : dial.numbers) {
            g.DrawPath(&dial.numberOutline, &number);
            g.FillPath(&dial.ink, &number);
        }
    }
    if (HandsVisible()) {
        float turns[] = { HourTurn(now.wHour, now.wMinute, now.wSecond), MinuteTurn(now.wMinute, now.wSecond) };
        if (alarmActive && !alarmPanicActive) {
            const float progress = (GetTickCount64() - animationStart) / static_cast<float>(AlarmHandGatherMs);
            turns[0] = AnimatedHandTurn(alarmHandStartTurns[0], AlarmHandTurn, progress);
            turns[1] = AnimatedHandTurn(alarmHandStartTurns[1], AlarmHandTurn, progress);
        } else if (handReturnActive) {
            const ULONGLONG elapsed = GetTickCount64() - handReturnStart;
            if (elapsed >= HandReturnMs) {
                handReturnActive = false;
            } else {
                const float progress = elapsed / static_cast<float>(HandReturnMs);
                turns[0] = AnimatedHandTurn(handReturnStartTurns[0], turns[0], progress);
                turns[1] = AnimatedHandTurn(handReturnStartTurns[1], turns[1], progress);
            }
        }
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        for (int i = 0; i < 2; ++i) {
            auto& hand = clockHands[i];
            GraphicsState state = g.Save();
            Matrix transform;
            const bool idleHandCenter = CurrentGif() == gifImage.get() && activeFrame < frameCenters.size();
            const bool transitionHandCenter = CurrentGif() == transitionImage.get() &&
                activeFrame < transitionSourceFrameEnds.size();
            if (idleHandCenter || transitionHandCenter) {
                Matrix fixedTipTransform;
                HandTransform(fixedTipTransform, hand, turns[i], clockCenter);
                PointF fixedTip = hand.tip;
                fixedTipTransform.TransformPoints(&fixedTip, 1);
                const PointF movingCenter = idleHandCenter ? IdleHandCenter(activeFrame) :
                    TransitionHandCenter(activeFrame);
                StretchedHandTransform(transform, hand, movingCenter, fixedTip);
            } else {
                HandTransform(transform, hand, turns[i], clockCenter);
            }
            g.MultiplyTransform(&transform);
            g.DrawImage(hand.image.get(), RectF(0, 0, static_cast<float>(hand.image->GetWidth()),
                static_cast<float>(hand.image->GetHeight())), 0, 0, static_cast<float>(hand.image->GetWidth()),
                static_cast<float>(hand.image->GetHeight()), UnitPixel);
            g.Restore(state);
        }
    }
}
