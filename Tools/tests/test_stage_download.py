"""Check that only the verified, complete single-APK build can be published."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from test_android_package import apk_fixture
from stage_android_download import stage


class PublishTests(unittest.TestCase):
    def test_publish_matching_self_contained_apk_and_reject_replaced_file_or_sidecars(self):
        with tempfile.TemporaryDirectory() as tmp:
            archive = Path(tmp) / "archive"
            archive.mkdir()
            apk = archive / "game.apk"
            apk_fixture(apk)
            size = apk.stat().st_size
            digest = hashlib.sha256(apk.read_bytes()).hexdigest()
            entry = {"path": apk.name, "signature_verified": True, "bytes": size, "sha256": digest}
            report = {"total_delivery_bytes": size, "delivery_files": [apk.name], "apks": [entry]}
            marker = archive / "Android-verification.json"
            marker.write_text(json.dumps(report))
            good = Path(tmp) / "good"
            stage(archive, good)
            self.assertEqual((good / "DelhiDrive.apk").read_bytes(), apk.read_bytes())
            self.assertIn(digest, (good / "SHA256SUMS").read_text())
            bad = Path(tmp) / "bad"
            entry["sha256"] = "ab" * 32
            marker.write_text(json.dumps(report))
            with self.assertRaisesRegex(ValueError, "checksum changed"):
                stage(archive, bad)
            self.assertFalse(bad.exists())
            report["delivery_files"].append("main.obb")
            marker.write_text(json.dumps(report))
            with self.assertRaisesRegex(ValueError, "inside the APK"):
                stage(archive, bad)
            self.assertFalse(bad.exists())
