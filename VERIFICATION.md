# Native VST3 verification — 2026-10-26

- Ubuntu 24.04 x86-64, MuseScore Studio 4.7.5 official AppImage.
- 31 native processor checks pass.
- MuseScore exported all 705,600 backing samples with every instrument muted.
  RMS error versus the source: 1.066e-10.
- Removing the effect yields a silent reference (peak below 5e-9). Both export
  processes exit successfully.
- The native editor opens inside MuseScore and its position display follows
  play, pause, resume, and seeking.
- No companion app or local HTTP service is used by these native checks.

See artifacts/native-verification/verification.json and artifacts/native-gui
for evidence. Test sources are in native/tests and native/scripts.

Not yet verified: Windows, acoustic/device latency, MP3/FLAC decoding, complex
navigation, count-ins, and practice-speed changes. Use 100% speed, count-in off,
and a recording matching the score's timing.
