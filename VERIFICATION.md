# Verification — 2026-10-26

- Ubuntu 24.04 x86-64, MuseScore Studio 4.7.5 official AppImage.
- 37 native processor checks pass (`ctest` / `ScoreBackingTests`).
- MuseScore exported all 705,600 backing samples of the demo with every
  instrument muted; RMS error versus the source: 1.07e-10.
- Removing the effect yields a silent reference (peak 4.8e-9). Both export
  processes exit successfully.
- The native editor opens inside MuseScore and its position display follows
  play-from-selection, pause, resume, and seeking.

Reproduce with `native/scripts/verify_native.py` (see native/README.md); it
writes logs, renders and screenshots to the git-ignored `artifacts/` directory.

Not yet verified: Windows in MuseScore, acoustic/device latency, MP3/FLAC
decoding in the host, complex navigation, count-ins, and practice-speed
changes. Use 100% speed, count-in off, and a recording matching the score's
timing.
