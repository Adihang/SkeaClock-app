#pragma once

PointF ClampedPatPull(PointF mouse) {
    const float dx = mouse.X - clockCenter.X, dy = mouse.Y - clockCenter.Y;
    const float distance = std::hypot(dx, dy);
    const float scale = distance > PatWarpRadius ?
        PatFollowFraction * PatWarpRadius / distance : PatFollowFraction;
    return PointF(dx * scale, dy * scale);
}

Bitmap* WarpedPatImage() {
    const PointF position = GifPosition();
    const PointF center((clockCenter.X - position.X) / CharacterScale,
        (clockCenter.Y - position.Y) / CharacterScale);
    Bitmap* frame = patIdleImage->Frame();
    if (!frame) return nullptr;
    return WarpedHead(patIdleImage->GetWidth(), patIdleImage->GetHeight(),
        *frame, patIdleImage->FrameOffset(), center,
        PointF(patPull.X / CharacterScale, patPull.Y / CharacterScale),
        PatWarpRadius / CharacterScale, true);
}
