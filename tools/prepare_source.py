"""Audit and archive a standalone OpenStellar7 source checkout; never publish it."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile

MANIFEST = "SOURCE_MANIFEST.json"
GENERATED = {".git", ".work", "build", "dist", "out", "releases", "datafile", "__pycache__", ".vs", ".vscode", ".idea"}
LOCAL_ROOT_ENTRIES = {"myfiles", "Play.cmd", "Play-gamepad.cmd", "Play-XR.cmd"}
SOURCE_SUFFIXES = {".cpp", ".hpp", ".h", ".inc", ".ipp", ".cmake", ".in", ".cmd", ".sh", ".txt", ".md", ".py", ".json", ".yml", ".yaml"}
PRIVATE_NAMES = {"cmakeuserpresets.json", "cmakecache.txt", "fame.hal", "resource.cfg", "enhanced.cfg"}
PERSONAL_PATH = re.compile(rb"(?i)(?:[a-z]:[\\/](?:users[\\/][^\\/\s\"']+|[^\\/\s\"']+[\\/]dev)[\\/]|/mnt/[a-z]/(?:users/[^/\s\"']+|[^/\s\"']+/dev)/|/(?:home|Users)/[^/\s\"']+/)")
REQUIRED = {"README.md", "LICENSE", "NOTICE.md", "BUILDING.md", "docs/TESTING.md", "reimpl/CMakeLists.txt", "reimpl/third_party/ymfm/LICENSE", "tools/package/payload/notices/LICENSE.txt", "tools/prepare_source.py"}
SHOWCASE_MEDIA = {"media/OpenStellar7-OpenXR-and-chase.mp4", "media/OpenStellar7-high-res-vs-DOS.mp4"}
ZIP_EPOCH = (1980, 1, 1, 0, 0, 0)


def _files(root: Path) -> list[Path]:
    if (root / ".git").exists():
        # Include force-added ignored files; never inherit the surrounding harness index.
        names = subprocess.check_output(["git", "-C", str(root), "ls-files", "--cached", "--others", "--exclude-standard", "-z"])
        return [root / name.decode("utf-8") for name in sorted(set(names.split(b"\0")) - {b""})]
    result = []
    pending = [root]
    while pending:
        for file in pending.pop().iterdir():
            if file.is_symlink():
                raise ValueError(f"Symlink is not source payload: {file.relative_to(root)}")
            if file.parent == root and file.name in LOCAL_ROOT_ENTRIES:
                continue
            if file.is_dir():
                if file.name not in GENERATED and not file.name.startswith(("build-", "cmake-build-")):
                    pending.append(file)
            else:
                result.append(file)
    return sorted(result)


def _scan(root: Path) -> dict[str, str]:
    hashes = {}
    for file in _files(root):
        name = file.relative_to(root).as_posix()
        if name == MANIFEST:
            continue
        lower = file.name.lower()
        if file.is_symlink() or file.relative_to(root).parts[0] in LOCAL_ROOT_ENTRIES or any(part in GENERATED or part == "CMakeFiles" for part in file.relative_to(root).parts):
            raise ValueError(f"Private/generated source entry: {name}")
        if lower in PRIVATE_NAMES or lower == ".env" or lower.startswith(".env."):
            raise ValueError(f"Machine-local configuration: {name}")
        showcase = name in SHOWCASE_MEDIA
        if not showcase and file.suffix.lower() not in SOURCE_SUFFIXES and file.name not in {"LICENSE", ".gitignore", ".gitattributes"}:
            raise ValueError(f"Non-source payload: {name}")
        data = file.read_bytes()
        if showcase and (len(data) < 12 or data[4:8] != b"ftyp"):
            raise ValueError(f"Invalid showcase MP4: {name}")
        if PERSONAL_PATH.search(data) or (not showcase and (b"\0" in data or data.startswith((b"MZ", b"PK\x03\x04")))):
            raise ValueError(f"Binary or personal workspace path: {name}")
        hashes[name] = hashlib.sha256(data).hexdigest()
    missing = REQUIRED - hashes.keys()
    if missing:
        raise ValueError(f"Missing source dependencies: {sorted(missing)}")
    return dict(sorted(hashes.items()))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--check", action="store_true")
    action.add_argument("--write-manifest", action="store_true")
    action.add_argument("--archive", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    hashes = _scan(root)
    expected = {"formatVersion": 1, "project": "OpenStellar7", "assetsBundled": False, "files": hashes}
    showcase = sorted(SHOWCASE_MEDIA & hashes.keys())
    if showcase:
        expected["showcaseMedia"] = showcase
    manifest = root / MANIFEST
    if args.write_manifest:
        manifest.write_text(json.dumps(expected, indent=2) + "\n", encoding="utf-8")
    elif json.loads(manifest.read_text(encoding="utf-8")) != expected:
        raise ValueError("Source manifest is stale; review changes before --write-manifest")
    if args.archive:
        archive = args.archive.resolve()
        if archive.exists():
            raise ValueError("Archive output must be new")
        if archive.is_relative_to(root) and archive.relative_to(root).parts[0] not in {"dist", "releases", "out"}:
            raise ValueError("Put archives outside source or under dist/releases/out")
        archive.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED) as output:
            for name in sorted([*hashes, MANIFEST]):
                entry = zipfile.ZipInfo("OpenStellar7/" + name, ZIP_EPOCH)
                entry.create_system = 3
                entry.external_attr = (0o100755 if name.endswith(".sh") else 0o100644) << 16
                entry.compress_type = zipfile.ZIP_DEFLATED
                output.writestr(entry, (root / name).read_bytes())
    print(json.dumps({"project": "OpenStellar7", "files": len(hashes) + 1, "assetsBundled": False, "manifestCurrent": True, "showcaseMedia": showcase}))


if __name__ == "__main__":
    main()
