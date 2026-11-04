from pathlib import Path
import json
import subprocess
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import prepare_source


class PreparedSourceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in prepare_source.REQUIRED:
            file = self.root / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text("Synthetic source fixture\n", encoding="utf-8")

    def invoke(self, *args):
        return subprocess.run([sys.executable, "-B", str(Path(prepare_source.__file__)), "--root", str(self.root), *args], capture_output=True, text=True)

    def test_stale_source_is_rejected(self):
        self.assertEqual(self.invoke("--write-manifest").returncode, 0)
        self.assertEqual(self.invoke("--check").returncode, 0)
        (self.root / "README.md").write_text("Edited source\n")
        self.assertIn("manifest is stale", self.invoke("--check").stderr)

    def test_assets_and_local_configuration_are_rejected(self):
        for name in ["LEVEL1.RES", "CMakeUserPresets.json", ".env.local"]:
            with self.subTest(name=name):
                file = self.root / name
                file.write_text("Synthetic non-source fixture\n")
                self.assertNotEqual(self.invoke("--write-manifest").returncode, 0)
                file.unlink()

    def test_archive_excludes_local_builds_and_original_data(self):
        for name in ["build/generated.bin", "datafile/LEVEL1.RES"]:
            file = self.root / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b"Synthetic private output\0")
        self.assertEqual(self.invoke("--write-manifest").returncode, 0)
        archive = self.root / "dist/source.zip"
        self.assertEqual(self.invoke("--archive", str(archive)).returncode, 0)
        with zipfile.ZipFile(archive) as output:
            self.assertFalse(any("/build/" in name or "/datafile/" in name for name in output.namelist()))
            expected = json.loads((self.root / prepare_source.MANIFEST).read_text())["files"]
            self.assertEqual(len(output.namelist()), len(expected) + 1)

    def test_force_added_asset_is_rejected(self):
        subprocess.run(["git", "init", "--quiet", str(self.root)], check=True)
        (self.root / ".gitignore").write_text("*.RES\n")
        asset = self.root / "LEVEL1.RES"
        asset.write_text("Synthetic original data\n")
        self.assertEqual(self.invoke("--write-manifest").returncode, 0)
        subprocess.run(["git", "-C", str(self.root), "add", "-f", "LEVEL1.RES"], check=True)
        self.assertNotEqual(self.invoke("--check").returncode, 0)

    def test_local_launchers_and_myfiles_are_excluded(self):
        self.assertEqual(self.invoke("--write-manifest").returncode, 0)
        for name in ["Play.cmd", "Play-gamepad.cmd", "Play-XR.cmd", "myfiles/private.bin"]:
            file = self.root / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text("Synthetic local file\n")
        self.assertEqual(self.invoke("--check").returncode, 0)
        archive = self.root / "dist/source.zip"
        self.assertEqual(self.invoke("--archive", str(archive)).returncode, 0)
        with zipfile.ZipFile(archive) as output:
            self.assertFalse(any(name.startswith("OpenStellar7/myfiles/") or name in {
                "OpenStellar7/Play.cmd", "OpenStellar7/Play-gamepad.cmd", "OpenStellar7/Play-XR.cmd"
            } for name in output.namelist()))

    def test_force_added_local_launcher_is_rejected(self):
        subprocess.run(["git", "init", "--quiet", str(self.root)], check=True)
        (self.root / ".gitignore").write_text("/Play.cmd\n")
        (self.root / "Play.cmd").write_text("Synthetic local launcher\n")
        self.assertEqual(self.invoke("--write-manifest").returncode, 0)
        subprocess.run(["git", "-C", str(self.root), "add", "-f", "Play.cmd"], check=True)
        self.assertIn("Private/generated source entry", self.invoke("--check").stderr)

    def test_showcase_video_is_hashed_and_archived(self):
        name = "media/OpenStellar7-OpenXR-and-chase.mp4"
        video = self.root / name
        video.parent.mkdir()
        video.write_bytes(b"\0\0\0\x18ftypisom" + b"\0" * 12)
        result = self.invoke("--write-manifest")
        self.assertEqual(result.returncode, 0, result.stderr)
        manifest = json.loads((self.root / prepare_source.MANIFEST).read_text())
        self.assertEqual(manifest["showcaseMedia"], [name])
        archive = self.root / "dist/source.zip"
        self.assertEqual(self.invoke("--archive", str(archive)).returncode, 0)
        with zipfile.ZipFile(archive) as output:
            self.assertEqual(output.read("OpenStellar7/" + name), video.read_bytes())
        video.write_bytes(video.read_bytes() + b"edited")
        self.assertIn("manifest is stale", self.invoke("--check").stderr)

    def test_other_videos_and_invalid_showcase_payloads_are_rejected(self):
        for name, data in [
            ("media/private-capture.mp4", b"\0\0\0\x18ftypisom" + b"\0" * 12),
            ("media/OpenStellar7-OpenXR-and-chase.mp4", b"MZ invalid video"),
        ]:
            with self.subTest(name=name):
                video = self.root / name
                video.parent.mkdir(exist_ok=True)
                video.write_bytes(data)
                self.assertNotEqual(self.invoke("--write-manifest").returncode, 0)
                video.unlink()


if __name__ == "__main__":
    unittest.main()
