"""Author a two-storey window tile over the shipped CC0 plaster maps, for flat mobile geometry."""
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2] / "SourceAssets/Environment"

def bake():
    maps = {role: Image.open(ROOT / f"T_Facade_{role}.jpg").convert("RGB") for role in ("Color","Normal","Rough")}
    maps = {role: image.resize((1024,1024)) for role,image in maps.items()}
    contexts = {role: ImageDraw.Draw(image) for role,image in maps.items()}
    def rect(box, color, roughness, normal=(128,128,255)):
        contexts["Color"].rectangle(box,fill=color)
        contexts["Rough"].rectangle(box,fill=(roughness,)*3)
        contexts["Normal"].rectangle(box,fill=normal)

    for row in range(2):
        for col in range(2):
            x,y = col*512,row*512
            l,t,r,b = x+76,y+94,x+438,y+366
            rect((l-9,t-9,r+9,b+9), (143,135,123),210,(128,162,248))
            rect((l,t,r,b), (49,52,53),130)
            for h in range(t+5,b-3):
                v = (h-t)/(b-t)
                shade = int(22*(1-v)) + (9 if col == row else 0)
                contexts["Color"].line((l+5,h,r-5,h),fill=(49+shade,69+shade,77+shade))
            contexts["Rough"].rectangle((l+5,t+5,r-5,b-5),fill=(53,)*3)
            contexts["Normal"].rectangle((l+5,t+5,r-5,b-5),fill=(128,128,255))
            margin = l+int((r-l)*.56)
            rect((margin,t+2,margin+5,b-2), (91,89,84),91)
            rect((l+3,t+58,r-3,t+63), (89,88,83),94)
            rect((l-14,b+8,r+13,b+17), (193,183,165),182,(128,86,249))
            rect((l-14,b+18,r+13,b+24), (132,126,117),192)
    entries={}
    for role,image in maps.items():
        output=ROOT/f"T_FacadeBays_{role}.jpg"
        image.save(output,quality=91,subsampling=0)
        entries[role]={"file":output.name,"sha256":hashlib.sha256(output.read_bytes()).hexdigest()}
    (ROOT/"facade_bays.json").write_text(json.dumps(entries,indent=2)+"\n")
if __name__ == "__main__":
    bake()
