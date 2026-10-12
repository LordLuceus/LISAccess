# LISAccess changelog

## 2026-10-12 — Opening navigation (experimental)

- Added optional directional obstacle cues, a possible-drop warning, and a
  selectable destination beacon. The initial lighthouse-approach destination has
  been used successfully to finish the opening and reach the next scene.
- F2 toggles obstacle cues; Page Up/Down select a destination, Home describes it,
  and End toggles its beacon. F1 reads navigation help and F3 previews sounds.
- Added replaceable ElevenLabs placeholder sounds and independent cue-volume
  settings. Custom settings and replacement sounds survive updates and removal.
- Added configurable actor-based landmarks without requiring a rebuild.

The navigation design and sounds remain experimental and may change after
feedback; the accepted menu narration is separate. Beacons give direction, not
walkable routes. NPC/item detection is not yet enabled, and cue balance at normal
game volume still needs testing. General gameplay accessibility is incomplete.

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

This initial milestone covered menus only. The project remains a private
prototype, not a public release. See the
[prototype guide](README.md) for controls, limitations and removal instructions.
