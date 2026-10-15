"""Generate the clock-face layers and looping idle character used by widgets.

Requires Pillow. Source frames and their timing/centers are the checked-in
skea_idle.gif and skea_animation.json resources.
"""
from __future__ import annotations

import json
import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
RES = ROOT / "android/app/src/main/res"
ASSETS = ROOT / "android/app/src/main/assets"
SIZE = 480
SCALE = 1.6
HIRES = 2


def make_face_background(flags: int) -> Image.Image:
    image = Image.new("RGBA", (SIZE, SIZE))
    if flags & 1:
        draw = ImageDraw.Draw(image)
        draw.ellipse((12, 12, 468, 468), fill="#DFF7F5")

    circle = Image.new("L", (SIZE, SIZE))
    ImageDraw.Draw(circle).ellipse((12, 12, 468, 468), fill=255)
    image.putalpha(ImageChops.multiply(image.getchannel("A"), circle))
    return image


def make_face_markers(flags: int, font: ImageFont.FreeTypeFont) -> Image.Image:
    image = Image.new("RGBA", (SIZE, SIZE))
    draw = ImageDraw.Draw(image)

    # Match extract_assets.py's generated face geometry and paint order.
    if flags & 2:
        for index in range(60):
            angle = index * math.pi / 30
            inner = 215 if index % 5 else 207
            draw.line(
                (
                    240 + math.sin(angle) * inner,
                    240 - math.cos(angle) * inner,
                    240 + math.sin(angle) * 225,
                    240 - math.cos(angle) * 225,
                ),
                fill="#54636D",
                width=3 if index % 5 else 4,
            )

    if flags & 4:
        for index in range(1, 13):
            angle = index * math.pi / 6
            x = 240 + math.sin(angle) * 193
            y = 240 - math.cos(angle) * 193
            label = str(index)
            draw.text(
                (x, y), label, font=font, anchor="mm", fill="#323F48",
                stroke_width=2, stroke_fill="white"
            )

    circle = Image.new("L", (SIZE, SIZE))
    ImageDraw.Draw(circle).ellipse((12, 12, 468, 468), fill=255)
    image.putalpha(ImageChops.multiply(image.getchannel("A"), circle))
    return image


def main() -> None:
    font_path = next(
        (
            path
            for path in (
                Path("/System/Library/Fonts/Helvetica.ttc"),
                Path("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
            )
            if path.exists()
        ),
        None,
    )
    font = ImageFont.truetype(str(font_path), 30) if font_path else ImageFont.load_default(size=30)

    nodpi = RES / "drawable-nodpi"
    nodpi.mkdir(parents=True, exist_ok=True)
    for flags in range(8):
        make_face_background(flags).save(
            nodpi / f"widget_background_{flags}.png", optimize=True
        )
        make_face_markers(flags, font).save(
            nodpi / f"widget_markers_{flags}.png", optimize=True
        )

    metadata = json.loads((ASSETS / "skea_animation.json").read_text())
    idle = metadata["animations"]["skea_idle"]
    centers = idle["centers"]
    durations = idle["durations"]
    source = Image.open(RES / "raw/skea_idle.gif")
    if source.n_frames != len(centers) or source.n_frames != len(durations):
        raise ValueError("Idle GIF frames do not match animation metadata")

    circle = Image.new("L", (SIZE * HIRES, SIZE * HIRES))
    ImageDraw.Draw(circle).ellipse(
        (12 * HIRES, 12 * HIRES, 468 * HIRES, 468 * HIRES), fill=255
    )
    frames: list[Image.Image] = []
    for index, (center_x, center_y) in enumerate(centers):
        source.seek(index)
        sprite = source.convert("RGBA").resize(
            (round(idle["width"] * SCALE * HIRES), round(idle["height"] * SCALE * HIRES)),
            Image.Resampling.LANCZOS,
        )
        frame = Image.new("RGBA", (SIZE * HIRES, SIZE * HIRES))
        left = round((240 - center_x * SCALE) * HIRES)
        top = round((240 - center_y * SCALE) * HIRES)
        frame.alpha_composite(sprite, (left, top))
        frame.putalpha(ImageChops.multiply(frame.getchannel("A"), circle))
        frames.append(frame.resize((SIZE, SIZE), Image.Resampling.LANCZOS))

    fallback = RES / "drawable-nodpi-v26"
    fallback.mkdir(parents=True, exist_ok=True)
    frames[0].save(
        fallback / "widget_idle.webp",
        "WEBP",
        lossless=True,
        method=6,
    )
    frames[0].save(
        nodpi / "widget_idle_frame.webp",
        "WEBP",
        lossless=True,
        method=6,
    )
    frames[0].save(
        nodpi / "widget_idle_loop.webp",
        "WEBP",
        save_all=True,
        append_images=frames[1:],
        duration=durations,
        loop=0,
        lossless=True,
        method=6,
    )
    print(
        f"Generated {len(frames)} idle frames; "
        f"loop is {(nodpi / 'widget_idle_loop.webp').stat().st_size / 1024:.0f} KiB"
    )


if __name__ == "__main__":
    main()
