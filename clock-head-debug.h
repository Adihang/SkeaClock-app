#pragma once

constexpr int TestPanelWidth = 96;
const RectF TestAlarmButtonBounds(8, 8, TestPanelWidth - 16.0f, 36);

void DrawTestAlarmButton(Graphics& graphics) {
    SolidBrush background(Color(235, 45, 75, 100));
    SolidBrush text(Color(255, 255, 255, 255));
    Pen border(Color(255, 150, 190, 220), 1.0f);
    Font font(L"Segoe UI", 12, FontStyleRegular, UnitPixel);
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);
    graphics.FillRectangle(&background, TestAlarmButtonBounds);
    graphics.DrawRectangle(&border, TestAlarmButtonBounds);
    graphics.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    graphics.DrawString(L"알람 테스트", -1, &font, TestAlarmButtonBounds, &format, &text);
}
