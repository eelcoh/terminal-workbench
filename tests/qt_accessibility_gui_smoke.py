"""Run accessibility checks on private session/accessibility buses and runtime dir."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

if not sys.platform.startswith("linux"):
    sys.exit("The AT-SPI test requires Linux; use toolkit-terminal with VoiceOver on Mac")
ROOT=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="wb-qt-a11y-") as directory:
    env=dict(os.environ,WORKBENCH_TEST_ACCESSIBILITY="1",QT_LINUX_ACCESSIBILITY_ALWAYS_ON="1",
             QT_NO_XDG_DESKTOP_PORTAL="1",XDG_RUNTIME_DIR=directory)
    # The GUI harness supplies its own Xvfb; never inherit desktop endpoints.
    for key in ("AT_SPI_BUS_ADDRESS","DBUS_SESSION_BUS_ADDRESS","WAYLAND_DISPLAY","DISPLAY","NIRI_SOCKET"):
        env.pop(key,None)
    subprocess.run(["dbus-run-session","--",sys.executable,str(ROOT/"scripts/toolkit-probe.py"),"terminal-test"],
                   cwd=ROOT,env=env,check=True,timeout=90)
