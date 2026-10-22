#pragma once

const UINT PatWarpPadding = static_cast<UINT>(std::ceil(PatWarpRadius * PatFollowFraction / CharacterScale)) + 2;

PointF ClampedPatPull(PointF mouse) {
    const float dx = mouse.X - clockCenter.X, dy = mouse.Y - clockCenter.Y;
    const float distance = std::hypot(dx, dy);
    const float scale = distance > PatWarpRadius ?
        PatFollowFraction * PatWarpRadius / distance : PatFollowFraction;
    return PointF(dx * scale, dy * scale);
}

Bitmap* WarpedPatImage() {
    const PointF position = GifPosition();
    const float padding = static_cast<float>(PatWarpPadding);
    const PointF center((clockCenter.X - position.X) / CharacterScale + padding,
        (clockCenter.Y - position.Y) / CharacterScale + padding);
    Bitmap* frame = patIdleImage->Frame();
    if (!frame) return nullptr;
    const PointF crop = patIdleImage->FrameOffset();
    return WarpedHead(patIdleImage->GetWidth() + PatWarpPadding * 2,
        patIdleImage->GetHeight() + PatWarpPadding * 2,
        *frame, PointF(crop.X + padding, crop.Y + padding), center,
        PointF(patPull.X / CharacterScale, patPull.Y / CharacterScale),
        PatWarpRadius / CharacterScale, true);
}
