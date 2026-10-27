# Native project handoff — 2026-10-26

The current implementation is the native VST3 in `native/`, built with JUCE
7.0.12 and installed at `~/.vst3/Muse Backing Track.vst3`. It is a Master Mixer
effect with recording import, offset, volume, and mute controls. Its shared
CMake project can target Windows, but Windows remains unbuilt and untested.

31 processor checks pass. MuseScore Studio 4.7.5 exports all 705,600 samples of
the generated demo with RMS error about 1.07e-10 against the source, with all
score instruments muted. Removing the effect yields a silent reference. The
native editor opens inside MuseScore and displays positions following live
play, pause, resume, and seeking. Evidence is in artifacts/native-verification
and artifacts/native-gui.

The plugin handles repeated timestamps from MuseScore's offline Master mixer
by counting rendered samples until host time changes again. Linux builds have
contained late-unload/shutdown compatibility fixes; these are documented in
native/README.md and native/cmake/JuceShutdownCompatibility.cmake.

The active dependency checkout is native/.deps/JUCE7. Keep it and native/build
for incremental development. Python GUI-test tools use .venv and `uv sync`.
The reusable demo generator is scripts/make_demo.py.

The earlier QML/Python companion implementation, installed plugin and launcher,
old settings, obsolete source archive, and unused JUCE 9 trial checkout were
removed at the user's request. No user score or recording was deleted. The
0.2.0 source archive was refreshed to contain only the native project and its
supporting scripts/demo.

Next priorities: testing with real recordings/audio devices, MP3/FLAC and
repeat/loop/jump cases, practice-speed support, then a Windows build. Keep the
prototype at 100% playback speed with count-in disabled for now.
