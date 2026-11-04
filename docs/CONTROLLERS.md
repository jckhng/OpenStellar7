# Controllers

Enhanced input uses SDL2's portable GameController API. On Windows the optional
Xbox launcher selects SDL's XInput backend. Pico uses OpenXR actions through
SteamVR, independent of SDL device enumeration. Neither adds a Windows dependency
to the portable simulation. Physical controller sign-off is still pending.

## Xbox / XInput

Use `Play-gamepad.cmd` beside the enhanced Windows executable. It selects a
window and software rendering, enables only SDL's XInput device backend, and
keeps native controller/button settings writable. The packaged `Play.cmd`
provides the device-free fallback. All launcher settings are process-local.
For an installed source build, copy `reimpl/windows/Play-gamepad.cmd` beside
`stellar7.exe`; the ordinary CPack archive still ships its safe `Play.cmd`.

| Default input | Action |
| --- | --- |
| Left stick / D-pad | Drive / navigate menus |
| A | Fire / confirm |
| X | Activate selected ability |
| Left shoulder | Cycle ability |
| Right shoulder | Jump |
| Y / B | Decoy / bomb; B backs out in menus |
| Start | Original game menu |
| Back | Native options, unless rebound |

Configure game buttons in native options. Keyboard F9 changes cockpit/chase;
F12 opens options. Release all controls after focus gain or reconnection before
driving. Text entry and controls without a button binding use the keyboard or
cycle/activate ability controls. Linux uses SDL's normal portable controller path,
not the Windows XInput launcher.

## Pico 4 through SteamVR

Start SteamVR with the Pico controllers connected, then launch `Play-XR.cmd` in
the combined Windows package. Actions are enabled by default. Release controls once after the
headset gains focus. Keyboard input still needs desktop window focus; XR input
uses headset focus. SteamVR owns action remapping; the native SDL button-capture
screen does not capture OpenXR buttons.

| Default input | Action |
| --- | --- |
| Left stick | Drive / navigate menus |
| Right trigger | Fire / confirm |
| Left trigger | Activate selected ability |
| Right A | Cycle ability / confirm |
| Right B | Jump / menu back |
| Left X | Shield |
| Left Y | Native options |
| Left menu | Original game menu |
| Right stick click | Cockpit / chase |
| Left stick click | Seated recenter (also keyboard F8) |

Pico and Touch profile suggestions are supplied; SteamVR may expose Pico as a
compatible profile. If a system-reserved button is unavailable, remap the named
action in SteamVR. Tank steering still aims the weapons: hand aiming, haptics and
virtual hands are not implemented. A keyboard is needed for name/text entry.
Set `STELLAR7_XR_CONTROLLERS=0` before launching for a keyboard-only fallback.

## Physical acceptance

Record candidate hash, launcher, controller model and runtime version:

1. From a cold launch, skip the intro, start gameplay and test every mapped input.
2. Open both menus; navigate, confirm, back out and return to play without firing.
3. Hold fire/steering, lose focus, release, then regain focus. No stuck action or
   movement should occur until controls are neutral and deliberately pressed.
4. Disconnect/reconnect while held. Check neutral gating and fresh input again.
5. XR: check recenter/view, headset focus versus desktop focus, and close/relaunch.
6. Play a sustained combat session, then verify settings persist after relaunch.

Automated coverage includes real SDL virtual-device mapping, XR action/session
tests and worker mailbox delivery/stale release. It cannot establish physical
mapping, latency, runtime-reserved buttons or comfort.
