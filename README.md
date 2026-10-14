# LISAccess — accessibility mod

This is an early-development mod for **original Life Is Strange on Windows
(Steam build 2304417)**. It is not for the remaster or Before the Storm.

The title, save-slot picker and main-menu narration are the accepted baseline.
An **experimental opening-navigation layer** has been used successfully to reach
the lighthouse and enter the next scene. Cue balance at normal game volume still
needs listening feedback. The corrected scanner and interactions have been
playtested through the classroom selfie, including automatic target announcements
while aiming with the right stick. Dialogue-choice narration and selection have
also passed their initial playtest.
This is not complete gameplay accessibility, and silence never means that an area
is safe.

## Licence and sound assets

LISAccess's original code and documentation are licensed under the
[MIT licence](LICENSE). This does **not** license the ElevenLabs-generated sounds
or third-party components; see the [third-party and audio notices](THIRD-PARTY-LICENSES.md).

The public source repository contains no audio binaries. Configuration,
provenance and processing scripts remain available. Processed sound effects are
distributed only as functional cues bundled with complete mod release archives,
not as a standalone sound pack. Raw provider PCM files remain private and are
never included in releases. Release archives must include the MIT licence,
third-party notices and audio restrictions; the MIT licence grants no independent
reuse rights to the sounds.

## What it does

- Announces that the mod loaded through Prism, preferring an available screen
  reader and retaining Windows SAPI as a system-speech fallback.
- Sends mod announcements to the selected reader's braille channel as well as
  speech, when that reader supports braille.
- Reads visible tutorial prompts, HUD notices and composed startup popups such
  as the autosave explanation. The narrated photo, selfie and general interaction
  tutorials describe the mod's scanner/action controls instead of mouse dragging.
- Announces the currently offered dialogue choices and their keyboard/controller
  buttons. It never selects an answer automatically.
- Reads the game's actual focused title/menu button, rather than estimating
  selection from keypresses.
- Describes the selected save slot using its displayed name, episode/checkpoint
  and date where present. Menu and slot readouts omit positional counts.
- Reads the common confirmation-message widget and its confirm/cancel/continue
  controls. It does not invent a highlighted Yes/No choice where the game has none.
- Repeats the current menu readout when returning focus to the game.

This is not complete menu coverage: settings values, account/login pages, all
confirmation variants and gameplay UI have not been fully tested. Unrecognized or
unavailable text is not replaced with guessed menu labels. New helper messages are
currently English; ordinary labels come from the game's displayed text.

## Speech and braille

Prism chooses an available output provider automatically, using its backend
priorities. NVDA takes priority over JAWS; Windows SAPI remains the preferred
system-speech provider when priorities are equal. Other Prism providers can also
be selected when available, but have not all been tested with this mod. Their
vendor software must be installed separately.

All existing mod announcements go to both speech and the selected reader's
braille channel when supported. Configure your braille display through your
screen reader as usual; the mod does not connect to displays directly. SAPI is
speech-only. A braille-output failure does not cause successful speech to repeat.

Availability is checked again for each announcement, so stopping or restarting
a reader can switch output without restarting the game. The existing interrupt
and queue requests are retained. The mod log records provider changes and output
failures, not the contents of announcements. The migrated output has been
playtested successfully; other reader and braille-display combinations may still
need testing.

## Prompts and startup notices

Tutorial prompts and text announcements are read as they appear, including text
captions on animated tutorial panels. Timed startup slides and popups use their
actual composed text rather than a fixed English substitute. Startup readouts
queue so the next slide or automatic title message does not cut them short;
explicit reread and subsequent menu navigation still interrupt. During
gameplay, **Ctrl+Home** repeats the latest captured prompt, even after it fades.
During dialogue it repeats the currently available choices instead.
On a controller, hold the left-stick click and press **D-pad right**. These repeat
commands do not select or dismiss a prompt. **R remains rewind in gameplay**;
in readable menus and startup popups, R repeats the current readout as before.

The photo, selfie and general interaction tutorials have adapted speech/braille
instructions for selecting a scanner target, facing it and choosing the announced
action. Their original visual text is not modified. Other messages retain the
game's displayed text and language; the adapted helper instructions are currently
English. Automatic prompts queue after other announcements; deliberate navigation
or repeat commands can interrupt them.

This reader covers the inspected tooltip, quote, HUD-prompt, notification and
animated-tutorial captions, plus composed menu popups and startup slides—not text
baked into videos or images. This prompt reader does not add subtitles,
nor announcements for the autosave icon itself. Startup narration and the photo
and selfie tutorials have been playtested successfully.

## Dialogue choices

