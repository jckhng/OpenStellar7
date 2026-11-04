# OpenStellar7

An unofficial, portable C++17/SDL2 reimplementation of Stellar 7 using
player-supplied original data. The project includes an original-presentation
player, an enhanced player and an experimental OpenXR cockpit.

Original game resources and saves are supplied by the player. The `media/`
directory contains the two gameplay recordings below. The runtime does not
require DOSBox, Ghidra, Unicorn, Python or the DOS RE Harness.

## Gameplay showcase

**OpenXR cockpit and chase view** (42 seconds)

https://github.com/user-attachments/assets/14c46685-d0b6-463d-9f58-98a7f0220471

Scripted head movement shows the experimental XR cockpit, followed by
simultaneous cockpit and chase views of the same gameplay run.
[Download the full-resolution 1080p60 recording](media/OpenStellar7-OpenXR-and-chase.mp4).

**Enhanced rendering vs. original DOS** (10 seconds)

https://github.com/user-attachments/assets/39c7d487-372a-4c3f-83a0-f83d8171432e

Separate Level 1 runs show the original and enhanced cockpits side by side.
[Download the 60 fps recording with both audio tracks](media/OpenStellar7-high-res-vs-DOS.mp4).

## Download and play

The prepared release is **0.1.0-rc.1**, a prerelease for wider testing. Windows
desktop with OpenXR and Ubuntu 24.04 x86_64 desktop are supplied as two
assets-free runtime archives. See [CHANGELOG.md](CHANGELOG.md)
for filenames, validation scope and open checks. No GitHub download is available
until the repository owner publishes the prepared release.

Extract the desktop archive, create `datafile/` beside the executable and put
your supported original resources there. Run `Play.cmd` for Windows desktop play, `Play-XR.cmd` for experimental
OpenXR, or `./Play.sh` on Linux. Alternatively pass `--data-dir PATH` to the launcher. The Windows
package includes SDL2 and needs no separate Visual C++ runtime installation.
The Linux binary needs system SDL2 and a compatible C++ runtime.

Use `stellar7 --supported-data` (`stellar7.exe` on Windows) to identify the
supported resource edition. Original executables and saves are not required.

## Build from source

See [BUILDING.md](BUILDING.md). Compile from `reimpl/`, then place your original
resources in `datafile/` beside the installed executable or pass
`--data-dir PATH`. `--supported-data` prints the required files and supported
hashes; `--verify-install` checks data, writable saves and adjacent gameplay.

The executable is currently named `stellar7`. Windows installs include `Play.cmd`;
Linux installs include `Play.sh`. Saves/settings use a writable platform profile.
`--user-dir PATH` chooses a separate profile. Original source data is not modified.
High scores/preferences persist; mid-campaign save/resume is not implemented.

## Modes and controls

- Original presentation preserves recovered indexed graphics and original
  animation quirks. This is not a claim of globally exact DOS timing/fidelity.
- Enhanced adds configurable controls, scaled/smoothed presentation, chase view,
  shadows, combat feedback and optional spatial cockpit/ground/lighting.
- OpenXR uses the spatial renderer and is an experimental option. Current
  headset feedback is from Pico 4 through SteamVR on Windows. Controller actions
  are implemented; physical Pico/Xbox controller validation remains open.

Arrows/WASD drive, Space fires, Tab cycles abilities and Enter activates one.
Escape/F10 opens the original menu; choose EXIT or close the window to quit.
Enhanced: F9 cockpit/chase, F12 native options, +/- simulation speed.
XR: F8 seated recenter. Keep the game focused for keyboard input.
See [docs/CONTROLLERS.md](docs/CONTROLLERS.md) for Xbox/XInput and Pico controls, launchers
and the controller acceptance checklist. Text entry still requires a keyboard.

Gameplay defaults to an explicit native 25-updates/s cap, adjustable independently
of audio pitch. Recovered CPU timing is incomplete. Higher presentation rates
are separate from simulation speed. The eight-pose Level 3 minion gait is an
enhanced presentation change; original presentation retains its four-pose cycle.

## Status

This is the OpenStellar7 0.1.0-rc.1 prerelease, pending broader physical testing.
The owner has played parts of the game; a complete human campaign pass has not
been established. Campaign and menu features are implemented, with bounded
original comparisons and retained route
tests. Complete equivalence across every path, difficulty and platform is not
claimed. Automated retained completion covers Easy through all seven levels,
death/Continue, ending and a new Begin; full Standard/Hard victories remain
unqualified. The release checklist is in [docs/TESTING.md](docs/TESTING.md).
XR is separately experimental.

Developer guidance covers runtime ownership, contributing and publication.
See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

## Known limitations

- Original CPU/device timing has bounded recovered domains, not global fidelity.
  The explicit native speed control does not recover missing CPU work.
  Enhanced/XR title pacing adds a one-second minimum display; original loading-time
  costs and exact full-intro timing remain unrecovered.
- Full natural Standard/Hard victories, physical controller/calibration coverage
  and a separate clean-machine Windows install require additional sign-off.
- Some unsupported provenance/state domains still reject explicitly. A reached
  rejection/crash on an ordinary supported route is a release-blocking defect.
- Speech clarity, physical input latency, perceived speed, focus/quit behavior and
  long-session stability require human checks; headless results cannot certify them.
- Enhanced hardware rendering retains a software fallback. The cause of an earlier
  whole-machine lockup was not conclusively established.
- XR is experimental: controller actions have automated coverage but need live
  Pico/SteamVR validation. XR scene interpolation is incomplete; authored
  cockpit/world scale and comfort need headset validation.
- Xbox/XInput has a separate software/windowed opt-in launcher; the safe launcher
  still disables device enumeration. Real-device mapping/hotplug checks are open.
- A matched original DOS replay of the reported live-sandsled guardian arrival is
  still missing. Controlled retained tests cover the repaired object-list boundary.
- Presentation effects and the new enclosed spatial cabin do not change hitboxes
  or steering. Original indexed artwork remains the original-presentation path.
- Mid-campaign save/load is not implemented; writable persistence covers settings
  and high scores. macOS and PortMaster builds are not yet qualified.

This list is a scope statement, not an assertion that remaining defects are known.

## License and credits

Developed with [DOS RE Harness](https://github.com/jckhng/dos-re-harness).

The source code and documentation are licensed under the [MIT License](LICENSE).
See [NOTICE.md](NOTICE.md) for third-party licences and original-game rights.

Stellar 7 and its original artwork, audio and trademarks belong to their
respective rights holders.
