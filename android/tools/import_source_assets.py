"""Import the latest source artwork and sounds into Android resources.

Requires Pillow. The Windows project stores animations as PNG atlases plus
frame rectangles and timings in ``sources/locate data/clock-layout.ini``.
"""
from __future__ import annotations

import configparser
import json
import math
import shutil
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "sources"
RES = ROOT / "android/app/src/main/res"
ANIMATIONS = ROOT / "android/app/src/main/assets/skea_animation.json"


def read_layout() -> configparser.ConfigParser:
    layout = configparser.ConfigParser(interpolation=None)
    layout.optionxform = str
    with (SOURCE / "locate data/clock-layout.ini").open(encoding="utf-16") as stream:
        layout.read_file(stream)
    return layout


def animation_frames(layout: configparser.ConfigParser, section: str):
    entry = layout[section]
    width, height = int(entry["width"]), int(entry["height"])
    first = int(entry["firstFrame"])
    count = int(entry["sheetFrames"])
    chunk = int(entry["chunkSize"])
    paths = [entry["file" if index == 0 else f"file{index}"] for index in range(int(entry["chunks"]))]
    sheets = [Image.open(SOURCE / path.replace("\\", "/")).convert("RGBA") for path in paths]
    try:
        frames = []
        for frame_index in range(first, first + count):
            x, y, w, h, ox, oy = map(int, entry[f"rect{frame_index}"].split(","))
            sheet = sheets[(frame_index - first) // chunk]
            frame = Image.new("RGBA", (width, height))
            frame.alpha_composite(sheet.crop((x, y, x + w, y + h)), (ox, oy))
            frames.append(frame)
        return frames
    finally:
        for sheet in sheets:
            sheet.close()


def gif_frames(frames: list[Image.Image]) -> list[Image.Image]:
    # GIF has binary transparency. The source artwork uses transparent pixels,
    # so threshold alpha and reserve palette entry 255 for the transparent key.
    columns = 12
    sample_w, sample_h = max(1, frames[0].width // 8), max(1, frames[0].height // 8)
    rows = math.ceil(len(frames) / columns)
    contact = Image.new("RGB", (columns * sample_w, rows * sample_h), "black")
    for index, frame in enumerate(frames):
        sample = frame.convert("RGB").resize((sample_w, sample_h), Image.Resampling.BILINEAR)
        contact.paste(sample, ((index % columns) * sample_w, (index // columns) * sample_h))
    palette_image = contact.quantize(colors=255, method=Image.Quantize.MEDIANCUT)
    palette = (palette_image.getpalette() or [])[:765]
    palette += [0] * (768 - len(palette))

    palette_canvas = Image.new("P", (1, 1))
    palette_canvas.putpalette(palette)
    output = []
    for frame in frames:
        indexed = frame.convert("RGB").quantize(palette=palette_canvas, dither=Image.Dither.NONE)
        alpha = frame.getchannel("A").point(lambda value: 255 if value >= 128 else 0)
        indexed.paste(255, mask=ImageOps.invert(alpha))
        indexed.putpalette(palette)
        output.append(indexed)
    contact.close()
    palette_image.close()
    palette_canvas.close()
    return output


def save_animation(
    layout: configparser.ConfigParser,
    section: str,
    resource: str,
    metadata: dict,
) -> None:
    entry = layout[section]
    frames = animation_frames(layout, section)
    indexed = gif_frames(frames)
    durations = [int(entry[f"delay{index}"]) for index in range(len(frames))]
    output = RES / "raw" / f"{resource}.gif"
    output.parent.mkdir(parents=True, exist_ok=True)
    indexed[0].save(
        output,
        save_all=True,
        append_images=indexed[1:],
        duration=durations,
        loop=0,
        transparency=255,
        background=255,
        disposal=2,
        optimize=False,
    )
    item = {
        "resource": resource,
        "width": int(entry["width"]),
        "height": int(entry["height"]),
        "durations": durations,
    }
    for key in ("offsetX", "offsetY", "x", "y", "radius"):
        if key in entry:
            item[key] = float(entry[key])
    if "frameCount" in entry:
        item["landmarks"] = [
            [float(value) for value in entry[f"frame{index}"].split(",")]
            for index in range(int(entry["frameCount"]))
            if f"frame{index}" in entry
        ]
    if section == "background":
        item["centers"] = [
            [float(value) for value in entry[f"frame{index}"].split(",")]
            for index in range(int(entry["frameCount"]))
        ]
    metadata[resource] = item
    for frame in frames:
        frame.close()
    for frame in indexed:
        frame.close()
    print(f"Wrote {resource}: {len(durations)} frames, {output.stat().st_size // 1024} KiB")


def main() -> None:
    layout = read_layout()
    current = json.loads(ANIMATIONS.read_text(encoding="utf-8"))
    definitions = current["animations"]
    for section, resource in (
        ("background", "skea_idle"),
        ("pat_idle", "skea_pat_idle"),
        ("pat_end", "skea_pat_end"),
    ):
        save_animation(layout, section, resource, definitions)

    raw = RES / "raw"
    for sound in (SOURCE / "sound").glob("*.wav"):
        shutil.copy2(sound, raw / sound.name)

    ANIMATIONS.write_text(
        json.dumps(current, ensure_ascii=False, separators=(",", ":")), encoding="utf-8"
    )
    print(f"Imported {len(list((SOURCE / 'sound').glob('*.wav')))} source sounds")


if __name__ == "__main__":
    main()
