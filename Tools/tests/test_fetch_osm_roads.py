"""Offline tests for provenance, actual hazard tags and geometry conversion."""
import importlib.util
import math
from pathlib import Path
import unittest

MODULE_PATH = Path(__file__).resolve().parents[1] / "fetch_osm_roads.py"
SPEC = importlib.util.spec_from_file_location("fetch_osm_roads", MODULE_PATH)
osm = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(osm)


def way(identifier=1, **tags):
    return {"type": "way", "id": identifier, "tags": {"highway": "residential", **tags},
            "geometry": [{"lat": 0, "lon": -.001}, {"lat": 0, "lon": .001}]}


def node(identifier, **tags):
    return {"type": "node", "id": identifier, "lat": 0, "lon": 0, "tags": tags}


class OsmConversionTests(unittest.TestCase):
    def convert(self, elements, **kwargs):
        return osm.convert({"elements": elements, "osm3s": {"timestamp_osm_base": "2026-01-01T00:00:00Z"}},
                           0, 0, 1200, name="fixture", retrieved_at="2026-01-02T00:00:00+00:00", **kwargs)

    def test_true_geometry_projection_and_provenance(self):
        document = self.convert([way(surface="asphalt", smoothness="bad")])
        road = document["roads"][0]
        self.assertEqual(road["p"], [[-11132.0, 0], [11132.0, 0]])
        self.assertEqual(road["tags"]["surface"], "asphalt")
        self.assertEqual(document["features"], [])  # bad smoothness never creates observed potholes
        self.assertEqual(document["metadata"]["license"], "ODbL-1.0")
        self.assertIn("random potholes", document["metadata"]["gameplay_synthetic"])
        self.assertEqual(document["metadata"]["osm_database_timestamp_utc"], "2026-01-01T00:00:00Z")

    def test_nodes_do_not_all_become_railway_crossings(self):
        doc = self.convert([way(), node(2, barrier="gate"), node(3, highway="crossing"),
                            node(4, railway="level_crossing"), node(5, hazard="pothole"),
                            node(6, traffic_calming="hump")])
        self.assertEqual(len(doc["crossings"]), 1)
        self.assertEqual({f["kind"] for f in doc["features"]},
                         {"barrier", "pedestrian_crossing", "railway_level_crossing", "pothole", "traffic_calming"})
        self.assertTrue(all(f["source"] == "explicit_osm_tag" for f in doc["features"]))
        self.assertTrue(all(not f["current_condition_verified"] for f in doc["features"]))

    def test_width_precedence_units_and_documented_estimates(self):
        self.assertEqual(osm.road_width({"highway": "primary", "width": "8 m", "lanes": "4"}), (8.0, "osm_width_tag"))
        self.assertEqual(osm.road_width({"highway": "service", "width": "20 ft"}), (6.1, "osm_width_tag"))
        self.assertEqual(osm.road_width({"highway": "primary", "lanes": "3"}), (9.9, "estimate_from_lanes"))
        self.assertEqual(osm.road_width({"highway": "service", "width": "unknown"}), (4.0, "estimate_from_road_class"))

    def test_bridge_tunnel_access_and_specific_override(self):
        doc = self.convert([way(1), way(2, bridge="yes"), way(3, tunnel="yes"), way(4, access="private"),
                            way(5, access="no", motorcar="yes"), way(6, layer="1")])
        self.assertEqual([r["osm_id"] for r in doc["roads"]], [1, 5])
        self.assertEqual(doc["metadata"]["excluded_way_counts"],
                         {"elevated_or_bridge": 2, "tunnel": 1, "restricted_motor_access": 1})

    def test_clip_through_segment_and_preserve_disconnected_parts(self):
        self.assertEqual(osm.clip_polyline([[-200, 0], [200, 0]], 100), [[[-100, 0], [100, 0]]])
        parts = osm.clip_polyline([[0, 0], [200, 0], [200, 200], [0, 0]], 100)
        self.assertEqual(len(parts), 2)
        self.assertTrue(all(math.hypot(*p) <= 100.2 for part in parts for p in part))
        self.assertEqual(osm.clip_polyline([[200, 200], [300, 300]], 100), [])

    def test_empty_map_fails_without_fake_fallback(self):
        with self.assertRaisesRegex(ValueError, "No drivable"):
            self.convert([])


if __name__ == "__main__":
    unittest.main()
