# Building OpenStellar7

C++17 compiler, CMake 3.19+ and SDL2 development files are required. No original
data, reverse-engineering tools or network downloads are required by CMake.
Obtain dependencies separately through their official distribution/package manager.
Source code is in `reimpl/`; the runtime notice/launcher payload is under
`tools/package/payload/`. Keep this relative layout.

## Linux

Ubuntu 24.04 x86_64 is the current checked Linux environment. Install your
distribution's C++ toolchain, CMake, Ninja, pkg-config and SDL2 development package.

```sh
cmake -S reimpl -B build/enhanced -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSTELLAR7_ENHANCED=ON \
  '-DSTELLAR7_PACKAGE_VERSION=0.1.0-rc.1'
cmake --build build/enhanced --target stellar7 -j4
cmake --install build/enhanced --prefix "$PWD/dist/Stellar7"
./dist/Stellar7/Play.sh --data-dir /path/to/original-data
```

## Windows x64

Use Visual Studio 2022 C++ tools and the official matching-architecture SDL2 SDK.
Set `SDL2_DIR` to its CMake package directory when it is not discoverable.

```powershell
cmake -S reimpl -B build/enhanced -G "Visual Studio 17 2022" -A x64 `
  -DSTELLAR7_ENHANCED=ON -DSDL2_DIR="$env:SDL2_DIR" `
  '-DSTELLAR7_PACKAGE_VERSION=0.1.0-rc.1'
cmake --build build/enhanced --config Release --target stellar7
cmake --install build/enhanced --config Release --prefix "$PWD/dist/Stellar7"
```

The project links the Microsoft C++ runtime statically. Windows still uses its
normal OS libraries and the SDL2 DLL. Linux uses system SDL2/C++ libraries.
The Windows toolchain's SDK is a build dependency, not a player installation.

## Original presentation, tests and archives

Use a separate build directory with `-DSTELLAR7_ENHANCED=OFF` for the original
presentation. Tests are enabled by default. For a player-only build, configure
with `-DBUILD_TESTING=OFF`; this omits test executables and test-only dependencies.
The optional `stellar7-tools` target remains controlled separately by
`STELLAR7_BUILD_DIAGNOSTICS`.

Resource-backed tests are optional:
configure `STELLAR7_TEST_RESOURCE_DIR` to your own supported original data.
Tests are CMake targets; build the desired ones before CTest, for example:

```sh
cmake --build build/enhanced --target stellar7_spatial_view_tests
ctest --test-dir build/enhanced -C Release --output-on-failure -R '^stellar7_spatial_view_tests$'
cpack --config build/enhanced/CPackConfig.cmake -C Release -B dist
```

Resource-free install/profile/launch regressions also run without the harness:

```sh
cmake --build build/enhanced --config Release --target stellar7_native_profile_tests stellar7_native_install_tests stellar7_native_player_launch_tests stellar7_highscore_tests
ctest --test-dir build/enhanced -C Release --output-on-failure -R '^stellar7_(native_profile|native_install|native_player_launch|highscore)_tests$'
```

Their generated profiles stay under the build directory. Run CTest serially
against any one build directory; independent build directories may run in parallel.

Pool retirement, compact/physical contact remapping and pending effect reuse have
a fast resource-free contract test:

```sh
cmake --build build/enhanced --config Release --target stellar7_campaign_allocation_tests
ctest --test-dir build/enhanced -C Release --output-on-failure -R '^stellar7_campaign_allocation_tests$'
```

The full retained campaigns for Easy, Standard and Hard have repository-owned
inputs and expectations.
Configure `STELLAR7_TEST_RESOURCE_DIR` with all 20 supported original RES files.
The shared resource-test setup requires uppercase filenames for `COCKPIT.RES`,
`STELLAR.RES`, `STELART.RES`, `LEVEL1.RES`, and `LEVEL2.RES`. The full campaign
test accepts either case for the other filenames. Then run:

```sh
cmake --build build/enhanced --config Release --target stellar7_playable_full_campaign_tests
ctest --test-dir build/enhanced -C Release --output-on-failure -L '^full-campaign$'
```

