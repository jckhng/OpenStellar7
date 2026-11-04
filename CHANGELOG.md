# Changelog

## 0.1.0-rc.1 — prepared 2026-11-05

First prepared standalone OpenStellar7 source and player prerelease. Unpublished
until owner review; broader physical playtesting remains open.

- Portable C++17/SDL2 player with player-supplied original data and writable
  settings/high scores, independent of reverse-engineering tools.
- Seven-level campaign, guardians, intermissions, death/Continue, ending and
  original menus. Automated retained completion covers Easy; other difficulties
  have bounded checks, not complete campaign qualification.
- Enhanced scaling, chase camera, shadows, combat feedback, native options and
  configurable controls. Includes pickup-motion, independent guardian-pose and
  energy-width corrections from the latest implementation sweep.
- Experimental OpenXR spatial cockpit for Pico 4 through SteamVR, bundled
  with the Windows enhanced player.
  Xbox/Pico input is implemented but real-device sign-off remains pending.
- Two runtime downloads for Windows and Linux, with matching hashes,
  notices and explicit known limitations. Windows includes desktop and XR modes. Original game data
  is player supplied. The source includes two showcase recordings under media/.
- Separate installed-player/diagnostic entry points, resource loading, campaign
  lifecycle/pools/projectiles/presentation and model/rendering responsibilities.
  Developer warnings and sanitizers are optional; see docs/DEVELOPMENT.md and BUILDING.md.
- One-second minimum full-title exposure in enhanced/XR mode, continuing music
  and responsive dismissal; recovered title timing remains separately available.
- Original two-line LEVEL/STORY PANELS preference backgrounds and shading.
- Standalone source audit/archive tool and GitHub bug-report template; no hosted
  CI, private harness or machine-specific configuration required.



### Downloads

| Archive | Contents |
| --- | --- |
| OpenStellar7-0.1.0-rc.1-Windows-x64.zip | Enhanced desktop, Xbox launcher and experimental OpenXR with loader notices |
| OpenStellar7-0.1.0-rc.1-Linux-x86_64.tar.gz | Ubuntu 24.04 x86_64 desktop player using system SDL2/C++ libraries |

Supply original game resources separately in datafile beside the executable or
pass --data-dir PATH. Extract the desktop ZIP and run Play.cmd. XR uses
Play-XR.cmd with a configured OpenXR runtime. Linux uses ./Play.sh.
CHECKSUMS.sha256 verifies downloads and release-index.json records source/runtime
identities and fresh versus recorded results. The two showcase MP4s in the README
are stored under media/ in the repository. GitHub provides source downloads
from the release tag. The Windows test kit is retained for maintainer validation.

### Validation and limitations

Standalone builds and focused regressions cover SDL2 hosts, view rendering,
OpenXR lifecycle/actions, import and save preservation. Recorded retained Easy
campaign coverage reaches all levels and a new Begin. Human full-campaign,
clean Windows, physical Xbox/Pico and XR comfort/relaunch checks remain open.
Full Standard/Hard victories, macOS and PortMaster builds remain unqualified.
Persistence covers settings and scores. Mid-campaign save/resume is absent.
See [README.md](README.md#known-limitations) and [docs/TESTING.md](docs/TESTING.md).
