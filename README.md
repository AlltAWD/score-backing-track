# Score Backing Track

Play a local recording in sync with MuseScore Studio's score playback.

Built for transcribing: load the recording you're transcribing, line it up with
your score once, and every time you press Play (or click a bar and play from
there) MuseScore plays the recording from the matching point, mixed with, or
instead of, your score. Mute your own part to compare it against the original
by ear.

![The plugin's editor inside MuseScore](docs/editor.png)

It is a VST3 effect that runs inside MuseScore's own mixer. No companion app,
QML plugin, or background service is needed.

## Install

1. Download the zip for your platform from the
   [Releases](../../releases) page and extract it. You get a folder named
   `Score Backing Track.vst3`. Keep the whole folder.
2. Put that folder in your VST3 directory:
   - **Linux:** `~/.vst3/`
   - **Windows:** `C:\Program Files\Common Files\VST3\`
3. Restart MuseScore Studio.

The Linux build is tested on Ubuntu 24.04 with MuseScore Studio 4.7.5.
**The Windows build is compiled by CI but has not been tested by a person yet.**
Reports are welcome.

## Use

1. Open the Mixer (**F10**).
2. In the **Master** channel's audio-effects slot, add
   **AlltAWD → Score Backing Track**.
3. In the effect's window, click **Choose recording...** (WAV, MP3, FLAC, OGG
   or AIFF).
4. Press Play. The recording follows MuseScore's playhead, including
   play-from-selection, pause and seeking.
5. Align the recording with **Recording offset**. A positive offset skips the
   recording's intro, and a negative offset delays it. Type a value, or use the
   ±0.01 s / ±0.1 s buttons. Double-click a slider to reset it.

Save the score to keep the recording's location, offset and volume. The audio
is not embedded in the score. If you move the file, the editor shows which
file is missing so you can choose its new location.

Tip: to hear only the recording, mute the score's instruments in the Mixer. Keep
**Master** unmuted, because that's where the recording plays.

## Limitations

This is an early release.

- **Playback speed must be 100%.** MuseScore doesn't tell effects about its
  speed setting, so at other speeds the recording drifts away from the score.
- **Count-in should be off.**
- The recording must follow the score's timing. There is no beat mapping, so
  the score's tempo has to match the recording (or you accept drift).
  Repeats, jumps and fermatas that the recording doesn't also contain will
  drift too.
- Live audio-device latency hasn't been measured. If the recording sounds
  slightly early or late against the score, nudge the offset.

## Build from source

See [native/README.md](native/README.md). In short:

```bash
cmake -S native -B native/build -DCMAKE_BUILD_TYPE=Release
cmake --build native/build --config Release
```

CMake downloads JUCE 7.0.12. Tests and verification against a real MuseScore
install are described in [native/README.md](native/README.md) and
[VERIFICATION.md](VERIFICATION.md).

`demo/` contains an eight-bar score and a matching recording for trying it out.
Regenerate them with `python3 scripts/make_demo.py`.

## License

GPL-3.0-only (see [LICENSE](LICENSE)). Built with
[JUCE](https://juce.com) 7 under its GPLv3 open-source license. VST is a
trademark of Steinberg Media Technologies GmbH. This project is not affiliated
with MuseScore or Muse Group.
