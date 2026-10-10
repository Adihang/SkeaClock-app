#pragma once

// Geometry stays in artwork coordinates; the renderer scales it for each window size.
struct ClockDial {
    GraphicsPath clip;
    GraphicsPath numbers[12];
    PointF tickStart[60], tickEnd[60];
    SolidBrush background{ Color(255, 0xDF, 0xF7, 0xF5) };
    SolidBrush ink{ Color(255, 50, 63, 72) };
    Pen majorOutline{ Color(255, 255, 255, 255), 5.0f };
    Pen minorOutline{ Color(255, 255, 255, 255), 3.5f };
    Pen majorTick{ Color(255, 67, 79, 88), 3.0f };
    Pen minorTick{ Color(220, 84, 99, 109), 1.5f };
    Pen numberOutline{ Color(255, 255, 255, 255), 2.5f };

    bool Initialize(PointF center) {
        if (clip.AddEllipse(center.X - DialRadius, center.Y - DialRadius,
            DialRadius * 2, DialRadius * 2) != Ok) return false;
        for (int i = 0; i < 60; ++i) {
            const float angle = i * Pi / 30;
            auto at = [&](float radius) {
                return PointF(center.X + std::sin(angle) * radius, center.Y - std::cos(angle) * radius);
            };
            tickStart[i] = at(DialRadius - (i % 5 == 0 ? 14 : 7));
            tickEnd[i] = at(DialRadius);
        }
        FontFamily family(L"Segoe UI");
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        numberOutline.SetLineJoin(LineJoinRound);
        for (int i = 1; i <= 12; ++i) {
            const std::wstring number = std::to_wstring(i);
            const PointF p(center.X + std::sin(i * Pi / 6) * (DialRadius - 14),
                center.Y - std::cos(i * Pi / 6) * (DialRadius - 14));
            if (numbers[i - 1].AddString(number.c_str(), -1, &family, FontStyleRegular, 16, p, &format) != Ok)
                return false;
        }
        return true;
    }
};

std::unique_ptr<ClockDial> clockDial;
