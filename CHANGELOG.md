# LISAccess changelog

## 2026-10-14 — Scripted dialogue choices

- Added reading and numbered keyboard selection for scripted decisions, including
  the principal's question about Nathan. These use the same repeat/help controls
  and device-specific announcements as ordinary conversations.
- Choices remain available when the game normally freezes its timeline for such
  a decision. Active rewinding, fast-forwarding and menu overlays still prevent
  the mod from selecting an answer.

## 2026-10-14 — Expanded scanner

- The scanner now lists active, eligible interactions across the loaded areas,
  nearest first, instead of dropping distant objects or stopping at 32 entries.
  Adjacent rooms can appear; this is not a room filter. Normal interaction range
  and story requirements are unchanged.
- Objects with matching names receive letters, such as Photo, A and Photo, B.
  Moving closer changes their order, not their letters. The selected object
  remains selected as the list reorders.
- Scanner readouts finish with their position in the list, such as “2 of 71,”
  after action information, direction, distance and elevation. Focus and camera
  announcements use the same disambiguated names.

## 2026-10-13 — Dialogue choices

- Added automatic reading of the answers actually offered, with keyboard numbers
  or controller face buttons according to the active input mode, rather than both.
  Repeat and help use the appropriate device's commands. Empty slots are not renumbered.
- Number-row 1–4 select the corresponding offered answer through the game's own
  dialogue controls. Choices are checked again before activation; hidden or
  changed answers and stale requests are refused.
- Ctrl+Home or left-stick click plus D-pad right repeats the current choices.
  F1 or left-stick click plus D-pad up reads dialogue help. R remains rewind.
- Timed choices are identified, but their deadlines are not paused or extended.
- Obsolete choice readouts stop when choices close, provided no newer mod
  announcement has taken over.

The classroom question supplied the read-only fixture, and the initial dialogue
build passed its playtest. Later questions, timed choices and rewind transitions
have not been comprehensively verified.
This does not add spoken subtitles or repeat the characters' voiced dialogue.

## 2026-10-13 — Tutorials and startup messages

- Added narration for tutorial/HUD text announcements and composed startup
  slides/popups, including the autosave explanation and animated tutorial captions.
  Timed startup readouts queue instead of cutting each other short.
  A message is read when it appears or changes, not continuously while it stays
  on screen.
- The narrated photo, selfie and general interaction tutorials explain scanner
  selection, facing and announced action controls instead of mouse dragging.
  Other text and the original visual tutorials are left unchanged.
  Other captions in the same panel remain intact.
- Ctrl+Home repeats the latest gameplay prompt, including after it disappears.
  Holding the left-stick click and pressing D-pad right does the same. This
  replaces that chord's duplicate sound-preview binding; left-stick + D-pad left
  still previews sounds. R remains rewind in gameplay and reread in menus.

Startup narration and the photo and selfie tutorials have been playtested
successfully. This does not add dialogue choices, subtitles, video/image text or
a saving-status announcement.

## 2026-10-13 — Prism speech and braille

- Mod announcements now use Prism, including braille through supported screen
  readers, rather than the speech-only NVDA client. Windows speech fallback is
  retained, and reader availability is rechecked for each announcement.
- Braille failures do not repeat already-spoken announcements. Output errors
  and provider changes are recorded in the mod log.
- The Windows speech fallback continues to treat announcements as ordinary text,
  including labels that begin with a less-than sign.
- Installation no longer requires a separate NVDA controller DLL. Upgrades
  retire only an unchanged, mod-owned old client and preserve other copies.
  Third-party notices are installed alongside the mod's settings and sounds.

The output migration does not add dialogue-choice accessibility or change game
controls. The installed migration has been playtested successfully; this does not
establish compatibility with every Prism provider or braille-display setup.

## 2026-10-12 — Interaction scanner and controls (experimental)

- Holding a target-cycle key now repeats without repeated taps.
- Added nearby object and character interactions to the scanner, using the game's
  labels and eligibility rules. Focus changes announce the available actions.
  Unavailable interaction data is reported separately from an empty target list.
- Added number-row 1–4 action shortcuts without mouse dragging, and explicit
  F camera facing. Scanner browsing alone never moves the camera or activates
  anything.
- Added gameplay controller commands for scanning, describing, facing, beacons,
  obstacle cues and previews. Native face-button actions and menus are retained.

The corrected scanner and interactions were playtested through the classroom
selfie. Target announcements while aiming with the right stick were also
confirmed. The design remains experimental; the successful classroom test does
not establish every interaction or camera mode. Dialogue-choice accessibility
is the next gap. The accepted menu narration is separate from these experiments.

## 2026-10-12 — Beacon shortcut correction

- Moved the beacon toggle from End to Shift+Home so it no longer opens the diary.
  Plain Home still describes the target; changing modifiers while holding Home
  does not trigger another command.

## 2026-10-12 — Opening navigation (experimental)

- Added optional directional obstacle cues, a possible-drop warning, and a
  selectable destination beacon. The initial lighthouse-approach destination has
  been used successfully to finish the opening and reach the next scene.
- F2 toggles obstacle cues; Page Up/Down select a destination, Home describes it,
  and Shift+Home toggles its beacon. F1 reads navigation help and F3 previews sounds.
- Added replaceable ElevenLabs placeholder sounds and independent cue-volume
  settings. Custom settings and replacement sounds survive updates and removal.
- Added configurable actor-based landmarks without requiring a rebuild.

The navigation design and sounds remain experimental and may change after
feedback; the accepted menu narration is separate. Beacons give direction, not
walkable routes. This initial navigation milestone listed configured landmarks;
the interaction scanner above extends that coverage. Cue balance at normal game
volume still needs testing. General gameplay accessibility is incomplete.

## 2026-10-12 — Initial menu prototype

- Added speech for the title screen, selected save slot and main-menu buttons in
  the original Life Is Strange. Labels follow the game's actual selection rather
  than an assumed sequence of keypresses.
- Save-slot readouts include the displayed slot name, episode/checkpoint and date
  where available. Menu readouts omit positional counts.
- Added R to repeat readable menu focus and F1 for prototype help. Returning to
  the game repeats the current menu readout.
- Added handling for the common confirmation-message widget. Full confirmation
  coverage and settings values have not yet been verified.
- Added NVDA output with Windows SAPI fallback and a removable installation that
  checks the supported game build before using native offsets.

This initial milestone covered menus only. The project remains an early-development
mod, not a public release. See the
[mod guide](README.md) for controls, limitations and removal instructions.