No harness, private replay directory or player save is needed. The test uses an
empty in-memory profile for each route and checks startup, seven levels, ending,
Hall/title and a new Begin with adjacent frames. The Easy route also exercises
two Continues. Allow several minutes per route and run the group at integration
milestones. See [coverage and limits](docs/TESTING.md#recorded-engine-coverage).

CPack creates assets-free runtime archives. It does not publish them. Do not add
your `datafile/`, profiles, private test evidence or original captures to archives.
Windows enhanced builds also create `Play-gamepad.cmd` beside the build executable.
To opt into Xbox/XInput in an installed desktop build, copy that script from
`reimpl/windows/` beside the installed executable. The ordinary CPack archive
retains its safe launcher; optional gamepad hardware qualification is separate.
See [docs/CONTROLLERS.md](docs/CONTROLLERS.md).

## Developer diagnostics

Optional compiler checks are disabled for ordinary builds. Enable
`STELLAR7_DEVELOPER_WARNINGS=ON` for project runtime warnings (`/W4` on MSVC,
`-Wall -Wextra -Wpedantic` on GCC/Clang). Vendor translation units are outside
that warning policy; warnings are not automatically treated as errors.

With a GCC/Clang toolchain that provides ASan/UBSan, use a separate debug build:

```sh
cmake -S reimpl -B build/sanitized -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR7_ENHANCED=ON \
  -DSTELLAR7_DEVELOPER_WARNINGS=ON -DSTELLAR7_SANITIZERS=ON
cmake --build build/sanitized --target stellar7_ordinary_behavior_tests stellar7_campaign_guardian_children_tests -j4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/sanitized --output-on-failure -R '^stellar7_(ordinary_behavior|campaign_guardian_children)_tests$'
```

Sanitizers instrument the selected targets and linked core, including vendor
code; classify any vendor finding separately. They add runtime dependencies and
overhead and are not release-package settings. A passing cohort establishes only
its exercised paths. Do not disable checks to hide recovered-arithmetic issues.

Developer trace/render/replay commands are separate from the player. To build
them, reconfigure with `-DSTELLAR7_BUILD_DIAGNOSTICS=ON`, then build target
`stellar7-tools` and run `stellar7-tools --help`. Historical diagnostic arguments
are unchanged; use the new executable name. This optional tool shares the player
loader/runtime and is not installed or included by CPack. Normal launch/import/
verification commands remain on `stellar7`; diagnostics are disabled by default.

## Experimental OpenXR

Configure enhanced SDL2 with `-DSTELLAR7_OPENXR=ON` and
`-DSTELLAR7_OPENXR_INCLUDE_DIR=/path/to/OpenXR-SDK/include`. Obtain the official
Khronos OpenXR loader for the target system separately, retaining its notices.
On Windows place `openxr_loader.dll` beside the executable. A configured runtime
supporting desktop OpenGL/WGL is required. Source builds do not install/select one.
Copy `reimpl/windows/Play-XR.cmd` beside that executable for the preview launcher.
It uses a separate OpenXR profile and process-local settings. Supply original data
in an adjacent `datafile/` directory or pass `--data-dir PATH` to the launcher.

```powershell
$env:STELLAR7_SAFE_MODE='0'
$env:STELLAR7_HOST_CONTROLLERS='0'
$env:STELLAR7_XR='1'
$env:STELLAR7_XR_UNITS_PER_METRE='200'
./dist/Stellar7/stellar7.exe --play --data-dir 'D:/OriginalStellar7'
```

These settings are scoped to the current terminal. `STELLAR7_HOST_CONTROLLERS=0`
disables SDL device discovery; OpenXR actions remain enabled. Close the terminal
or clear these variables before a desktop run. Windows safe mode otherwise
suppresses XR, and the authored world scale must be explicit.

The Windows release bundles the XR-enabled executable with Play.cmd,
Play-gamepad.cmd and Play-XR.cmd in one folder. Each launcher selects its own
mode and profile. Ordinary CPack output is the desktop install layout. To assemble
the combined release, include the XR launcher, official loader and its complete
vendor notice set in that folder, then regenerate the exact package manifest.
Source builds obtain the loader separately as described above. Linux release
packages cover desktop play. Linux/macOS/handheld OpenXR remains unqualified.
Existing profiles and filenames remain compatible with the project.
