# LISAccess changelog

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

This is an experimental, private prototype, not a public release. Opening
navigation and gameplay accessibility are not implemented. See the
[prototype guide](README.md) for controls, limitations and removal instructions.
