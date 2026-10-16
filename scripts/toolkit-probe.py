#!/usr/bin/env python3
"""Build/run the isolated Qt input experiment; never opens Workbench state."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / ".local/build/qt-input"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("build", "run", "test", "terminal", "terminal-test", "terminal-wayland-test", "terminal-ime-test", "text-test"))
    args = parser.parse_args()
    if args.mode in ("terminal-test", "terminal-wayland-test", "terminal-ime-test") and not sys.platform.startswith("linux"):
        parser.error("Qt terminal GUI tests require Linux; use toolkit-terminal for native Mac testing")
    subprocess.run(["cmake", "-S", str(ROOT / "spikes/qt-input"), "-B", str(BUILD), "-DWORKBENCH_TERMINAL_PROBE=" + ("ON" if (args.mode.startswith("terminal") or args.mode=="text-test") else "OFF")], check=True)
    subprocess.run(["cmake", "--build", str(BUILD), "--parallel"], check=True)
    if args.mode == "build":
        return
    name = "workbench-terminal-probe" if args.mode.startswith("terminal") else "workbench-input-probe"
    binary = BUILD / name
    if args.mode == "text-test":
        name="workbench-text-probe"
        binary=BUILD/name
    if sys.platform == "darwin":
        binary = BUILD / (name + ".app/Contents/MacOS") / name
    if args.mode == "text-test":
        artifact=ROOT/".local/artifacts/qt-text"
        (artifact/"report.json").unlink(missing_ok=True)
        command=[str(binary),"--output",str(artifact)]
        if sys.platform.startswith("linux"):
            command=["xvfb-run","-a","env","QT_QPA_PLATFORM=xcb","LIBGL_ALWAYS_SOFTWARE=1",*command]
        subprocess.run(command,check=True,timeout=30)
        subprocess.run([sys.executable,str(ROOT/"tests/qt_text_artifacts.py"),str(artifact)],check=True)
        return
    if args.mode in ("terminal-test", "terminal-wayland-test", "terminal-ime-test"):
        test = {"terminal-test":"qt_terminal_gui_smoke.py",
                "terminal-wayland-test":"qt_wayland_terminal_gui_smoke.py",
                "terminal-ime-test":"qt_ime_gui_smoke.py"}[args.mode]
        subprocess.run([sys.executable, str(ROOT / "tests" / test),
                        str(ROOT / "target/debug/workbench-session"), str(binary)], check=True)
        return
    if args.mode == "terminal":
        # Disposable shell only: no catalog, ownership transfer or saved sessions.
        with tempfile.TemporaryDirectory(prefix="wb-qt-") as directory:
            socket = Path(directory) / "terminal.sock"
            runtime = ROOT / "target/debug/workbench-session"
            with (Path(directory) / "runtime.log").open("w") as log:
                server = subprocess.Popen([str(runtime), str(socket)], cwd=Path.home(), stdout=log, stderr=log)
                try:
                    deadline = time.monotonic() + 10
                    while not socket.with_suffix(".ctl").exists():
                        if server.poll() is not None or time.monotonic() > deadline:
                            raise RuntimeError("Disposable terminal runtime did not start")
                        time.sleep(.02)
                    subprocess.run([str(binary), "--control", str(socket.with_suffix(".ctl")), "--disposable"], check=True)
                finally:
                    subprocess.run([str(runtime), "stop", str(socket)], capture_output=True, timeout=12)
                    try:
                        server.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        server.terminate()
                        server.wait(timeout=5)
        return
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
