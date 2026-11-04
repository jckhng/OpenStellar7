Stellar 7 portable reimplementation - Linux SDL2 candidate

Requires a C++ runtime and system SDL2 (2.0.10 or later). Original game data is
supplied separately by the player and is not included in this package.
Current prebuilt release candidates are checked on Ubuntu 24.04 x86_64.
Other distributions may need a build from the supplied portable source.

Put the supported original .RES files in datafile/ beside stellar7, then run
./Play.sh. You can instead run ./Play.sh --data-dir /path/to/original/data.
Use ./stellar7 --supported-data to see the accepted resource set and
./stellar7 --help for import, profile and headless verification options.

Saves and settings use the writable user profile; original resources are not
modified. --user-dir selects a separate writable profile. No SDK, Python,
DOSBox or reverse-engineering tool is required to play.

Enhanced build: F9 toggles cockpit/chase; F12 opens native display, audio,
camera and control options. Custom F9 bindings keep priority. Fullscreen,
controller and audio behavior still require testing on the target desktop.

This is a candidate, not public 1.0 certification. See notices/ for licenses
and the boundary between implementation and player-supplied original data.
