"""Compile production C++ topology helper and validate actual OSM extracts."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class RoadGraphTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            raise unittest.SkipTest("A C++17 compiler is needed for production route tests")
        cls.temporary = tempfile.TemporaryDirectory(prefix="delhidrive-roads-")
        cls.executable = Path(cls.temporary.name) / "graph-tests"
        subprocess.run([compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-pedantic",
                        str(Path(__file__).with_suffix(".cpp")), "-o", str(cls.executable)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_disconnected_and_directed_scenarios(self):
        result = subprocess.run([str(self.executable)], check=True, capture_output=True, text=True)
        self.assertIn("scenarios passed", result.stdout)

    def test_real_district_routes(self):
        for city in ("delhi", "chandigarh"):
            with self.subTest(city=city):
                data = json.loads((ROOT / "Content" / "Data" / (city + ".json")).read_text())
                segments = Path(self.temporary.name) / (city + ".txt")
                lines = []
                for road in data["roads"]:
                    width = road["w"] * 100
                    if width < 300 or width > 5000:
                        continue
                    tag = road.get("tags", {}).get("oneway", "no")
                    direction = -1 if tag == "-1" else (1 if tag in ("yes", "true", "1") else 0)
                    for a, b in zip(road["p"], road["p"][1:]):
                        lines.append(f"{a[0]} {a[1]} {b[0]} {b[1]} {width} {direction}\n")
                segments.write_text("".join(lines))
                result = subprocess.run([str(self.executable), str(segments)], check=True, capture_output=True, text=True)
                self.assertEqual(result.stdout.count("checkpoints=8"), 4)
                print(city + ": " + result.stdout.strip().replace("\n", "; "))


if __name__ == "__main__":
    unittest.main()