When choices appear or change, their labels are read with keyboard numbers or
controller face buttons, according to the game's active input mode—the same
detection used for object actions. Repeat and help follow the device you use;
you do not need to hear both sets of commands. The mapping stays **1 = Y, 2 = X,
3 = B, 4 = A**. Missing slots are not renumbered, so a two-answer question may
offer 2 and 3 rather than 1 and 2. Controller face buttons retain their native
game behaviour; numbered keyboard choices use the same native dialogue handler.

This also covers scripted decisions using the game's dialogue-choice interface,
including the principal's question about Nathan. They use the same controls as
ordinary conversations, without a separate mode to enable.

- **1–4, number row:** select the corresponding offered answer.
- **Ctrl+Home**, or **hold left-stick click and press D-pad right:** repeat the
  current choices without selecting anything.
- **F1**, or **hold left-stick click and press D-pad up** while choices are
  available: read dialogue help.
- **R:** remains the game's rewind control, not dialogue repeat.

Choice requests are checked again before activation. Hidden, unavailable or
changed choices are not selected, and queued requests expire rather than firing
after a long stall. Game pauses, menu overlays and active rewind/fast-forward
block the mod's choice input. A scripted decision's normal frozen timeline does
not block its available answers. The mod does not choose for you, advance the conversation
on its own, pause a choice timer or extend its deadline. Timed choices are
identified as timed; long speech may still outlast the game's deadline.
When choices close, their readout is cancelled if it is still the mod's latest
announcement, without cancelling a newer menu or prompt readout.

The implementation is based on passive captures of the classroom question and
the principal's decision, plus the game's own widget and input code. Both ordinary
dialogue and the principal's scripted decision have passed their playtests.
This does not establish coverage of every later question, timed
choice or rewind transition. This does not add spoken subtitles or
repeat the characters' voiced dialogue.

## Starting and controls

**Launch from the Steam library**, not by opening `LifeIsStrange.exe` directly.
The executable's direct-launch Steam fallback requests a separate demo app.

After the startup announcement, use the game's controls. At the title screen,
follow its spoken “Press Any Button” prompt. Choose the save slot you intend to
use; existing saves are not disposable test data.

| Key | Context and action |
|---|---|
| Arrow keys | The game's menu navigation |
| Enter | The game's accept/select action |
| Escape | The game's back/cancel action |
| R | Repeat the last current menu readout while the mod has readable menu focus |
| F1 | Read help for readable menu focus, dialogue, or navigation during supported gameplay |
| F3 | Preview the next navigation sound in readable menus or supported gameplay (experimental) |

R remains the game's rewind key outside menu narration. Native movement bindings
are not changed. Scanner shortcuts ignore Ctrl and Alt; Shift+Home is the one
shifted shortcut. Controller menus keep their native controls. Controller scanner
commands, including sound preview, run in gameplay only; keyboard F3 also works
in readable menus.

## Navigation and interactions (experimental)

> This is a first navigation design to try, not a settled interface. The cues and
> controls may change or be replaced after listening and walking tests. Feedback
> on whether they are useful matters as much as whether they fire correctly. The
> accepted menu narration is separate from this experimental navigation layer.

Use the game's movement controls, mouse or right stick normally. There is no
auto-walk, route planner or automatic interaction. Camera facing is an explicit
command, not a side effect of browsing the scanner. Audio direction is relative
to the camera, not Max's body orientation.

| Key | Navigation action (experimental) |
|---|---|
| F2 | Toggle obstacle proximity cues; initially off |
| Page Down / Page Up | Select next / previous scanner target; hold to repeat |
| Home | Describe the selected target; with no selection, repeat focused-object actions |
| Shift+Home | Toggle the selected target's directional beacon |
| Ctrl+Home | Repeat the latest tutorial/HUD prompt, including after it fades |
| F | Request camera facing toward the selected target |
| Number row 1, 2, 3, 4 | Use the focused object's announced action; equivalents of controller Y, X, B, A |
| F1 | Read navigation help |
| F3 | Preview the next sound: obstacle, person, object, destination |

End is left entirely to the game's diary binding. Action keys never auto-repeat.

### Controller controls (experimental)

These require an XInput-compatible controller, including one presented as XInput
by Steam Input. Button names use Xbox terminology. The mappings still need
in-game conflict testing. Scanner controls stand down while the native debug
menu is active or visible, since it also uses the D-pad.

| Controller input | Gameplay action |
|---|---|
| D-pad left / right | Previous / next scanner target; hold to repeat |
| D-pad up | Describe the selected target or focused-object actions |
| D-pad down | Toggle the selected target's beacon |
| Click the left stick | Face the selected target on button release |
| Hold left-stick click + D-pad down | Toggle obstacle cues |
| Hold left-stick click + D-pad up | Read help |
| Hold left-stick click + D-pad left | Preview the next sound |
| Hold left-stick click + D-pad right | Repeat the latest tutorial/HUD prompt, including after it fades |
| Y, X, B, A | The game's native actions, announced with their button names |

