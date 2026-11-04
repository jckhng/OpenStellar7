# Release playtest

Use the exact candidate listed in its local release receipt. Record its EXE
SHA256 (`Get-FileHash stellar7.exe -Algorithm SHA256`), Windows/runtime version,
launcher, difficulty, speed and renderer. Use a fresh `--user-dir` for one run;
also test your existing profile. Do not replace your original data or saves.

Report each row as PASS, FAIL, NOT RUN or UNAVAILABLE. A required row that is
not run remains open. A subjective visual/audio observation needs a human report.
Do not publish original data or private profiles with a bug report.

## First pass: approximately 20 minutes

| ID | Action | Pass condition |
| --- | --- | --- |
| P01 | Cold start with Story enabled; let narration play | Speech intelligible at normal pitch; no garbling, clipping, stalls or cut-off logo/title |
| P02 | Restart and skip at several intro stages with a normal key/Escape | Prompt skip, no stuck or held-key action on entering menu/game |
| P03 | Navigate original menus and F12 options; tap and hold controls | Each tap responds, no obvious input backlog, text aligned, settings persist after restart |
| P04 | Drive, turn, fire and change speed; test cockpit and F9 chase | Responsive controls, stable audio pitch, no accumulating latency; chosen speed comfortable |
| P05 | F10/Escape in gameplay; resume; Alt+Tab/focus loss and return | Menu opens instead of quitting; no stuck steering/fire; safe focus recovery |
| P06 | Enemies hit player with projectiles; collide at full armor | Visible armor damage from actual hits; no unexplained universal instant death or immunity |
| P07 | Jump, shield, decoy and remaining available abilities | Controls/HUD/audio correspond to action; no rejection or crash on supported use |

## Campaign pass: required before desktop 1.0

Complete one uninterrupted Easy campaign through the ending, Hall/title and a
new BEGIN. Include a normal death and Continue/retry. Record each level and
transition separately. Do not mark the whole campaign passed after testing only
Level 1 or by starting independent level fixtures.

| ID | Action | Pass condition |
| --- | --- | --- |
| C01 | Levels 1-2: waves, guardian, reward/warp, Continue | Natural progression and adjacent transitions; no retained-owner/audio storage errors |
| C02 | Level 3 fuel bay and guardian/minions | Refuelling contact works; enhanced full gait cycles both sides without rapid/partial motion |
| C03 | Levels 4-7: every wave, guardian and transition | No stalls, missing actors or crashes; ending and new BEGIN complete |
| C04 | Observe illustrated intermissions and ending | Expected music/voice segments play without pitch/speed issues; music-only segments are not failures solely for lacking speech |
| C05 | Encounter repair pickup after 3 consecutive kills of same eligible type | Visible pickup, collectible once, correct system repair; ordinary mixed kills need not drop one |
| C06 | Leave an ordinary enemy alive near guardian arrival | Record moving enemy, radar, quota context and difficulty; no assumption that all globally allocated enemies must die |
| C07 | Save preferences/high score and relaunch | Correct writable profile, persistence, original source data unchanged |

Standard/Hard: verify menu selection and play at least one wave; full victories
remain explicitly limited until separately tested. Report any supported ordinary
path that rejects or crashes as a blocker regardless of automated tests.

## XR pass: experimental Windows mode

Start SteamVR/Pico 4, launch Play-XR.cmd in the XR release folder, focus the
window and sit normally. F8 recenters. Use Play.cmd to test desktop mode
from the same package. The two launchers use separate profiles.

| ID | Action | Pass condition |
| --- | --- | --- |
| X01 | Start wearing headset, then F8; look front/left/right/rear/up/down | Comfortable seat; solid floor/roof/rear and side consoles, intentional windows, no detached/transparent walls or seams |
| X02 | Lean gently and turn while driving; cockpit and F9 chase | Stable cabin, no body/head clipping in normal seated movement, stationary world grid and readable instruments |
| X03 | Approach sandsleds and ground-contact scenery | Useful enemy scale, visible sled, no persistent near-ground flicker |
| X04 | Play combat including Level 3 guardian | Both eyes correct, tolerable frame pacing, complete gait, working aim/shadows/effects |
| X05 | Close normally; relaunch twice without restarting SteamVR | Clean shutdown and new session acquired each time |
| X06 | Play 20-30 minutes | No accumulating lag, severe discomfort, crash or host lockup |
| X07 | Test Pico controller actions using docs/CONTROLLERS.md | Drive/fire/abilities, both menus, view and recenter work; tracking/focus loss and reconnect never leave held controls |

Controller actions are experimental until X07 passes on the real device/runtime.
Run the Xbox checklist in docs/CONTROLLERS.md before advertising Xbox support as tested.
Keyboard-only XR remains available with `STELLAR7_XR_CONTROLLERS=0`.

## What to send back

Candidate hash/launcher, test IDs, PASS/FAIL, level, view, difficulty/speed,
what happened immediately before failure, exact error and a short clip/time
if useful. Keep the private host log. Never send original resource/save files
in a public issue. A clean Windows 10/11 test on a separate machine/VM remains
separate from your development-host playtest; its prepared kit records the details.

## Release readiness

The first prerelease is v0.1.0-rc.1. Partial owner playtesting and automated
coverage are recorded separately. Use the accompanying release-index.json and
CHECKSUMS.sha256 to identify the packaged source and binaries.

| Gate | Required result | Owner/status |
| --- | --- | --- |
| Source boundary | Standalone Windows/Linux build from the repository root; no dependency on private harness files | Use the matching release-index.json for this snapshot's results |
| Distribution audit | Original resources, captures, profiles, binaries, SDKs and local paths/config absent; third-party notices retained | Source and runtime archive audits passed |
| Recent engine regression | Fresh retained full campaign after the latest allocation/list changes, plus focused state/render/audio checks | Easy1-7 route and focused checks passed |
| Clean Windows | Assets-free install/data import/writable profile/launch on a separate Windows10/11 x64 system without development tools | Tester + release review, open |
| Desktop physical | TESTING P01-P07 and C01-C07, no blocker | Primary tester, open |
| Optional controller/Linux claims | Real device/display/audio checks, or restrict release claims to checked scope | Open/limited |
| XR | TESTING X01-X07 and known limitations clearly stated | Separate preview, open |
| Final candidate freeze | Exact source/runtime hashes, matching notices/checklist and no unexplained changes after testing | Local RC freeze; see accompanying release-index.json and CHECKSUMS.sha256; stable tag pending |

### Recorded engine coverage

The retained Easy route completed all seven levels, two death/Continue cycles,
ending/Hall/title and a new Begin plus two frames, using 27,728 calls and
18,197,743 samples. Cleanup comparisons retained state, audio and presentation
results. This is recorded automated evidence from the development harness.
Packaging reuses that evidence and records fresh build/import/archive checks.

Human campaign, clean Windows, Xbox/Pico, headset comfort/relaunch and Linux
physical display/audio checks remain open. Full Standard/Hard victories remain
unqualified. Cosmetic and timing gaps are listed in the README.

### Acceptance rule

No unresolved reproducible crash, ordinary supported-path rejection, campaign
blocker, corrupt save, major input/audio failure or host lockup. Required manual
rows must be reported. Full campaign/menus must work within the documented data,
platform and difficulty scope. Remaining cosmetic/fidelity differences can be
listed. Narrowly qualified automated results cannot stand in for physical checks.

Public 1.0 and the XR preview can have separate readiness decisions. Do not hold
the desktop source release on experimental motion-controller qualification; do not
describe XR as stable merely because desktop passed.
