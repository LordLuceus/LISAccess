# LISAccess — experimental menu prototype

This is a private prototype for **original Life Is Strange on Windows
(Steam build 2304417)**. It is not for the remaster or Before the Storm.

**Opening navigation and gameplay accessibility are not implemented.** This build
is intended to test reaching a new game through the title screen, save-slot picker
and main menu. Do not interpret silence during gameplay as an empty or safe area.

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
| F1 | Read prototype help while it has readable menu focus |

There is no navigation assistance after New Game begins. R is not assigned a new
gameplay action by this mod; outside its menu context it remains the game's key.

## Reversible installation

The development installation adds only `dinput8.dll` and
`nvdaControllerClient32.dll` beside the game executable. It does not patch the
executable, cooked assets, Steam app IDs or saves. Normal game actions can still
create/update saves and settings as usual.

The installer refuses an unsupported executable or an unrelated/changed DLL. Its
ownership record is `%LOCALAPPDATA%\LISAccess\install.json`. Close the game before
installing, updating or removing the prototype.

To remove only the recorded, unchanged prototype DLLs, run from this repository:

```powershell
powershell -NoProfile -File scripts/remove-prototype.ps1
```

If removal reports a changed file, investigate rather than deleting it blindly.
Logs and game saves are not removed.

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