A left-stick chord does not also trigger facing on release. Native face buttons
are not emulated or duplicated, and the mod does not force an input-mode switch.
Prompt repeat remains available when a tutorial pauses normal gameplay; other
scanner and interaction commands keep their existing availability checks.

### Selecting and using an object

Select an object with Page Up/Down or D-pad left/right, then press F or click the
left stick to face it. **Wait for the focused-object announcement and its action
keys/buttons before acting.** Selecting a scanner entry does not itself change
the game's focus. Action shortcuts and native face buttons refer to that focus,
not an unfocused scanner selection. Original mouse interactions still work.

The scanner lists all active, qualifying registered interactions in the loaded
areas, active scripted exit areas and configured landmarks, nearest first. The
previous proximity cutoff
and 32-interaction limit are removed. Distance ordering uses ground distance;
height is described separately. The list can include adjacent rooms: this is not
a current-room filter, a route finder, or a list of every decorative object.

Matching names receive letters, such as **Photo, A** and **Photo, B**. Letters
follow object identity rather than distance, so walking past two photos does not
swap their names. They can change when the duplicate group changes or a scene
reloads; a unique name has no letter. Selection stays on the same object when
distance order changes. Focus announcements and camera-facing feedback use the
same labelled name. Selecting or rereading a scanner entry ends with **N of M**,
after its action information, direction, distance and elevation.

The game's labels and native story, inventory, one-shot and angle checks still
apply. Angle-restricted targets are identified so you can try another approach.
Listing a distant object does **not** extend interaction reach: move close enough
and wait for its real focused-object announcement and available actions. Occluded
objects may still need a different approach.
Some characters can use the generic object sound when their data does not
identify a character name.

Entries announced as **Exit area** are scripted loading-zone triggers, such as
the hallway exit to the main campus. Use the same facing and beacon controls to
approach their area; the beacon uses the destination sound. These entries do not
need an ordinary focused-object announcement. Enter the area and follow the
game's prompt to leave. For the campus exit, this is normally left mouse button
on keyboard/mouse, or A on a controller; follow the prompt if bindings differ.
Numbered object-action shortcuts
do **not** activate exit areas; they still refer to the focused ordinary object.

An exit-area entry is a destination, not a promise that the story currently lets
you leave. The game's requirements, nearby-interaction priority and cinematic or
rewind restrictions still apply. This supports the game's loading-zone trigger
behaviour; it is not a guarantee that every later scene transition uses that
behaviour. The hallway exit to the main campus has passed its in-game playtest.

Facing supports the normal basic camera and clamps requests to its authored
pitch bounds. Fixed, dialogue, derived or multiple-active-behaviour camera modes
are refused; other transitions still need testing.
Collision, camera offsets and native interaction priority can prevent focus.
“Turning toward” acknowledges a request, not successful focus: listen for the
actual object/action announcement, and move closer or aim manually if necessary.
The successful classroom test does not prove every action or camera mode.

### Beacons and obstacles

The initial destination is **Lighthouse approach**, attached to an authored
opening-scene trigger. Its beacon is a bearing to that point, **not a walkable
route**. Turn and choose your way around obstacles yourself. The beacon pans left
or right; a lower pitch means the target is behind the camera. Speech gives broad
distance and elevation descriptions without claiming an unverified metre scale.

Obstacle cues use short, partial-body collision sweeps ahead, ahead-left and
ahead-right. The nearest detected obstruction determines pan and pulse rate;
nearer obstacles pulse faster. A lower-pitched obstacle pulse warns of a possible
drop or missing ground just ahead. These checks cover a small region, not the
whole character or a planned route. Floor contact can hide geometry beyond it;
slopes, moving objects and uneven ground still need testing. They neither prevent
movement nor guarantee that a passage is clear.

Navigation requires a verified controllable player and a positive floor query.
It stops when the game loses focus, controls are unavailable, a menu is active,
or the snapshot becomes stale. After such a transition, re-enable a destination
beacon with Shift+Home or D-pad down. If navigation reports that its readout is unavailable or
incomplete, do not treat silence as an all-clear.

Person and object sounds are now used by their target beacons. F3 is only a
preview, not evidence of a nearby object. Scanner controls remain gated by the
verified player, floor and gameplay state.
If interaction data cannot be read, the scanner reports that it is unavailable
rather than claiming that no targets exist.

### Sound and destination customization

The game directory's `Binaries\Win32\LISAccess\navigation.ini` sets default
obstacle-cue state and `Volume` (0.0 to 1.0). F2 changes the current session only.
Keep `Diagnostics=0` except when collecting a requested development log.

Replace `obstacle.wav`, `person.wav`, `object.wav` or `destination.wav` in
`Binaries\Win32\LISAccess\audio` to try different sounds. Use mono PCM16 WAV,
8–48 kHz, at most two seconds and 256 KiB. Very short sounds work best: the supplied
obstacle cue is 160 ms; busy cues are dropped rather than queued. Restart the game
after editing settings or replacing sounds. The four supplied ElevenLabs sounds
are placeholders for listening feedback, not final sound design.

