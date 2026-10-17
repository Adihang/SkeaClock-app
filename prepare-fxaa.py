import configparser
import json
import math
from pathlib import Path
import re
import sys

import numpy as np
from PIL import Image, PngImagePlugin

from animation_chunks import pack_frames, write_chunks


def fxaa(frame):
    rgba = np.asarray(frame.convert("RGBA"), dtype=np.float32) / 255
    rgba[..., :3] *= rgba[..., 3:4]
    padded = np.pad(rgba, ((1, 1), (1, 1), (0, 0)))
    alpha = padded[..., 3]
    center = alpha[1:-1, 1:-1]
    nw, ne = alpha[:-2, :-2], alpha[:-2, 2:]
    sw, se = alpha[2:, :-2], alpha[2:, 2:]
    low = np.minimum.reduce((center, nw, ne, sw, se))
    high = np.maximum.reduce((center, nw, ne, sw, se))
    edge = high - low >= np.maximum(1 / 32, high / 8)
    y, x = np.nonzero(edge)
    if len(x):
        # Directional FXAA uses alpha contrast so only the transparent silhouette is filtered.
        dx = -((nw + ne) - (sw + se))
        dy = (nw + sw) - (ne + se)
        reduce = np.maximum((nw + ne + sw + se) / 32, 1 / 128)
        reciprocal = 1 / (np.minimum(np.abs(dx), np.abs(dy)) + reduce)
        dx = np.clip(dx * reciprocal, -8, 8)[y, x]
        dy = np.clip(dy * reciprocal, -8, 8)[y, x]

        def sample(fraction):
            sx = np.clip(x + dx * fraction + 1, 0, frame.width + 1)
            sy = np.clip(y + dy * fraction + 1, 0, frame.height + 1)
            ix, iy = sx.astype(np.intp), sy.astype(np.intp)
            jx = np.minimum(ix + 1, frame.width + 1)
            jy = np.minimum(iy + 1, frame.height + 1)
            tx, ty = (sx - ix)[:, None], (sy - iy)[:, None]
            return ((padded[iy, ix] * (1 - tx) + padded[iy, jx] * tx) * (1 - ty)
                    + (padded[jy, ix] * (1 - tx) + padded[jy, jx] * tx) * ty)

        a = (sample(-1 / 6) + sample(1 / 6)) / 2
        b = a / 2 + (sample(-1 / 2) + sample(1 / 2)) / 4
        outside = (b[:, 3] < low[y, x]) | (b[:, 3] > high[y, x])
        rgba[y, x] = np.where(outside[:, None], a, b)
    np.divide(rgba[..., :3], rgba[..., 3:4], out=rgba[..., :3], where=rgba[..., 3:4] > 0)
    return Image.fromarray(np.clip(np.rint(rgba * 255), 0, 255).astype(np.uint8))


