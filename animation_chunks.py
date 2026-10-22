import json
import math
from pathlib import Path

from PIL import Image, PngImagePlugin


def pack_frames(frames):
    widest = max(image.width for _, image, _ in frames)
    limits = {widest * columns for columns in range(1, min(8, len(frames)) + 1)}
    limits.add(max(widest, math.ceil(math.sqrt(sum(image.width * image.height for _, image, _ in frames)))))
    ordered = sorted(frames, key=lambda item: item[1].height, reverse=True)
    best = None
    for limit in sorted(limits):
        x = y = row_height = width = 0
        regions = {}
        for index, image, offset in ordered:
            if x and x + image.width > limit:
                x, y, row_height = 0, y + row_height, 0
            regions[str(index)] = [x, y, image.width, image.height, *offset]
            width = max(width, x + image.width)
            row_height = max(row_height, image.height)
            x += image.width
        height = y + row_height
        if max(width, height) <= 32767 and (best is None or width * height < best[0] * best[1]):
            best = width, height, regions
    width, height, regions = best
    atlas = Image.new("RGBA", (width, height))
    for index, image, _ in frames:
        atlas.paste(image, tuple(regions[str(index)][:2]))
    return atlas, regions


def write_chunks(output, metadata, chunk_size):
    chunks, source = [], None
    first = metadata["firstFrame"]
    stop = first + len(metadata["regions"])
    try:
        for number, begin in enumerate(range(first, stop, chunk_size)):
            end = min(begin + chunk_size, stop)
            path = output.with_name(f"{output.stem}.{number:03d}.png")
            rebuild = not path.exists() or path.stat().st_mtime < max(
                output.stat().st_mtime, Path(__file__).stat().st_mtime)
            part = None
            if not rebuild:
                with Image.open(path) as cached:
                    part = json.loads(cached.info.get("skea-animation", "null"))
            rebuild = rebuild or not part or part["firstFrame"] != begin or len(part["regions"]) != end - begin
            if rebuild:
                if source is None:
                    source = Image.open(output)
                frames = []
                for index in range(begin, end):
                    x, y, w, h, ox, oy = metadata["regions"][str(index)]
                    frames.append((index, source.crop((x, y, x + w, y + h)), (ox, oy)))
                atlas, regions = pack_frames(frames)
                part = {"firstFrame": begin, "regions": regions, "size": atlas.size}
                info = PngImagePlugin.PngInfo()
                info.add_text("skea-animation", json.dumps(part, separators=(",", ":")))
                atlas.save(path, pnginfo=info, compress_level=6)
            chunks.append({"path": path, **part})
    finally:
        if source is not None:
            source.close()
    return chunks

