# Muse Backing Track

A native VST3 recording player for MuseScore Studio. It runs entirely inside
MuseScore's Mixer. No companion application or QML plugin is required.

The Linux VST3 is installed at `~/.vst3/Muse Backing Track.vst3`.

1. Restart MuseScore and open Mixer with F10.
2. Add **MuseBacking → Muse Backing Track** to the **Master** channel's effects.
3. Open its editor and choose a recording.
4. Adjust the recording offset and volume, then use MuseScore's playback controls.

Use 100% speed with count-in disabled. Save the score to preserve the recording
path and settings. Recordings remain external files.

See [native/README.md](native/README.md) for installation, building, usage, and
current limitations. Windows remains unbuilt and untested.

The original eight-bar score and matching WAV are in `demo/`. Regenerate them
with `python3 scripts/make_demo.py`.

## Development

The native plugin uses CMake and JUCE 7.0.12. Python is used only for developer
scripts; optional GUI verification uses python-xlib, ffmpeg, and Xvfb.
Run `uv sync` to create the small development environment.

See [VERIFICATION.md](VERIFICATION.md) for completed checks and
[HANDOFF.md](HANDOFF.md) for implementation context.

The old companion application, QML plugin, launcher, and their sources have been
removed. The native source is GPL-3.0-only; see LICENSE and native/README.md.
