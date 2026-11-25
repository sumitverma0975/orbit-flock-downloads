"""Bake the offline navigation textures from the same coordinates used for physics."""
import json
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
SIZE = 1536

def bake(city):
    roads = json.loads((ROOT/f"Content/Data/{city}.json").read_text())
    buildings = json.loads((ROOT/f"Content/Data/{city}_buildings.json").read_text())
    extent = roads["radius_m"]*100
    def pixels(points):
        return [(round((x/extent + 1)*SIZE/2), round((y/extent + 1)*SIZE/2)) for x,y in points]
    canvas=Image.new("RGB",(SIZE,SIZE),(19,39,43))
    draw=ImageDraw.Draw(canvas)
    for b in buildings["buildings"]:
        draw.polygon(pixels(b["p"]), fill=(46,62,63))
    major={"primary","trunk","secondary","primary_link","trunk_link","secondary_link"}
    for road in sorted(roads["roads"],key=lambda r: r["h"] in major):
        arterial = road["h"] in major
        width=max(2,round(road["w"]*100*SIZE/(2*extent)))
        points=pixels(road["p"])
        draw.line(points,fill=(13,26,28),width=width+2,joint="curve")
        draw.line(points,fill=(183,181,155) if arterial else (103,128,129),width=width,joint="curve")
    canvas.resize((1024,1024),Image.Resampling.LANCZOS).save(ROOT/f"SourceAssets/Cartography/T_Map_{city}.png",optimize=True)

if __name__=="__main__":
    for name in ("delhi","chandigarh"): bake(name)
