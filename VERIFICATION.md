# Verification — v0.2.0, 2026-10-29

- Ubuntu 24.04 x86-64, MuseScore Studio 4.7.5 official AppImage.
- 53 native processor checks pass (`ctest` / `ScoreBackingTests`), including:
  - practice speed: a tone starting at 0.5 s in the recording starts at
    0.990 s in the score at 50% (expected 1.0) and at 0.242 s at 200%
    (expected 0.25), also after a seek; its pitch measures 440 Hz at 50% and
    442 Hz at 200% (440 Hz source, ±1 zero crossing of measurement error);
  - 5 ms fade-in after starts, seeks and offset changes; no fade while playing
    continuously; blocks larger than announced are split correctly.
- MuseScore export at 100% with every instrument muted: all 705,600 backing
  samples compared. Only the first 218 samples (the 5 ms start fade) differ;
  the rest match the source to a squared error of 8e-15. Overall RMS error
  3.4e-4 against a source RMS of 0.094.
- Removing the effect yields a silent reference.
- Live GUI at 50%: with MuseScore's speed at 0.5 (♩ = 60 instead of 120) and the
  plugin at 50%, MuseScore shows 0:00:09.3 at bar 3 beat 2, and the plugin
  reports score 9.300 s and recording 4.650 s, which is bar 3 beat 2 of the
  120 BPM recording. Seeking keeps the same relation (9.184 s and 4.592 s).

Reproduce with `native/scripts/verify_native.py` (see native/README.md); add
`--gui --speed 50` for the speed check. It writes logs, renders and
screenshots to the git-ignored `artifacts/` directory.

Not yet verified: Windows and macOS in MuseScore, the audible quality of
stretched playback on real recordings (checked by measurement only), acoustic
and device latency, MP3/FLAC decoding in the host, complex navigation and
count-ins.
