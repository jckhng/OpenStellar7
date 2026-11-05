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

For Standard and Hard, verify menu selection and play at least one wave during
the physical pass. Report any supported ordinary path that rejects or crashes
as a blocker regardless of automated tests.

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
| Recent engine regression | Fresh retained full campaign after the latest allocation/list changes, plus focused state/render/audio checks | Fresh Easy1-7 route and focused checks passed; legacy AD04 mismatch remains documented below |
| Clean Windows | Assets-free install/data import/writable profile/launch on a separate Windows10/11 x64 system without development tools | Tester + release review, open |
| Desktop physical | TESTING P01-P07 and C01-C07, no blocker | Primary tester, open |
| Optional controller/Linux claims | Real device/display/audio checks, or restrict release claims to checked scope | Open/limited |
| XR | TESTING X01-X07 and known limitations clearly stated | Separate preview, open |
| Final candidate freeze | Exact source/runtime hashes, matching notices/checklist and no unexplained changes after testing | Local RC freeze; see accompanying release-index.json and CHECKSUMS.sha256; stable tag pending |

### Recorded engine coverage

`stellar7_playable_full_campaign_tests` makes the current Easy route reproducible
from this checkout and player-supplied supported resources. See BUILDING.md for
the build/CTest command. It verifies the supported data hashes, creates an empty
in-memory high-score/profile store and uses the production asset loader. It never
reads or writes the player's FAME.HAL. It needs no harness, private pilot, replay
directory, DOS executable, captured state or save snapshot.

The versioned `reimpl/tests/fixtures/easy_campaign.inputs.txt` stores only held
controls, system actions and name text. Each run is `count phase bank keys action
text`; phase/bank are checked preconditions, never state assignments. Banks are
one-based; bank 8 is the ending sentinel. Key bits are left/right/forward/reverse/
fire/confirm at bits 0-5. `-` means no text. Enum values match gameplay.hpp.
Its 27,733 calls retain the previous 27,728-call gameplay controls and add five
name/Hall polls: an empty table also ranks the second death's score. No original
profile entries are distributed.

The run retains each model/synth/device owner across seven levels, two deaths and
Continues, ending/Hall/title and a new Begin plus two adjacent frames. Work budgets
10,000 and 257 compare retained state, including inactive physical pool records,
aliases, RNG, pages, indexed pixels/palette, PCM and ordered events/OPL writes.
The first five expected decimal fields retain the pre-combat-extraction sample
count and FNV-1a digests of selected state/clock observations, delivered
pixels/palettes, PCM deliveries and ordered audio events/OPL writes. The sixth
digest was recorded before the projectile-lifetime extraction. It covers live
projectile records, compact-to-physical slot mappings, active masks and all
physical projectile records, including retired slots. Fields use explicit
little-endian serialization. These expectations guard native behavior across
refactors. Keep them fixed when investigating a regression.

This is a long-running milestone check (`full-campaign` CTest label). Keep focused
combat/contact tests in the edit loop. The route covers Easy with keyboard,
music/story off and recovered indexed rendering; it does not certify every input,
Standard/Hard victories, enhanced/XR visuals or physical audio/controller behavior.
Existing original-backed checks and historical replay expectations remain separate.

Fresh Windows/MSVC and Ubuntu 24.04/GCC runs on 2026-11-05 passed the owned-profile
route with 18,203,406 samples and the same frozen digests after behavior-adapter
extraction. The targeted cohort passed 13 Windows and six Linux tests, including
ordinary/guardian policy, retired guardian emission and Type 6 visibility.
Controlled reversed-order cases verify last-solid player response and first-hostile
retirement followed by the remaining shot on the adjacent frame. They passed on
both the pre-extraction and extracted Windows engines, without changing campaign
inputs or expected digests. The resource-free allocation contract checks swap-back
order, compact/physical identity remapping, continuation-dependent presenter
caches and pending effect-slot reuse. Full-route wall time was about 2.4 minutes
on Linux and 3.6 minutes on Windows with concurrent checks.

The Level 3 guardian extraction passed the same full-campaign digests on Windows
and Ubuntu. The animation fixture passed on both platforms, and the legacy replay
matched all five trace files byte for byte.

The registered `stellar7_playable_level3_combat_retry_tests` uses the A50E
approach controls with firing held back until the first Type 47 emission, then
releases all controls. Live combat causes death at tick 1844 with the required
wave/craft/A50E/emission ancestry. The original assertions still check Continue,
the constructor RNG words and three adjacent frames after retry. The revised
route also passes on the pre-extraction Windows engine.

The registered `stellar7_playable_level3_progression_tests` clears surviving
Type 47 minions before collecting the guardian reward. It reaches Level 4 and
advances three frames there after 4,643 Level 3 combat frames. The existing
assertions cover the reward and guardian defeat, destination selection, and the
Level 4 constructor RNG and frame continuity.

The retained Easy route completed all seven levels, two death/Continue cycles,
ending/Hall/title and a new Begin plus two frames, using 27,728 calls and
18,197,743 samples. Cleanup comparisons retained state, audio and presentation
results. This is recorded automated evidence from the development harness.
A fresh 2026-11-05 run against the cleaned engine repeats this route with the
same call/sample counts; all five progression/input/text/armor/actor traces match
the earlier qualification. Physical playtesting remains separate.

The optional AD04 native replay predates the corrected hostile-projectile
collision path. It fails at event 1: current impact 6 at call 34 versus the old
next-shot event 2 at call 39. It is a historical native witness, not an original
DOS oracle or current campaign qualification. Keep its inputs/expectations intact;
current campaign qualification uses a legal run under corrected combat. The separate
Level 2-to-4 adaptive test clears surviving Level 3 minions before collecting
the guardian reward; it checks retained state/audio/pages across budgets and one
Continue, but does not replace an all-level campaign run.

### Standard and Hard campaign runs

On 2026-11-05, the development harness ran the engine at `5822093` from Begin
through all seven levels on both difficulties. The pilot used gameplay inputs
based on live observations. Each run reached the ending, high-score hall,
title menu and a new Begin with two adjacent gameplay frames. Work budgets
10,000 and 257 matched on retained state, indexed presentation, PCM and ordered
audio events. Neither route used a Continue.

| Difficulty | Gameplay calls | PCM samples | Final score |
| --- | ---: | ---: | ---: |
| Standard | 28,752 | 18,767,191 | 14,376 |
| Hard | 37,512 | 24,277,307 | 20,290 |

The repository's Easy regression also passed with all six baseline values.
Human campaign, clean Windows, Xbox/Pico, headset comfort/relaunch and Linux
physical display/audio checks remain open. Cosmetic and timing gaps are listed
in the README.

### Acceptance rule

No unresolved reproducible crash, ordinary supported-path rejection, campaign
blocker, corrupt save, major input/audio failure or host lockup. Required manual
rows must be reported. Full campaign/menus must work within the documented data,
platform and difficulty scope. Remaining cosmetic/fidelity differences can be
listed. Narrowly qualified automated results cannot stand in for physical checks.

Public 1.0 and the XR preview can have separate readiness decisions. Do not hold
the desktop source release on experimental motion-controller qualification; do not
describe XR as stable merely because desktop passed.
