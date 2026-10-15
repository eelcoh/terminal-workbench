#!/usr/bin/env python3
"""Build/run the isolated Qt input experiment; never opens Workbench state."""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / ".local/build/qt-input"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("build", "run", "test"))
    args = parser.parse_args()
    subprocess.run(["cmake", "-S", str(ROOT / "spikes/qt-input"), "-B", str(BUILD)], check=True)
    subprocess.run(["cmake", "--build", str(BUILD), "--parallel"], check=True)
    if args.mode == "build":
        return
    binary = BUILD / "workbench-input-probe"
    if sys.platform == "darwin":
        binary = BUILD / "workbench-input-probe.app/Contents/MacOS/workbench-input-probe"
    command = [str(binary)]
    if args.mode == "test":
        artifact = ROOT / ".local/artifacts/toolkit-probe/qt-input.png"
        artifact.parent.mkdir(parents=True, exist_ok=True)
        command.extend(["--self-test", "--screenshot", str(artifact)])
        if sys.platform.startswith("linux"):
            command = ["xvfb-run", "-a", "env", "QT_QPA_PLATFORM=xcb", "LIBGL_ALWAYS_SOFTWARE=1", *command]
    subprocess.run(command, check=True, timeout=30 if args.mode == "test" else None)


if __name__ == "__main__":
    main()
