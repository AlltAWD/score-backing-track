"""Exercise the native editor and transport in a separate MuseScore GUI profile."""

from pathlib import Path
import json
import os
import re
import signal
import subprocess
import time
import zipfile


def main(appimage):
    from Xlib import X, display
    from Xlib.ext import xtest
    root = Path(__file__).resolve().parents[2]
    out = root / "artifacts/native-gui"
    out.mkdir(parents=True, exist_ok=True)
    config, data, docs = out / "config", out / "data", out / "Documents"
    plugins = docs / "MuseScore4/Plugins"
    plugins.mkdir(parents=True, exist_ok=True)
    config.mkdir(exist_ok=True)
    (config / "user-dirs.dirs").write_text(f'XDG_DOCUMENTS_DIR="{docs}"\n')
    (config / "MuseScore").mkdir(exist_ok=True)
    (config / "MuseScore/MuseScore4.ini").write_text(f'[application]\nhasCompletedFirstLaunchSetup=true\npaths\\myPlugins={plugins}\n')
    user_data = data / "MuseScore/MuseScore4"
    user_data.mkdir(parents=True, exist_ok=True)
    registry = json.loads((root / "artifacts/native-verification/data/MuseScore/MuseScore4/known_audio_plugins.json").read_text())
    registry[0]["path"] = str(Path.home() / ".vst3/Muse Backing Track.vst3")
    (user_data / "known_audio_plugins.json").write_text(json.dumps(registry))
    (user_data / "extensions").mkdir(exist_ok=True)
    (user_data / "extensions/config.json").write_text(json.dumps([{
        "uri": "musescore://extensions/v1/nativeharness.qml",
        "actions": [{"code": "main", "exec_point": "manually"}]}]))
    (plugins / "NativeHarness.qml").write_text('''import QtQuick 2.15
import MuseScore 3.0
MuseScore {
    id: root
    title: "Native Backing Test"
    pluginType: "dialog"
    requiresScore: true
    width: 340
    height: 80
    property var transport: null
    property var mixer: null
    property int phase: 0
    property real at: 0
    onRun: {
        transport = Qt.createQmlObject('import MuseScore.Playback 1.0; PlaybackToolBarModel {}', root, "NativeTestTransport");
        transport.load();
        mixer = Qt.createQmlObject('import Muse.Ui 1.0; import MuseScore.Playback 1.0; MixerPanelModel { navigationSection: NavigationSection { name: "NativeTest" } }', root, "NativeTestMixer");
        console.log("NATIVEGUI loaded channels=" + mixer.count);
    }
    Timer {
        running: root.transport !== null
        interval: 150
        repeat: true
        onTriggered: {
            if (!root.transport.isPlayAllowed || !root.mixer || root.mixer.count === 0) return;
            if (Date.now() - root.at < 1200) return;
            console.log("NATIVEGUI phase=" + root.phase + " time=" + root.transport.playTime);
            if (root.phase === 0) {
                var master = root.mixer.get(root.mixer.count - 1).channelItem;
                console.log("NATIVEGUI master=" + master.title + " fx=" + master.outputResourceItemList[0].title);
                master.outputResourceItemList[0].requestToLaunchNativeEditorView();
            } else if (root.phase === 1) {
                var c = curScore.newCursor(); c.rewindToTick(3840);
                curScore.selection.select(c.element.notes[0]);
                cmd("play-from-selection");
            } else if (root.phase === 2) {
                cmd("play");
            } else if (root.phase === 3) {
                cmd("play");
            } else if (root.phase === 4) {
                root.transport.playTime = new Date(1970, 0, 1, 0, 0, 8, 0);
            } else if (root.phase === 5) {
                cmd("play");
            }
            root.at = Date.now();
            root.phase += 1;
        }
    }
}
''')
    score = out / "Native GUI Demo.mscz"
    # Keep the live test inaudible without muting the Master generator itself.
    with zipfile.ZipFile(root / "artifacts/native-verification/Native Backing Demo.mscz") as source, zipfile.ZipFile(score, "w", zipfile.ZIP_DEFLATED) as target:
        for info in source.infolist():
            value = source.read(info)
            if info.filename == "audiosettings.json":
                settings = json.loads(value)
                settings["master"]["volumeDb"] = -144
                value = json.dumps(settings).encode()
            target.writestr(info, value)

    env = {**os.environ, "XDG_CONFIG_HOME": str(config), "XDG_DATA_HOME": str(data),
        "QT_QPA_PLATFORM": "xcb", "QT_QUICK_BACKEND": "software", "QT_OPENGL": "software"}
    env.pop("PYTHONPATH", None)
    xdisplay = display.Display()
    def click(x, y):
        xtest.fake_input(xdisplay, X.MotionNotify, x=x, y=y)
        xtest.fake_input(xdisplay, X.ButtonPress, 1)
        xtest.fake_input(xdisplay, X.ButtonRelease, 1)
        xdisplay.sync()
    def capture(name):
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "x11grab", "-video_size", "1280x1024",
            "-i", os.environ["DISPLAY"], "-frames:v", "1", str(out / name)], check=True, timeout=10)

    process = None
    try:
        with (out / "musescore.log").open("w") as log:
            process = subprocess.Popen([str(appimage), "-d", str(score)], env=env,
                stdout=log, stderr=log, start_new_session=True)
            began = time.monotonic()
            opened = False
            captures = set()
            while time.monotonic() - began < 23:
                if process.poll() is not None: break
                if not opened and time.monotonic() - began > 7:
                    click(286, 13); time.sleep(0.2); click(350, 82)
                    opened = True
                text = (out / "musescore.log").read_text(errors="replace")
                for phase, name in [(2, "playing.png"), (3, "paused.png"), (5, "seek.png")]:
                    if f"NATIVEGUI phase={phase} " in text and name not in captures:
                        capture(name); captures.add(name)
                time.sleep(0.1)
            capture("editor.png")
            # Record X11 window geometry to locate the hosted native editor.
            windows = []
            def walk(window, depth=0):
                if depth > 7: return
                try:
                    geometry = window.get_geometry()
                    point = xdisplay.screen().root.translate_coords(window, 0, 0)
                    if geometry.width >= 300 and geometry.height >= 200:
                        windows.append({"id": window.id, "name": window.get_wm_name(), "x": point.x, "y": point.y,
                            "width": geometry.width, "height": geometry.height})
                    for child in window.query_tree().children: walk(child, depth+1)
                except Exception: pass
            walk(xdisplay.screen().root)
            (out / "windows.json").write_text(json.dumps(windows, indent=2))
    finally:
        if process and process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            try: process.wait(5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL); process.wait(5)
    text = (out / "musescore.log").read_text(errors="replace")
    print("\n".join(line for line in text.splitlines() if any(x in line for x in ["NATIVEGUI", "TypeError", "ReferenceError", "VstView", "VstEditor", "Failed to"]))[-8000:])
    assert "fx=Muse Backing Track" in text, "Native Master effect was not available"
    assert (out / "playing.png").exists(), "Live transport test did not reach playback"
    print("Native GUI test reached play/pause/seek; screenshots:", out, flush=True)