The INI's `[Landmarks]` section can add or relabel destinations by mapping a
spoken name to an existing actor's full object path. This supports additional
authored landmarks without recompiling; it does not discover useful destinations
automatically. Only matching actors in the active world qualify. Internal paths
belong in this configuration or diagnostic tools, never ordinary speech.

## Reversible installation

The development installation adds `dinput8.dll` and the `LISAccess`
settings/audio/notices directory beside the game executable. Prism is built into
the mod; a separate Prism or NVDA controller DLL is not required. It does not patch the
executable, cooked assets, Steam app IDs or saves. Normal game actions can still
create/update saves and settings as usual.

The installer refuses an unsupported executable or an unrelated/changed DLL. Its
ownership record is `%LOCALAPPDATA%\LISAccess\install.json`. Close the game before
installing, updating or removing the mod.

After building, install with:

```powershell
powershell -NoProfile -File scripts/install-prototype.ps1
```

Upgrading removes the old `nvdaControllerClient32.dll` only when it still matches
the mod's ownership record. A modified or unowned copy is preserved and reported;
the new mod does not use it. The build stages the required
[third-party notices](THIRD-PARTY-LICENSES.md) and MIT licence; installation requires both.

To remove only the recorded, unchanged mod DLLs, run from this repository:

```powershell
powershell -NoProfile -File scripts/remove-prototype.ps1
```

If removal reports a changed DLL, investigate rather than deleting it blindly.
Updates and removal preserve customized settings and replacement sounds; unchanged
owned files are removable. Logs and game saves are not removed. Empty data folders
may remain after removal.

## Development and live inspection

Build with Visual Studio 2022 C++ Build Tools (including ATL), a Windows SDK with
MIDL, and CMake 3.24 or newer. Prism requires the compiler's C++23 support; this
build is verified with MSVC 19.44. The first configure downloads a pinned,
SHA-256-verified Prism source archive and builds it for **Win32**, not x64:

```powershell
powershell -NoProfile -File scripts/build.ps1
ctest --test-dir build-x86 -C Release --output-on-failure
```

Compiling the code and running its tests does not require the private audio.
Installing does: supply licensed `obstacle.wav`, `person.wav`, `object.wav` and
`destination.wav` in `assets/audio/`, or use a complete mod release for installation.
These local files are ignored by Git. Development worktrees can copy the local
audio inputs from the primary checkout using `.worktreeinclude`; a fresh clone
does not acquire them automatically. `scripts/generate-cues.ps1` is an optional
maintainer processing step requiring the private PCM inputs; it does not download
sounds or call the generation service.

For a real 32-bit output-backend readiness check without launching the game:

```powershell
.\build-x86\Release\prism_smoke.exe
```

This probes registered providers and SAPI synthesis into memory without audible
output. Adding `--output` emits one short speech/braille test announcement.
The deterministic CTest routing fixtures do not require a running screen reader.

The [source architecture guide](docs/ARCHITECTURE.md) describes module ownership,
threading and validation boundaries, and the automated checks that keep source
files and dependencies manageable.

Menu inspection runs read-only GFx queries on the game's window/game thread.
`tools/probe_runtime.py` requests a number, string or a known menu object's
`GetText` result through local request/response files. It does not provide arbitrary
native calls, script evaluation or action activation. `tools/live_inspect.py` and
`tools/live_objects.py` provide external, read-only registry/object snapshots.

Navigation samples game collision on the same thread. Indexed-native Trace needs
a narrowly scoped, immediately restored dispatch-metadata adapter; it does not
activate actors or change world/save state. Both event dispatchers require three
stack arguments, including the legacy result slot. Live scene validation remains
separate from engine-free geometry, input-command, native-ABI and installer tests.
Player-requested actions use the native interaction listener and guarded request
path; facing uses a checked camera request. Both run on the game thread, not the
input/speech worker. Inspection request files do not expose these actions.

Diagnostics are written to `%LOCALAPPDATA%\LISAccess\prototype.log`; menu text may
appear there. Treat logs and ignored `artifacts/` as private development data.
Do not redistribute extracted game assets, metadata or scripts.

The runtime checks the executable SHA-256 before using this build's native offsets.
It disables inspection on a mismatch or a caught bridge fault. This is a safety
gate, **not a guarantee that experimental injected code cannot crash the game**.
The DLL is pinned until process exit; hot unloading is not supported.

See the [initial feasibility investigation](FEASIBILITY.md),
[changelog](CHANGELOG.md),
[research-tool notes](tools/README.md) and
[third-party component/distribution limitations](THIRD-PARTY-LICENSES.md).
