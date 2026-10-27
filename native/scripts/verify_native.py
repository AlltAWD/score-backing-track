"""Verify the native VST3 using an isolated MuseScore profile and exported audio.

Run under xvfb-run; does not launch the Python companion or use a local server.
"""

from pathlib import Path
import argparse
import base64
import json
import os
import shutil
import subprocess
import sys
import zipfile
import array
import math
import wave

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / "scripts"))
from make_demo import generate

parser = argparse.ArgumentParser()
parser.add_argument("appimage", type=Path)
parser.add_argument("--scan-only", action="store_true")
parser.add_argument("--debug-export", action="store_true")
parser.add_argument("--gui", action="store_true")
args = parser.parse_args()
if args.gui:
    from gui_native import main
    main(args.appimage)
    raise SystemExit(0)
out = root / "artifacts" / "native-verification"
out.mkdir(parents=True, exist_ok=True)
config, data = out / "config", out / "data"
config.mkdir(exist_ok=True)
data.mkdir(exist_ok=True)
env = {**os.environ, "XDG_CONFIG_HOME": str(config), "XDG_DATA_HOME": str(data),
    "QT_QPA_PLATFORM": "xcb", "QT_QUICK_BACKEND": "software", "QT_OPENGL": "software"}
env.pop("PYTHONPATH", None)
binary = root / "native/build/MuseBacking_artefacts/Release/VST3/Muse Backing Track.vst3"
tests = root / "native/build/MuseBackingTests_artefacts/Release/MuseBackingTests"


def run(name, command, timeout=90):
    with (out / (name + ".log")).open("w") as log:
        result = subprocess.run([str(value) for value in command], env=env, stdout=log, stderr=log, timeout=timeout)
    if result.returncode:
        print((out / (name + ".log")).read_text(errors="replace")[-6000:])
        raise SystemExit(f"{name} failed with code {result.returncode}")
    print(name + " passed", flush=True)


if args.debug_export:
    extracted = Path("/tmp/musescore-native-debug/squashfs-root")
    env["APPDIR"] = str(extracted)
    env["APPIMAGE"] = str(args.appimage)
    run("debug-export", ["gdb", "--nx", "--batch", "-ex", "set pagination off", "-ex", "run",
        "-ex", "thread apply all bt", "--args", extracted / "bin/mscore4portable", "-o",
        out / "debug-render.wav", out / "Native Backing Demo.mscz"], timeout=120)
    print((out / "debug-export.log").read_text(errors="replace")[-16000:])
    raise SystemExit(0)

run("scan", [args.appimage, "--register-audio-plugin", binary])
registry = data / "MuseScore/MuseScore4/known_audio_plugins.json"
print("Registry:", registry.read_text(), flush=True)
if args.scan_only:
    raise SystemExit(0)
run("processor-tests", [tests])
score, wav = generate(out / "demo")
run("preset", [tests, "--write-state", wav, out / "component-state.bin"])
run("convert", [args.appimage, "-o", out / "base.mscz", score])
print("Base score audio settings:", flush=True)
with zipfile.ZipFile(out / "base.mscz") as archive:
    settings = json.loads(archive.read("audiosettings.json"))
print(json.dumps(settings, indent=2), flush=True)

meta = next(item["meta"] for item in json.loads(registry.read_text()) if item["meta"]["id"] == "Muse Backing Track")
assert "Generator" in meta["attributes"]["categories"], "Host must recognize generator category"
# The registry and score serializers use different names for the same type.
meta["type"] = "vst_plugin"
settings["master"]["fxChain"] = {"0": {"active": True, "chainOrder": 0,
    "resourceMeta": meta,
    "unitConfiguration": {"componentState": base64.b64encode((out / "component-state.bin").read_bytes()).decode()}}}
# This score has one piano part. Muting it proves the generator runs on silence.
settings["tracks"] = [{"partId": "1", "instrumentId": "piano",
    "soloMuteState": {"mute": True, "solo": False},
    "out": {"balance": 0, "volumeDb": -144, "fxChain": {}}}]
native_score = out / "Native Backing Demo.mscz"
with zipfile.ZipFile(out / "base.mscz") as original, zipfile.ZipFile(native_score, "w", zipfile.ZIP_DEFLATED) as target:
    for info in original.infolist():
        content = json.dumps(settings).encode() if info.filename == "audiosettings.json" else original.read(info)
        target.writestr(info, content)
run("render", [args.appimage, "-o", out / "native-render.wav", native_score], timeout=120)

rendered = array.array("f", subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(out / "native-render.wav"),
    "-af", "pan=mono|c0=c0", "-f", "f32le", "-ar", "44100", "pipe:1"]))
if sys.byteorder != "little": rendered.byteswap()
with wave.open(str(wav), "rb") as reader:
    original = array.array("h", reader.readframes(reader.getnframes()))
if sys.byteorder != "little": original.byteswap()
assert len(rendered) >= len(original), "Host truncated the recording"
error = math.sqrt(sum((rendered[i] - sample / 32768.0) ** 2 for i, sample in enumerate(original)) / len(original))
signal = math.sqrt(sum((sample / 32768.0) ** 2 for sample in original) / len(original))
print(f"Native host export: RMS error={error:.8f}; source RMS={signal:.8f}", flush=True)
assert error < signal * 0.01, "Rendered recording differs from the source by more than 1% RMS"

# Removing the native effect must leave an otherwise identical muted score silent.
settings["master"]["fxChain"] = {}
silent_score = out / "Muted Reference.mscz"
with zipfile.ZipFile(native_score) as source, zipfile.ZipFile(silent_score, "w", zipfile.ZIP_DEFLATED) as target:
    for info in source.infolist():
        target.writestr(info, json.dumps(settings).encode() if info.filename == "audiosettings.json" else source.read(info))
run("render-muted", [args.appimage, "-o", out / "muted-render.wav", silent_score], timeout=120)
silent = array.array("f", subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(out / "muted-render.wav"), "-f", "f32le", "pipe:1"]))
if sys.byteorder != "little": silent.byteswap()
assert max(map(abs, silent), default=0) < 0.00001, "The muted reference contains score audio"
summary = {"native_processor_tests": (out / "processor-tests.log").read_text().strip(),
    "host": "MuseScore Studio 4.7.5", "category": meta["attributes"]["categories"],
    "audio_samples_compared": len(original), "rms_error": error, "source_rms": signal,
    "muted_reference_peak": max(map(abs, silent), default=0), "companion_required": False}
(out / "verification.json").write_text(json.dumps(summary, indent=2) + "\n")
print("All native processor and MuseScore export checks passed.", flush=True)
