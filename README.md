<div align="center">

<img src="docs/logo.svg" width="112" alt="Score Backing Track logo">

# Score Backing Track

**Play the recording you're transcribing in sync with MuseScore Studio's playback, at any practice speed.**

[![Build](https://github.com/AlltAWD/score-backing-track/actions/workflows/build.yml/badge.svg)](https://github.com/AlltAWD/score-backing-track/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/AlltAWD/score-backing-track?include_prereleases&label=release)](https://github.com/AlltAWD/score-backing-track/releases)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue)](LICENSE)
[![Platforms](https://img.shields.io/badge/platforms-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)](#install)
[![Format: VST3](https://img.shields.io/badge/format-VST3-8a2be2)](https://steinbergmedia.github.io/vst3_dev_portal/)
[![MuseScore Studio 4](https://img.shields.io/badge/MuseScore%20Studio-4.x-1f6feb)](https://musescore.org)

[Install](#install) · [Usage](#usage) · [Limitations](#limitations) · [Building](#building-from-source) · [Roadmap](#roadmap) · [Contributing](#contributing)

<img src="docs/editor.png" width="620" alt="The Score Backing Track editor open inside MuseScore Studio">

</div>

> [!IMPORTANT]
> **Set the plugin's Playback speed to the same value as MuseScore's.**
> MuseScore doesn't tell plugins about its speed setting, so when you slow
> MuseScore down to 50%, set the plugin to 50% too. The recording then slows
> down with the score and keeps its pitch.
>
> Only the **Linux** build has been tested in MuseScore by a person so far.
> The Windows and macOS builds are compiled and unit-tested automatically.

## Why

When you transcribe, you're constantly comparing what you've written with the
original, often slowed down to catch fast passages. MuseScore can play your
score at any speed, but it can't play the original alongside it. Score Backing
Track can: load the recording once, line it up, and every time you press Play
(or play from a selected bar) the recording starts from the matching point, at
the same speed as your score.

## Features

- 🎧 **Follows MuseScore's playhead.** Play, pause, play from a selection, and
  seeking all stay in sync, with no clicking "start" in a second app.
- 🐢 **Practice speed, same pitch.** Slow down (or speed up) from 10% to 300%
  together with MuseScore. The recording is time-stretched without changing
  its pitch.
- 🎚️ **Mix or compare.** Hear the recording alongside your score, or mute your
  own part in the Mixer to check it against the original by ear.
- ⏱️ **Fine alignment.** A recording offset you can type in or nudge by
  ±0.01 s / ±0.1 s.
- 🤫 **No clicks.** A 5 ms fade-in whenever playback jumps.
- 💾 **Saved with the score.** The recording's location, offset, speed and
  volume are stored in the `.mscz`. The audio stays an external file.
- 📂 **Common formats.** WAV, MP3, FLAC, OGG and AIFF, at any sample rate.
- 🧩 **No extra software.** A VST3 effect that runs inside MuseScore's own
  audio engine. There's no companion app, QML plugin or background service.

## Install

| Platform | Status | VST3 folder |
| --- | --- | --- |
| **Linux** (x86-64) | ✅ Tested on Ubuntu 24.04 with MuseScore Studio 4.7.5 | `~/.vst3/` |
| **Windows** (x64) | ⚠️ Built and unit-tested by CI, not yet tested by a person in MuseScore | `C:\Program Files\Common Files\VST3\` |
| **macOS** 11+ (Apple Silicon & Intel) | ⚠️ Built and unit-tested by CI, not yet tested by a person in MuseScore | `~/Library/Audio/Plug-Ins/VST3/` |

1. Download the zip for your platform from the
   [latest release](https://github.com/AlltAWD/score-backing-track/releases/latest).
2. Extract it. You get a folder named `Score Backing Track.vst3`. Copy the
   **whole folder** (not just the file inside it) into the VST3 folder above.
3. Restart MuseScore Studio.

> [!NOTE]
> **macOS:** the plugin isn't notarized by Apple, so macOS blocks it the first
> time. After copying it, run this once in Terminal, then restart MuseScore:
>
> ```bash
> xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Score Backing Track.vst3"
> ```

## Usage

1. Open the Mixer (**F10**).
2. In the **Master** channel's audio-effects slot, add
   **AlltAWD → Score Backing Track**.
3. In the effect's window, click **Choose recording...**.
4. Press Play. The recording follows MuseScore's playhead.
5. Align the recording with **Recording offset**:
   - a **positive** offset skips the recording's intro;
   - a **negative** offset delays the recording.

   Type a value, or use the ±0.01 s / ±0.1 s buttons. Double-click a slider to
   reset it. Align at 100% speed; the offset stays correct at other speeds.
6. To practice slower, change MuseScore's playback speed and set the plugin's
   **Playback speed** to the same value (type it, or use the 50% / 75% / 100%
   buttons).

> [!TIP]
> To hear only the recording, mute the score's instruments in the Mixer. Keep
> **Master** unmuted, because that's where the recording plays.

Save the score to keep the recording's location and settings. If you move the
audio file later, the editor shows which file is missing so you can choose it
again.

## Limitations

- **Speed must be set in both places.** If MuseScore and the plugin are set to
  different speeds, the recording drifts away from the score.
- **Very slow speeds sound smeared.** The stretching library sounds best
  between about 75% and 150%. Slower speeds are still useful for picking out
  notes, but attacks blur. In tests, note onsets stay within about 10 ms of
  the score.
- **Turn count-in off.** It shifts the score relative to the recording.
- **The score's timing has to match the recording.** There's no beat mapping:
  tempo changes, repeats, jumps and fermatas in the score that the recording
  doesn't have (or vice versa) will drift.
- **Audio-device latency hasn't been measured.** If the recording sounds
  slightly early or late against the score, nudge the offset.

## How it works

The plugin decodes the recording on a background thread. In MuseScore's audio
callback it reads the host's playhead position and mixes in the matching slice
of the recording: at 100% directly, sample for sample; at other speeds through
[Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch),
re-aligned on every jump. MuseScore implements its speed setting by stretching
the score's timeline, so the recording position is simply *speed × playhead
time + offset*. The audio thread never waits on a lock and never allocates or
frees memory. More detail is in [native/README.md](native/README.md).

## Building from source

Requires CMake 3.24+ and a C++17 compiler. On Linux you also need JUCE's
development packages (ALSA, freetype/fontconfig, X11; see
[the CI workflow](.github/workflows/build.yml) for the exact list).

```bash
cmake -S native -B native/build -DCMAKE_BUILD_TYPE=Release
cmake --build native/build --config Release
ctest --test-dir native/build -C Release   # use xvfb-run on a headless Linux machine
python3 native/scripts/install_native.py   # copies the bundle to your VST3 folder
```

CMake downloads [JUCE 7.0.12](https://github.com/juce-framework/JUCE/tree/7.0.12)
and [Signalsmith Stretch 1.4.0](https://github.com/Signalsmith-Audio/signalsmith-stretch)
automatically. End-to-end checks against a real MuseScore install are
described in [native/README.md](native/README.md#verification) and
[VERIFICATION.md](VERIFICATION.md). `demo/` contains an eight-bar score and a
matching recording for trying it out.

## Roadmap

- [x] Practice-speed support (pitch-preserving time-stretch), v0.2
- [x] macOS build, v0.2
- [ ] Windows and macOS testing in MuseScore
- [ ] Waveform view for easier alignment
- [ ] Better handling of repeats and jumps

## Contributing

Bug reports, test results (especially on Windows and macOS) and pull requests
are welcome. Please [open an issue](https://github.com/AlltAWD/score-backing-track/issues)
and include your OS, MuseScore version, and the audio format you used.

## License

Score Backing Track is licensed under the [GNU GPL v3.0](LICENSE). It's built
with [JUCE](https://juce.com) 7 under JUCE's GPLv3 open-source license and
uses [Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch)
(MIT).

VST is a registered trademark of Steinberg Media Technologies GmbH. MuseScore
is a trademark of Muse Group. This project is independent and not affiliated
with or endorsed by either.
