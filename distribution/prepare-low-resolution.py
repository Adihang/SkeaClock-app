import configparser
import math
from pathlib import Path
import re
import sys

from PIL import Image

root = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(root))
from animation_chunks import pack_frames


def resized(image, scale):
    size = (math.ceil(image.width * scale), math.ceil(image.height * scale))
    extent = (size[0] / scale, size[1] / scale)
    padding = math.ceil(3 / scale)
    canvas = Image.new("RGBA", (math.ceil(extent[0]) + padding, math.ceil(extent[1]) + padding))
    canvas.paste(image, (0, 0))
    # Premultiplied alpha keeps transparent edges free of dark fringes.
    return canvas.convert("RGBa").resize(size, Image.Resampling.LANCZOS,
        box=(0, 0, *extent)).convert("RGBA")


def convert_animation(entry, source, target, scale):
    size = (int(entry["width"]), int(entry["height"]))
    first = int(entry["firstFrame"])
    stop = first + int(entry["sheetFrames"])
    chunk_size = int(entry["chunkSize"])
    for number, begin in enumerate(range(first, stop, chunk_size)):
        relative = Path(entry["file" + (str(number) if number else "")])
        frames = []
        with Image.open(source / relative) as sheet:
            for index in range(begin, min(begin + chunk_size, stop)):
                x, y, w, h, ox, oy = map(int, entry[f"rect{index}"].split(","))
                image = Image.new("RGBA", size)
                image.paste(sheet.crop((x, y, x + w, y + h)).convert("RGBA"), (ox, oy))
                image = resized(image, scale)
                bounds = image.getchannel("A").getbbox()
                if bounds:
                    left, top = max(0, bounds[0] - 1), max(0, bounds[1] - 1)
                    bounds = (left, top, min(image.width, bounds[2] + 1), min(image.height, bounds[3] + 1))
                    frames.append((index, image.crop(bounds), (left, top)))
                else:
                    frames.append((index, Image.new("RGBA", (1, 1)), (0, 0)))
        atlas, regions = pack_frames(frames)
        output = target / relative
        output.parent.mkdir(parents=True, exist_ok=True)
        atlas.save(output, compress_level=6)
        for index, region in regions.items():
            entry[f"rect{index}"] = ",".join(map(str, region))


def scale_coordinates(entry, scale):
    for key in entry:
        if key in ("width", "height"):
            entry[key] = str(math.ceil(int(entry[key]) * scale))
        elif re.fullmatch(r"frame\d+", key):
            entry[key] = ",".join(f"{(float(value) + .5) * scale - .5:.8f}"
                                  for value in entry[key].split(","))
        elif key in ("x", "y", "centerX", "centerY", "tipX", "tipY"):
            entry[key] = f"{(float(entry[key]) + .5) * scale - .5:.8f}"
        elif key in ("offsetX", "offsetY", "radius"):
            entry[key] = f"{float(entry[key]) * scale:.8f}"


def prepare(scale):
    source = root / "sources"
    target = source / "low-resolution"
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    with (source / "locate data" / "clock-layout.ini").open(encoding="utf-16") as file:
        config.read_file(file)
    for name in config.sections():
        entry = config[name]
        if "sheetFrames" in entry:
            convert_animation(entry, source, target, scale)
        elif "file" in entry:
            output = target / entry["file"]
            output.parent.mkdir(parents=True, exist_ok=True)
            with Image.open(source / entry["file"]) as image:
                resized(image.convert("RGBA"), scale).save(output, compress_level=6)
        scale_coordinates(entry, scale)
    layout = target / "locate data" / "clock-layout.ini"
    layout.parent.mkdir(parents=True, exist_ok=True)
    with layout.open("w", encoding="utf-16") as file:
        config.write(file, space_around_delimiters=False)
    print(f"Prepared {scale:.0%} images: {target}", flush=True)


if __name__ == "__main__":
    prepare(float(sys.argv[1]))
