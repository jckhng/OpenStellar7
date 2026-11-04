# Development

Run commands from the repository root.

## Runtime structure

The player is native C++17. Reverse-engineering tools and original DOS execution
are development oracles, not runtime dependencies. Original data is supplied by
the player and decoded at startup.

| Responsibility | Implementation |
| --- | --- |
| Installed entry, data/profile validation | `reimpl/src/main.cpp`, `native_player_entry.cpp`, `gameplay_asset_loader.cpp`, `resource_file.cpp` |
| Startup and retained gameplay/audio handoff | `native_startup_adoption.cpp`, `include/stellar7/native_startup.hpp`, `include/stellar7/native_session.hpp` |
| Campaign coordination and sample-clock deliveries | `native_playable_campaign.cpp` and the other `native_*` session modules |
| Simulation state and ordered frame dispatch | `gameplay.cpp`, `include/stellar7/gameplay.hpp` |
| Lifecycle, loading and physical record reuse | `playable_campaign_lifecycle.cpp`, `playable_campaign_loader.cpp`, `campaign_entity_pools.cpp`, `campaign_enemy_projectiles.cpp` |
| Ordinary/guardian behavior | `ordinary_behavior.cpp`, `campaign_guardian.cpp`, `campaign_guardian_children.cpp`, `level5_guardian.cpp` |
| Presentation snapshots and animation callbacks | `campaign_presentation.cpp` |
| Indexed rasterization, clipping and enhanced scene observation | `gameplay_renderer.cpp`; small shared helpers in `gameplay_render_support.cpp` |
| Enhanced/spatial/XR views | `world_presentation.cpp`, `spatial_scene.cpp`, `spatial_view.cpp`, `chase_view.cpp`, `xr_presentation.cpp` |
| Host input, output and OpenXR lifecycle | `frontend_sdl.cpp`, SDL adapters, GL presenter and `openxr_*` modules |
| Audio synthesis, driver semantics and device timing | `sound_blaster.cpp` behind `SoundBlasterSynth`, clock/work modules and bundled ymfm |

Unqualified C++ filenames are in `reimpl/src/`; `include/...` paths are relative
to `reimpl/`.

`GameplayModel` keeps one owner for actor records, physical slots, incarnations,
ordered lists and simulation timers. `NativeGameplaySession` retains its model,
audio owner and presentation clock across frames, level changes, failure/Continue
and ending/title transitions. Rendered views must not replace these owners.

Presentation is not uniformly passive. Snapshot creation writes live fixed records
and retains callback membership; main/radar callbacks advance some animations.
Preserve their existing order and call count. Additional camera renders and head
poses observe held snapshots without advancing simulation or audio.

The root presentation packet carries projected cockpit pixels. Its chase child
also contains the spatial world used by desktop spatial views and OpenXR; an XR
cockpit view does not require a second simulation. The native 25-updates/s control
is separate from presentation refresh and audio pitch.

Enhanced/XR installed players also enable `NativeMenuFeatures::nativeTitlePacing`:
the full Stellar 7 title stays visible for at least one second before credits.
Escape and held pointer/controller buttons dismiss it. Native idle work advances
the retained devices and music while waiting; it does not recover original loading
costs. The recovered `releaseMenus` feature set keeps its original two title polls.

Recovered addresses, work descriptors and identity checks document original
widths, aliases, callback order and supported domains. Keep the driver mechanics
inside their existing adapters. Do not replace work with average frame costs,
copy later original state into a retained run, or suppress an unsupported-path
exception to make a test pass.

The main frame dispatcher remains large and coupled. Further branch/context
refactoring is documented debt, not evidence of global DOS equivalence. Before
changing it, identify affected aliases and compare retained state, indexed pixels
and palette, ordered audio and device phase at the relevant boundaries.

Developer trace/replay commands are available in the optional `stellar7-tools`
target (`STELLAR7_BUILD_DIAGNOSTICS=ON`). It is off by default and excluded from
runtime installation. Developer warnings and GCC/Clang ASan/UBSan are also opt-in;
see [BUILDING.md](../BUILDING.md). Tests include controlled entries and retained
routes; neither finite coverage nor a showcase video qualifies every input,
difficulty, original timing behavior or physical headset/controller combination.

## Contributing

Build from `reimpl/` using [BUILDING.md](../BUILDING.md). Keep the simulation,
resource decoding and audio independent of SDL, platform code and the optional
OpenXR host. Enhanced presentation must not silently alter original-presentation
behavior. Use separate build directories for different modes/dependencies.

For a bug report, include the release version, OS, launcher, level, view,
difficulty/speed, exact error and reproduction steps. State whether the issue
also occurs with a fresh `--user-dir`. Never attach original resources, saves,
memory dumps or private machine logs without reviewing their contents.

Run the focused build/CTest targets relevant to a change. The resource-free
tests in BUILDING run without original data; resource-backed tests require your
own supported data and should stay outside the source tree. A passing component
test does not certify a complete campaign or physical audio/controller behavior.

