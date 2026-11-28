"""Delivery guard regressions; synthetic fixtures are not runnable APKs."""
import io
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import setup_map
import verify_android_package as verify


def pak_fixture():
    # Minimal structural footer fixture with a small index; not a playable pak.
    return b"INDEX123" + verify.PAK_MAGIC + struct.pack("<Iqq", 11, 0, 8) + b"\0" * 20


def apk_fixture(path, machine=183, payload=True, engine="libUnreal.so"):
    header = bytearray(64)
    header[:6] = b"\x7fELF\x02\x01"
    struct.pack_into("<H", header, 18, machine)
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("AndroidManifest.xml", b"\x03\x00\x08\x00fixture")
        archive.writestr("classes.dex", b"dex\n035\x00fixture")
        archive.writestr("lib/arm64-v8a/" + engine, bytes(header) + b"\0" * 1_000_000)
        if payload:
            archive.writestr("assets/DelhiDrive.pak", pak_fixture())


class AndroidDeliveryTests(unittest.TestCase):
    def test_budget_counts_apk_and_expansion_together(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for name, size in (("game.apk", 1_500_000_000), ("main.obb", 1_500_000_001)):
                with (root / name).open("wb") as stream:
                    stream.truncate(size)  # Sparse files; no multi-GB test allocation.
            with self.assertRaisesRegex(ValueError, "3,000,000,001"):
                verify.collect_delivery(root)

    def test_exact_decimal_three_gb_limit_is_allowed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with (root / "game.apk").open("wb") as stream:
                stream.truncate(verify.MAX_DELIVERY_BYTES)
            self.assertEqual(verify.collect_delivery(root)[3], 3_000_000_000)

    def test_other_engine_apk_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "other.apk"
            apk_fixture(path, engine="libgodot_android.so")
            with self.assertRaisesRegex(ValueError, "libUnreal.so"):
                verify.inspect_unreal_apk(path)

    def test_wrong_machine_in_arm64_folder_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "wrongabi.apk"
            apk_fixture(path, machine=62)
            with self.assertRaisesRegex(ValueError, "ARM64 ELF"):
                verify.inspect_unreal_apk(path)

    def test_native_library_without_cooked_game_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "nocontent.apk"
            apk_fixture(path, payload=False)
            with self.assertRaisesRegex(ValueError, "no Unreal pak"):
                verify.inspect_unreal_apk(path)

    def test_embedded_expansion_pak_is_recognized(self):
        expansion = io.BytesIO()
        with zipfile.ZipFile(expansion, "w") as archive:
            archive.writestr("DelhiDrive/Content/Paks/DelhiDrive.pak", pak_fixture())
        apk = io.BytesIO()
        with zipfile.ZipFile(apk, "w") as archive:
            archive.writestr("assets/main.obb.png", expansion.getvalue())
        apk.seek(0)
        with zipfile.ZipFile(apk) as archive:
            self.assertTrue(verify.archive_has_cooked_payload(archive))

    def test_invalid_pak_index_is_not_accepted(self):
        payload = b"INDEX123" + verify.PAK_MAGIC + struct.pack("<Iqq", 11, 999, 999) + b"\0" * 20
        self.assertFalse(verify.has_pak_footer(io.BytesIO(payload), len(payload)))

    def test_failed_signature_never_writes_success_report(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            apk_fixture(root / "unsigned.apk")
            failure = subprocess.CompletedProcess([], 1, "", "DOES NOT VERIFY")
            with mock.patch.object(verify, "find_apksigner", return_value=Path("apksigner")), mock.patch.object(verify.subprocess, "run", return_value=failure):
                with self.assertRaisesRegex(ValueError, "signature verification failed"):
                    verify.verify_delivery(root)
            self.assertFalse((root / "Android-verification.json").exists())

    def test_linux_installed_editor_without_cmd_variant(self):
        with tempfile.TemporaryDirectory() as temp:
            engine = Path(temp)
            binaries = engine / "Engine/Binaries/Linux"
            binaries.mkdir(parents=True)
            (binaries / "UnrealEditor").touch()
            self.assertEqual(setup_map.editor_executable(engine, "Linux"), binaries / "UnrealEditor")
            (binaries / "UnrealEditor-Cmd").touch()
            self.assertEqual(setup_map.editor_executable(engine, "Linux"), binaries / "UnrealEditor-Cmd")

    def test_stale_editor_marker_cannot_mask_commandlet_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            marker = root / "Saved/Setup/import_environment.json"
            marker.parent.mkdir(parents=True)
            marker.write_text('{"success": true}')
            with mock.patch.object(setup_map, "ROOT", root), mock.patch.object(setup_map.subprocess, "run"):
                with self.assertRaisesRegex(RuntimeError, "did not complete"):
                    setup_map.run_editor_task(Path("UnrealEditor-Cmd"), Path("DelhiDrive.uproject"), "import_environment.py")


if __name__ == "__main__":
    unittest.main()
