Stellar 7 native reimplementation - SDL2 / Windows x64

Extract the complete ZIP into a folder. Create a datafile subfolder beside
stellar7.exe and copy your legally obtained original game .RES files there.
Double-click Play.cmd. Resources are validated and read directly from datafile.
For another location, use Play.cmd --data-dir "C:\Original Game".
No original game data is included. No network connection or download is needed.

Scores and preferences live separately in:
  %LOCALAPPDATA%\Stellar7
  FAME.HAL    scores and preferences

Previously imported profile data\ installations still work when no adjacent
datafile folder exists. An explicit --data-dir overrides both locations.
Direct play ignores FAME.HAL in the resource folder and uses the profile save.

The original installation is left unchanged. Reimport preserves an existing
profile's FAME.HAL. Keep that file when updating this program. Import/verification
errors do not repair or reset an existing score file. Campaign save/resume is
not provided; calibration is session state.

Menus: arrows select, press and release Enter to activate. Escape goes back in
manual submenus and attract. Use EXIT or window close from the main menu.
BEGIN starts the campaign. PREFS includes sound, music, difficulty, mouse,
joystick, Story and graphic detail. Escape or F10 opens the in-game menu.
Default speed is 25 updates/second. Use +/- (or keypad +/-) to change speed;
the window title shows the selected rate. This is a native pacing setting.

Gameplay:
  Arrows       turn / forward / reverse
  Space        fire
  Tab          cycle selected system
  Enter        activate selected system
  I E J B C S T  Decoy / Shield / Jump / Bomb / Scanner / Super Cannon / Turbo
  Escape / F10 open the in-game menu

Joystick: enable it in PREFS, then Alt+C in gameplay or a manual menu.
Follow the upper-left, lower-right and center prompts. Arrows override axes.
Button1 fires; button2 release cycles systems; both activate the selected
system. Either button confirms a menu widget. One two-axis/two-button device
is supported. F10 preferences blocks nested calibration.

If setup fails, run these commands from a terminal in this folder:
  stellar7.exe --help
  stellar7.exe --verify-install
  stellar7.exe --verify-install --data-dir "C:\Original Game"
  stellar7.exe --import-data "C:\Path To\Your Original Installation"

Verification requires an existing profile FAME.HAL and never creates one.
To initialize a separate profile and play directly from original resources:
  Play.cmd --data-dir "C:\Original Game" --user-dir "C:\My Profile"
Use the same --data-dir/--user-dir options for later verification.

Keep SDL2.dll beside stellar7.exe. The Microsoft C++ runtime is statically linked;
no separate Microsoft runtime DLLs or runtime installer are required.
Supported platform target: Windows10/11 x64. See notices for SDL, implementation, ymfm, Microsoft and
original-data boundaries. Local validation does not establish clean-machine
qualification. This candidate still requires physical controls, focus, display,
pace and listening checks on the final build; original fidelity is separate.

One natural Easy campaign through all seven levels, ending and restart is
verified. Rare guardian allocation variants outside that route remain rejected.
Physical playtesting and clean-machine validation are still required.
This is a test candidate, not public 1.0.
