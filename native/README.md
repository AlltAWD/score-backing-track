# Score Backing Track — native VST3, version 0.1.0

Play a recording directly inside MuseScore's audio engine. The plugin is an
audio effect loaded through the Mixer and needs nothing else at runtime.

## Use in MuseScore

1. Install the VST3 bundle once, then restart MuseScore so it discovers it.
2. Open the Mixer with **F10**.
3. In the **Master** channel's effects area, add **AlltAWD → Score Backing Track**.
4. Open that effect's editor and click **Choose recording...**.
5. Choose a WAV, MP3, FLAC, OGG, or AIFF recording.
6. Press Play in MuseScore. The recording follows the host's playback position.

Use the **Recording offset** value to align the recording: positive values skip
its intro; negative values delay it. You can type seconds directly into the value
box or use the ±0.01 s / ±0.1 s buttons; double-click a slider to reset it. **Recording volume** and **Mute recording** affect the backing recording.
The score's existing audio passes through the effect.

Mute your own part in the instrument's Mixer channel when practicing. Keep the
Master channel unmuted so the backing recording can play. The effect runs even
when every score instrument is muted. Closing its editor leaves it playing;
disable or remove the effect to stop using it.

Save your score to retain the recording path, offset, volume, and mute setting.
Audio remains an external file; it is not embedded in the score. If the file is
moved, open the effect and choose its new location.

Use **100% playback speed** and disable count-in for this prototype. The recording
must already follow the score's timing. Automatic beat mapping, tempo stretching,
and comprehensive repeat/jump/fermata support are not implemented.

## Linux installation

The bundle belongs in `~/.vst3/Score Backing Track.vst3/`. Keep the whole directory,
including `Contents`; copying only the `.so` is insufficient.

From the project directory after building:

```bash
python3 native/scripts/install_native.py
```

Alternatively, extract the Linux VST3 archive into `~/.vst3/`. If your MuseScore
configuration uses custom VST search directories, include this directory there.

The Linux binary targets x86-64 Ubuntu 24.04 and is tested with MuseScore Studio
4.7.5. It has no Python or other runtime dependency.

## Build

Requires CMake 3.22+, a C++17 compiler, and JUCE's development dependencies.
On Ubuntu these include X11, Freetype/fontconfig, and ALSA development headers.
Web browsing and curl are disabled for the plugin. No standalone app is built.

```bash
cmake -S native -B native/build -DCMAKE_BUILD_TYPE=Release
cmake --build native/build --config Release --target ScoreBacking_VST3 ScoreBackingTests -j 3
```

CMake downloads JUCE 7.0.12 on a fresh build. To use an existing checkout, add
`-DJUCE_PATH=/absolute/path/to/JUCE7` when configuring. The default output is
`native/build/ScoreBacking_artefacts/Release/VST3/Score Backing Track.vst3`.

The same CMake project supports a Windows build with Visual Studio's C++ tools.
GitHub Actions (`.github/workflows/build.yml`) builds and runs the processor
tests on Linux and Windows; Windows host behavior in MuseScore is still
unverified. Pushing a `v*` tag creates a draft release with both zips. Do not
copy the Linux binary to Windows; build its Windows VST3 counterpart.

## Verification

The native processor tests cover actual WAV loading, selected starts, backward
seeks, pause/resume, missing transport information, offsets, resampling, mute,
state restoration, missing files, removal, and cancellation of stale decodes.

With an X server available:

```bash
native/build/ScoreBackingTests_artefacts/Release/ScoreBackingTests
```

On a headless Linux machine, run it under `xvfb-run -a`.

The full host test uses an isolated MuseScore profile, registers the VST3,
renders a score with its piano part muted, and compares every backing sample
against the original recording. A second render with the VST3 removed must be
silent.

```bash
xvfb-run -a .venv/bin/python -E native/scripts/verify_native.py /path/to/MuseScore.AppImage
xvfb-run -a .venv/bin/python -E native/scripts/verify_native.py /path/to/MuseScore.AppImage --gui
```

The GUI test additionally needs `python-xlib` and ffmpeg for input and screenshots.
Its live playback uses a Master volume of -144 dB so it is inaudible. MuseScore
always rescans `~/.vst3`, so the GUI test exercises the **installed** bundle:
run `install_native.py` first. Evidence is written to the git-ignored
`artifacts/` directory.

## Implementation notes

- The plugin uses host sample positions and renders into the host's audio buffer.
- File decoding happens on a worker thread; immutable PCM is swapped under a
  short lock. The audio callback only tries the lock, never waits for decoding,
  and never deletes an old recording.
- The decoded-audio limit is 512 MiB. Mono is duplicated into stereo; stereo
  recordings retain their channels. Sample-rate conversion uses linear
  interpolation. Volume changes have a 20 ms ramp.
- MuseScore 4.7's offline Master mixer can report an unchanged sample position
  with a realtime process flag. While playing, repeated timestamps advance by
  the exact block size; any changed timestamp or pause reanchors to the host.
- Linux compatibility: MuseScore can release VST instances from a static cleanup
  queue very late at process exit. The bundle retains its code mapping until
  exit, and CMake applies a small JUCE patch keeping its shutdown registry
  container alive. Normal registered-object cleanup still occurs. The patch is
  in `cmake/JuceShutdownCompatibility.cmake` and affects only this build's JUCE
  checkout.

This is an early prototype. Live audio-device latency, tempo changes, complex
navigation, and additional hosts need broader testing.

## License

This project's native source is GPL-3.0-only, as in `../LICENSE`. It uses
[JUCE 7.0.12](https://github.com/juce-framework/JUCE/tree/7.0.12) under its open-source
terms. JUCE's license and third-party notices are in the dependency checkout.
The shutdown-registry file patched by CMake retains its original ISC notice.
