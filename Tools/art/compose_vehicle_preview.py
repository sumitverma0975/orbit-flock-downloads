#!/usr/bin/env python3
"""Assemble unmodified Blender renders into a labelled asset-inspection contact sheet."""
from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'SourceAssets/Vehicles'
OUT=ROOT/'Docs/VehiclePreviews'
entries=json.loads((BASE/'prepared_manifest.json').read_text())['assets']
assert len(entries)==12
font_dir=Path('/usr/share/fonts/truetype/dejavu')
def font(size,bold=False):
    return ImageFont.truetype(str(font_dir/('DejaVuSans-Bold.ttf' if bold else 'DejaVuSans.ttf')),size)
canvas=Image.new('RGB',(2020,1325),'#101b27')
draw=ImageDraw.Draw(canvas)
draw.text((34,23),'DELHI DRIVE  /  VEHICLE ART',font=font(34,True),fill='#f2d5a6')
draw.text((35,72),'Offline Blender render of prepared source models — not Unreal or gameplay screenshots.',font=font(21),fill='#c0cedb')
for i,entry in enumerate(entries):
    path=OUT/(entry['id']+'.png')
    if not path.is_file(): raise RuntimeError('Missing rendered model: '+str(path))
    with Image.open(path) as im:
        im.load()
        assert im.size==(480,310)
        x=20+(i%4)*500;y=122+(i//4)*380
        canvas.paste(im.convert('RGB'),(x,y))
    draw.text((x+9,y+318),entry['display_name'],font=font(20,True),fill='#edf3f8')
    label='Attributed model' if entry['source_asset_id'] else 'Original fictional design'
    draw.text((x+9,y+347),f"{label}  ·  {entry['triangles']:,} triangles",font=font(14),fill='#a7baca')
draw.text((32,1280),'BMW: rifdanzz  |  G-Class: Lexyc16  |  AMG GT: Yan Carvalho  ·  CC BY 4.0  ·  Full sources and changes: SourceAssets/Vehicles/ATTRIBUTION.md',font=font(16),fill='#b3c1cf')
canvas.save(ROOT/'Docs/vehicle-garage-preview.png',optimize=True)
print(ROOT/'Docs/vehicle-garage-preview.png')