Do not commit original game files, decoded art/audio, personal profiles, release
archives, SDKs, build directories, editor settings or CMakeUserPresets.json.
Pass local dependency paths as configure arguments or environment variables.
Retain third-party copyright/licence notices. Project-authored contributions use
the MIT licence in LICENSE; this does not license the original game assets.

SOURCE_MANIFEST.json binds the prepared source snapshot. Its assetsBundled field
tracks original runtime resources. The showcaseMedia list identifies the two
repository videos separately. A source edit requires
a new reviewed source manifest and matching release metadata before distribution.
Use `python tools/prepare_source.py --check` to audit the checkout; after reviewing
changes, `--write-manifest` refreshes it and `--archive PATH` creates a new source
ZIP. This optional maintainer tool requires Python 3.9+; builds and players do not.
Its resource-free regression checks run with
`python -B -m unittest discover -s tools/tests -v` from the repository root.
The current manifest is not a claim that a later edited checkout is unchanged.
.gitattributes preserves the snapshot's bytes across Git line-ending settings.
No hosted CI service is required to build or contribute.

## Repository and release preparation

This folder is the standalone source repository candidate. Release downloads are
kept separately: upload them as GitHub Release assets, not ordinary Git files.
Original data files, SDKs, profiles, private captures and machine-specific
configuration stay outside the source history and runtime downloads. The two
reviewed showcase recordings under media/ are included in the repository. Keep the MIT licence and all
third-party notices. The source snapshot and download hashes are separate records.

### Review before checking in

1. Read README.md, BUILDING.md, CHANGELOG.md and docs/TESTING.md. Confirm the
   first tag will be `v0.1.0-rc.1`, with GitHub's **prerelease** setting enabled.
2. Run `python tools/prepare_source.py --check`, then review SOURCE_MANIFEST.json
   and the matching release CHECKSUMS/release index. Python 3.9+ is only needed
   for this maintainer check, not to build or run the game.
3. Confirm `.gitignore` excludes local data/config, SDK/build outputs and archives.
   `.gitattributes` deliberately prevents line-ending conversion of hashed files.
4. Build/test as documented if you change source. Record failures and untested
   hardware honestly. No CI setup, code signing or installer is required for this
   initial prerelease. Stable 1.0 uses the [release readiness checklist](TESTING.md#release-readiness).

### Initial check-in â€” only after review

These commands are instructions, not actions already performed. Run them from
the repository root after accepting the candidate. Create an empty GitHub repository
named OpenStellar7 without an automatic README/licence, then substitute its URL.

```sh
git init --initial-branch=main
git add --dry-run .
git add .
git diff --cached --stat
git diff --cached --check
git diff --cached
git commit -m "Prepare OpenStellar7 release candidate"
git remote add origin YOUR_GITHUB_REPOSITORY_URL
git push -u origin main
```

Inspect the staged file list/diff before the commit; `.gitignore` is a guard,
not proof that files are safe. Never force-add original game data or local config.
Do not use the surrounding development/harness repository as this project's root.

The attributes retain existing source bytes and recognize CRLF during whitespace
review. `git diff --cached --check` may still report a few existing trailing spaces
or blank end lines in recovered code/tests and verbatim vendor licence files.
Review these warnings; they are not hidden or automatically reformatted by source
preparation. They do not prevent Git from committing the reviewed files.

After reviewing source edits, refresh the snapshot explicitly:

```sh
python tools/prepare_source.py --write-manifest
python tools/prepare_source.py --check
python tools/prepare_source.py --archive dist/OpenStellar7-0.1.0-rc.1-source.zip
```

Archive output must be new. The tool excludes Git/build/profile directories and
checks Git's tracked and untracked nonignored file list in an initialized repo.
Force-added original assets or local configuration fail the audit. It never
stages, commits, pushes or uploads files. Rebuild runtime archives after code
changes; regenerating a source manifest does not qualify an existing executable.

### GitHub prerelease â€” only after source review/push

Create a draft release targeting the reviewed commit, use tag `v0.1.0-rc.1` and
title `OpenStellar7 0.1.0-rc.1`, enable **Set as a pre-release**, and paste
CHANGELOG.md as its description. Attach the Windows ZIP and Linux tar.gz,
CHECKSUMS.sha256 and release-index.json from the matching prepared release folder.
The optional standalone instructions can accompany them. Verify filenames/hashes
before publishing; do not upload the whole development workspace or original data.

GitHub-generated source archives come from the committed tree. The explicit source
ZIP is the byte-checked prepared snapshot. If the reviewed commit changes any
snapshot content, rebuild/re-audit the source archive and release identity records
before uploading. Do not silently reuse earlier source hashes.

Collect broader tester feedback on the prerelease. Mark missing tests NOT RUN;
do not infer speech quality, input latency, controller comfort or a human campaign
pass from automation. Promote to stable 1.0 only when the advertised scope and
remaining blockers have been reviewed. XR may remain a separate experimental
preview after desktop is stable.
