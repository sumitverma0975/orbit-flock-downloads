"""Download the redistributable 1K Poly Haven surface maps used by the mobile renderer."""
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess


def read_url(url):
    return subprocess.check_output(["curl", "-fsSL", "--retry", "2", "--max-time", "45", url])

DEST = Path(__file__).resolve().parents[1] / "SourceAssets/Environment"
SURFACES = {"Asphalt": "asphalt_04", "Ground": "dry_ground_01",
            "Facade": "white_plaster_02", "Roof": "concrete_pavement_02"}
CHANNELS = {"Color": "Diffuse", "Normal": "nor_dx", "Rough": "Rough"}

def material(item):
    name, slug = item
    files = json.loads(read_url("https://api.polyhaven.com/files/" + slug))
    facts = json.loads(read_url("https://api.polyhaven.com/info/" + slug))
    result = {"material": name, "url": "https://polyhaven.com/a/" + slug,
              "license": "CC0-1.0", "authors": facts.get("authors", {}), "textures": {}}
    for role, key in CHANNELS.items():
        info = files[key]["1k"]["jpg"]
        data = read_url(info["url"])
        if len(data) != info["size"] or hashlib.md5(data).hexdigest() != info["md5"]:
            raise ValueError("Publisher checksum mismatch for " + info["url"])
        path = DEST / f"T_{name}_{role}.jpg"
        path.write_bytes(data)
        result["textures"][role] = {"file": path.name, "url": info["url"],
                                    "sha256": hashlib.sha256(data).hexdigest()}
    return result

if __name__ == "__main__":
    DEST.mkdir(exist_ok=True, parents=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as workers:
        manifest = list(workers.map(material, SURFACES.items()))
    (DEST / "surfaces.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("Downloaded and verified four CC0 materials.")
