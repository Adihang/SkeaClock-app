"""Reproduce Android artwork from the repository's Windows PE resources.
Requires Pillow and pefile. No executable code from the PE is run.
"""
from pathlib import Path
import math
import pefile
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'android/app/src/main/res'
pe = pefile.PE(str(ROOT / 'distribution/SkeaClock.exe'))
assets = {}
for kind in pe.DIRECTORY_ENTRY_RESOURCE.entries:
    if kind.id == 10:
        for entry in kind.directory.entries:
            data = entry.directory.entries[0].data.struct
            assets[entry.id] = pe.get_data(data.OffsetToData, data.Size)
import io
character = Image.open(io.BytesIO(assets[101])).convert('RGBA')
pixels = character.load()
for y in range(character.height):
    for x in range(character.width):
        r,g,b,a = pixels[x,y]
        if g > 180 and r < 90 and b < 90: pixels[x,y] = (0,0,0,0)
character = character.resize((678,597),Image.Resampling.LANCZOS)
mask = Image.new('L',(480,480)); ImageDraw.Draw(mask).ellipse((12,12,468,468),fill=255)
font_path = next((p for p in [Path('/System/Library/Fonts/Helvetica.ttc'),Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')] if p.exists()),None)
font = ImageFont.truetype(str(font_path),30) if font_path else ImageFont.load_default(size=30)
for flags in range(8):
    im=Image.new('RGBA',(480,480)); d=ImageDraw.Draw(im)
    if flags & 1: d.ellipse((12,12,468,468),fill='#DFF7F5')
    im.alpha_composite(character,(-79,176))
    im.putalpha(Image.composite(im.getchannel('A'),Image.new('L',im.size),mask))
    d=ImageDraw.Draw(im)
    if flags & 2:
        for i in range(60):
            a=i*math.pi/30; r=215 if i%5 else 207
            d.line((240+math.sin(a)*r,240-math.cos(a)*r,240+math.sin(a)*225,240-math.cos(a)*225),fill='#54636D',width=3 if i%5 else 4)
    if flags & 4:
        for i in range(1,13):
            a=i*math.pi/6
            d.text((240+math.sin(a)*193,240-math.cos(a)*193),str(i),font=font,anchor='mm',fill='#323F48',stroke_width=2,stroke_fill='white')
    im.save(OUT/f'drawable-mdpi/dial_{flags}.png')
Image.new('RGBA',(480,480)).save(OUT/'drawable-mdpi/empty_dial.png')
for name,resource,cx,cy,tx,ty in [('hour',115,12,1,4,77),('minute',116,6,3,17,89)]:
    hand=Image.open(io.BytesIO(assets[resource])).convert('RGBA').resize((int(Image.open(io.BytesIO(assets[resource])).width*1.9),int(Image.open(io.BytesIO(assets[resource])).height*1.9)),Image.Resampling.LANCZOS)
    canvas=Image.new('RGBA',(480,480)); canvas.alpha_composite(hand,(round(240-cx*1.9),round(240-cy*1.9)))
    angle=math.degrees(math.atan2(tx-cx,-(ty-cy)))
    canvas.rotate(angle,resample=Image.Resampling.BICUBIC,center=(240,240)).save(OUT/f'drawable-mdpi/{name}_hand.png')
(OUT/'raw/base_alert.wav').write_bytes(assets[125])
(OUT/'raw/touch.wav').write_bytes(assets[120])

# Preserve the complete native frame sequences for the Android canvas renderer.
import re
import json
from PIL import ImageChops, ImageOps

layout = assets[100].decode('utf-16')
sections = {}
current = None
for line in layout.splitlines():
    if line.startswith('[') and line.endswith(']'):
        current = line[1:-1]
        sections[current] = {}
    elif current and '=' in line:
        key, value = line.split('=', 1)
        sections[current][key] = value
ids_by_path = {}
for resource_id, line in enumerate(re.findall(r'^file=(.*)$', layout, re.M), 101):
    ids_by_path.setdefault(line, resource_id)

animation_specs = {
    'skea_idle': 'background', 'skea_transition': 'touch',
    'skea_touch_back': 'touch_back', 'skea_touch_head': 'touch_head',
    'skea_touch_front': 'touch_front', 'skea_touch_end': 'touch_end',
    'skea_touch_end_back': 'touch_end_back', 'skea_touch_end_head': 'touch_end_head',
    'skea_touch_end_front': 'touch_end_front', 'skea_smash': 'smash',
    'skea_angry': 'angry', 'skea_panic': 'panic', 'skea_blank': 'blank',
    'skea_angry_alt': 'angry_alt',
}
metadata = {'animations': {}, 'landmarks': {}, 'sounds': {}}
for name, section in animation_specs.items():
    info = sections[section]
    resource_id = ids_by_path[info['file']]
    source = Image.open(io.BytesIO(assets[resource_id]))
    frames, delays = [], []
    for index in range(source.n_frames):
        source.seek(index)
        frame = source.convert('RGBA')
        red, green, blue, _ = frame.split()
        key = ImageChops.multiply(
            ImageChops.multiply(red.point([255 if v < 65 else 0 for v in range(256)]),
                                green.point([255 if v > 127 else 0 for v in range(256)])),
            blue.point([255 if v < 65 else 0 for v in range(256)]))
        frame.putalpha(ImageOps.invert(key))
        indexed = frame.convert('RGB').quantize(colors=255, method=Image.Quantize.MEDIANCUT)
        indexed.paste(255, mask=key)
        palette = (indexed.getpalette() or [])[:765] + [0, 255, 0]
        indexed.putpalette(palette)
        frames.append(indexed)
        delays.append(max(10, int(source.info.get('duration', 30))))
    (OUT / 'raw' / f'{name}.gif').parent.mkdir(parents=True, exist_ok=True)
    frames[0].save(OUT / 'raw' / f'{name}.gif', save_all=True, append_images=frames[1:],
                   duration=delays, loop=0, transparency=255, background=255,
                   disposal=2, optimize=False)
    entry = {'resource': name, 'width': source.width, 'height': source.height,
             'durations': delays}
    for key in ('offsetX', 'offsetY', 'frame', 'x', 'y', 'radius'):
        if key in info: entry[key] = float(info[key])
    if 'frameCount' in info:
        entry['landmarks'] = [[float(v) for v in info[f'frame{i}'].split(',')]
                              for i in range(int(info['frameCount']))]
    if section == 'background':
        entry['centers'] = [[float(v) for v in info[f'frame{i}'].split(',')]
                            for i in range(int(info['frameCount']))]
    metadata['animations'][name] = entry

for name, resource_id in [('idle1',117),('idle2',118),('smash',119),
                          ('touch_start',120),('touch_end',121),('blank1',122),
                          ('answer',123),('alert',124),('base_alert',125)]:
    (OUT / 'raw' / f'{name}.wav').write_bytes(assets[resource_id])
    metadata['sounds'][name] = name
(ROOT / 'android/app/src/main/assets').mkdir(parents=True, exist_ok=True)
(ROOT / 'android/app/src/main/assets/skea_animation.json').write_text(
    json.dumps(metadata, ensure_ascii=False, separators=(',', ':')))
