# LISAccess — accessibility prototype

This is a private prototype for **original Life Is Strange on Windows
(Steam build 2304417)**. It is not for the remaster or Before the Storm.

The title, save-slot picker and main-menu narration are the accepted baseline.
An **experimental opening-navigation layer** has been used successfully to reach
the lighthouse and enter the next scene. Cue balance at normal game volume still
needs listening feedback. This is not complete gameplay accessibility, and silence
never means that an area is safe.

## What it does

- Announces that the prototype loaded, using NVDA when available, with Windows
  SAPI as a fallback.
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
| R | Repeat the last current menu readout while the prototype has readable menu focus |
| F1 | Read help for readable menu focus, or navigation help during supported gameplay |
| F3 | Preview the next navigation sound in readable menus or supported gameplay (experimental) |

R remains the game's rewind key outside menu narration. Navigation shortcuts
ignore Ctrl and Alt; Shift+Home is the only shifted shortcut. Native movement
bindings are not changed.

## Opening navigation (experimental)

> This is a first navigation design to try, not a settled interface. The cues and
> controls may change or be replaced after listening and walking tests. Feedback
> on whether they are useful matters as much as whether they fire correctly. The
> accepted menu narration is separate from this experimental navigation layer.

Use the game's movement controls and mouse to turn the camera. There is no
auto-walk, automatic turning, route planner or automatic interaction. Audio
direction is relative to the camera, not Max's body orientation.

| Key | Navigation action (experimental) |
|---|---|
| F2 | Toggle obstacle proximity cues; initially off |
| Page Down / Page Up | Select next / previous supported destination |
| Home | Describe the selected destination and its current direction |
| Shift+Home | Toggle the selected destination's directional beacon |
| F1 | Read navigation help |
| F3 | Preview the next sound: obstacle, person, object, destination |

End remains the game's alternate diary key; toggling a beacon no longer opens
the diary. Controller navigation commands are not implemented yet.

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
beacon with Shift+Home. If navigation reports that its readout is unavailable or
incomplete, do not treat silence as an all-clear.

**NPC and item detection are not enabled yet.** Their F3 sounds are placeholders
for later work, not evidence that a person or object is nearby. Only explicitly
configured destinations are currently listed; this is not a complete world scanner.

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

The development installation adds `dinput8.dll`, `nvdaControllerClient32.dll`
and the `LISAccess` settings/audio directory beside the game executable. It does not patch the
executable, cooked assets, Steam app IDs or saves. Normal game actions can still
create/update saves and settings as usual.

The installer refuses an unsupported executable or an unrelated/changed DLL. Its
ownership record is `%LOCALAPPDATA%\LISAccess\install.json`. Close the game before
installing, updating or removing the prototype.

To remove only the recorded, unchanged prototype DLLs, run from this repository:

```powershell
powershell -NoProfile -File scripts/remove-prototype.ps1
```

If removal reports a changed DLL, investigate rather than deleting it blindly.
Updates and removal preserve customized settings and replacement sounds; unchanged
owned files are removable. Logs and game saves are not removed. Empty data folders
may remain after removal.

## Development and live inspection

Build with Visual Studio 2022 C++ Build Tools and CMake:

```powershell
powershell -NoProfile -File scripts/build.ps1
ctest --test-dir build-x86 -C Release --output-on-failure
```

The native bridge runs read-only GFx queries on the game's window/game thread.
`tools/probe_runtime.py` requests a number, string or a known menu object's
`GetText` result through local request/response files. It does not provide arbitrary
native calls, script evaluation or action activation. `tools/live_inspect.py` and
`tools/live_objects.py` provide external, read-only registry/object snapshots.

Navigation samples game collision on the same thread. Indexed-native Trace needs
a narrowly scoped, immediately restored dispatch-metadata adapter; it does not
activate actors or change world/save state. Both event dispatchers require three
stack arguments, including the legacy result slot. Live scene validation remains
separate from the engine-free geometry, native-ABI and installer tests.

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