def prepare(config_path):
    source = config_path.parent.parent
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    config.read(config_path, encoding="utf-16")
    artwork = Path(__file__).with_name("clock-artwork.h").read_text(encoding="utf-8-sig")
    def constant(name):
        return int(re.search(rf"constexpr UINT {name} = (\d+);", artwork).group(1))
    first_frames = {"touch": constant("TransitionStartFrame"), "smash": constant("SmashStartFrame"),
                    "angry_alt": constant("AngryAltStartFrame"), "touch_end": constant("TouchBounceFrameCount")}
    sections = ("background", "touch", "smash", "angry", "angry_alt", "panic", "blank",
                "pat_idle", "pat_end", "touch_back", "touch_end_back", "touch_end",
                "touch_head", "touch_end_head", "touch_front", "touch_end_front")
    total_pixels = pinned_pixels = 0
    for section in sections:
        entry = config[section]
        original = source / entry["file"]
        correct = not section.endswith(("_head", "_front"))
        front_source = source / config[section + "_sheet"]["file"] if section.endswith("_front") else None
        folder = original.parent if section.startswith("touch_") and section != "touch_end" else source / "fxaa"
        folder.mkdir(parents=True, exist_ok=True)
        output = folder / (original.stem + (".fxaa.png" if correct else ".packed.png"))
        rebuild = not output.exists() or output.stat().st_mtime < max(
            original.stat().st_mtime, Path(__file__).stat().st_mtime,
            front_source.stat().st_mtime if front_source else 0)
        metadata = None
        if not rebuild:
            with Image.open(output) as cached:
                metadata = json.loads(cached.info.get("skea-animation", "null"))
        with Image.open(original) as gif:
            count = gif.n_frames
            first = first_frames.get(section, 0)
            stop = constant("TransitionEndFrame") + 1 if section == "touch" else count
            if section in ("touch_end_back", "touch_end_head", "touch_end_front"):
                stop = constant("TouchBounceFrameCount")
            if not 0 <= first < stop <= count:
                raise ValueError(f"Invalid playback range for {section}: {first}:{stop} of {count}")
            rebuild = rebuild or not metadata or metadata["firstFrame"] != first or len(metadata["regions"]) != stop - first
            if rebuild:
                front = Image.open(front_source) if front_source else None
                try:
                    if front and (front.width % gif.width or front.height % gif.height or
                                  (front.width // gif.width) * (front.height // gif.height) < stop):
                        raise ValueError(f"Invalid front sheet for {section}")
                    frames, delays = [], []
                    for index in range(count):
                        gif.seek(index)
                        delays.append(max(10, gif.info.get("duration", 0)))
                        if not first <= index < stop:
                            continue
                        if front:
                            columns = front.width // gif.width
                            x, y = (index % columns) * gif.width, (index // columns) * gif.height
                            image = front.crop((x, y, x + gif.width, y + gif.height)).convert("RGBA")
                        else:
                            image = fxaa(gif) if correct else gif.convert("RGBA")
                        bounds = image.getchannel("A").getbbox()
                        if bounds:
                            # Keep one transparent sampling pixel without discarding any soft alpha.
                            left, top = max(0, bounds[0] - 1), max(0, bounds[1] - 1)
                            bounds = (left, top, min(image.width, bounds[2] + 1), min(image.height, bounds[3] + 1))
                            frames.append((index, image.crop(bounds), (left, top)))
                        else:
                            frames.append((index, Image.new("RGBA", (1, 1)), (0, 0)))
                    atlas, regions = pack_frames(frames)
                    metadata = {"firstFrame": first, "delays": delays, "regions": regions, "size": atlas.size}
                    info = PngImagePlugin.PngInfo()
                    info.add_text("skea-animation", json.dumps(metadata, separators=(",", ":")))
                    atlas.save(output, pnginfo=info, compress_level=6)
                finally:
                    if front:
                        front.close()
            chunk_size = 12 if section in ("touch", "angry_alt") else 8
            chunks = write_chunks(output, metadata, chunk_size)
            entry["frameCount"] = str(count)
            entry["firstFrame"] = str(first)
            entry["sheetFrames"] = str(stop - first)
            entry["chunkSize"] = str(chunk_size)
            entry["chunks"] = str(len(chunks))
            for index, delay in enumerate(metadata["delays"]):
                entry[f"delay{index}"] = str(delay)
            for number, part in enumerate(chunks):
                entry["file" + (str(number) if number else "")] = str(part["path"].relative_to(source))
                for index, region in part["regions"].items():
                    entry[f"rect{index}"] = ",".join(map(str, region))
                pixels = math.prod(part["size"])
                total_pixels += pixels
                if number == 0 or (section == "background" and number == (constant("IdleReturnFrame") - first) // chunk_size):
                    pinned_pixels += pixels
        print(f"Chunked {section}: {stop - first}/{count} frames, {len(chunks)} chunks of up to {chunk_size}", flush=True)
    config.remove_section("touch_front_sheet")
    config.remove_section("touch_end_front_sheet")
    with config_path.open("w", encoding="utf-16") as output:
        config.write(output, space_around_delimiters=False)
    print(f"Pinned animation pixel buffers: {pinned_pixels * 4 / 1048576:.2f} MiB; all chunks: {total_pixels * 4 / 1048576:.2f} MiB", flush=True)


if __name__ == "__main__":
    prepare(Path(sys.argv[1]))
