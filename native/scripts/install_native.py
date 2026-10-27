"""Copy the built VST3 bundle into the current user's plugin directory."""

import argparse
import os
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--bundle", type=Path, default=root / "native/build/MuseBacking_artefacts/Release/VST3/Muse Backing Track.vst3")
parser.add_argument("--destination", type=Path)
args = parser.parse_args()
if args.destination:
    directory = args.destination
elif sys.platform.startswith("linux"):
    directory = Path.home() / ".vst3"
elif sys.platform == "win32":
    directory = Path(os.environ["LOCALAPPDATA"]) / "Programs/Common/VST3"
else:
    raise SystemExit("Use your platform's VST3 directory explicitly with --destination.")
if not args.bundle.is_dir():
    raise SystemExit(f"Build the VST3 first; bundle not found: {args.bundle}")
target = directory / args.bundle.name
directory.mkdir(parents=True, exist_ok=True)
shutil.copytree(args.bundle, target, dirs_exist_ok=True)
print("Installed:", target)
print("Restart MuseScore. In Mixer, add MuseBacking → Muse Backing Track to Master effects.")
