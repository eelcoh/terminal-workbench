"""Portable Qt event -> host -> disposable PTY key-byte proof; no saved sessions."""
import json
import os
from pathlib import Path
import shlex
import signal
import subprocess
import sys
import tempfile
from session_smoke import attach,send,wait_until

runtime,viewer=[Path(arg).resolve() for arg in sys.argv[1:3]]
artifacts=Path(__file__).resolve().parent.parent/".local/artifacts/qt-keys"
artifacts.mkdir(parents=True,exist_ok=True)
report=artifacts/"report.json";report.unlink(missing_ok=True)
with tempfile.TemporaryDirectory(prefix="wb-keys-",dir="/tmp") as directory:
    root=Path(directory);sock=root/"session.sock";server=client=None;connection=None
    def stop_client():
        if not client:return
        # xvfb-run is a wrapper: terminate the isolated GUI process group too.
        try:os.killpg(client.pid,signal.SIGTERM)
        except ProcessLookupError:pass
        try:client.wait(timeout=5)
        except subprocess.TimeoutExpired:
            try:os.killpg(client.pid,signal.SIGKILL)
            except ProcessLookupError:pass
            client.wait()
    with (artifacts/"run.log").open("w") as log:
        try:
            server=subprocess.Popen([str(runtime),str(sock),"/bin/sh"],cwd=root,stdout=log,stderr=log)
            wait_until(sock.exists)
            connection,_,_=attach(sock)
            reader=root/"reader.py"
            reader.write_text("""import os,tty,termios
from pathlib import Path
saved=termios.tcgetattr(0);tty.setraw(0)
try:
 os.write(1,b'QT-KEY-PROBE\\r\\n');Path('ready').touch();data=bytearray()
 while True:
  chunk=os.read(0,4096)
  if not chunk:break
  data.extend(chunk);Path('input').write_bytes(data)
finally:termios.tcsetattr(0,termios.TCSANOW,saved)
""")
            send(connection,16,("python3 "+shlex.quote(str(reader))+"\n").encode())
            wait_until((root/"ready").exists);connection.close();connection=None
            command=[str(viewer),"--control",str(sock.with_suffix(".ctl")),"--test-report",str(report),"--test-keys"]
            if sys.platform.startswith("linux"):
                command=["xvfb-run","-a","env","QT_QPA_PLATFORM=xcb","LIBGL_ALWAYS_SOFTWARE=1",*command]
            client=subprocess.Popen(command,cwd=root,stdout=log,stderr=log,start_new_session=True)
            def state():
                try:return json.loads(report.read_text())
                except (OSError,ValueError):return {}
            def completed():
                if client.poll() is not None:raise RuntimeError("Qt key probe exited; see "+str(artifacts/"run.log"))
                return state().get("keys_done") and (root/"input").exists()
            wait_until(completed,timeout=15)
            expected=b"\x1b[D"+(b"\x1bb\x1bf\x1bf" if sys.platform=="darwin" else b"\x1b[1;3D\x1b[1;3C\x1b[1;3C")+b"\x1b[C\x1b[1;5D"
            try:
                wait_until(lambda:(root/"input").read_bytes()==expected,timeout=5)
                # Require a quiet period so asynchronous release/duplicate bytes fail.
                import time
                time.sleep(.2)
            finally:
                # Preserve byte evidence even when the native platform check fails.
                actual=(root/"input").read_bytes() if (root/"input").exists() else b""
                (artifacts/"input.bin").write_bytes(actual)
                result=state();result.update(expected_hex=expected.hex(),actual_hex=actual.hex(),key_bytes_match=actual==expected,platform_policy="mac-word-motion" if sys.platform=="darwin" else "linux-generic-alt-arrows")
                stop_client()
                report.write_text(json.dumps(result,indent=2)+"\n")
            assert actual==expected,(actual,expected)
            print("PASS: Qt host -> real PTY plain/control arrows, Option word motion on Mac, Linux Alt arrows, repeat and no release duplicates")
            print("Artifacts:",artifacts)
        finally:
            if connection:connection.close()
            stop_client()
            if server:
                subprocess.run([str(runtime),"stop",str(sock)],capture_output=True,timeout=12)
                try:server.wait(timeout=5)
                except subprocess.TimeoutExpired:server.kill();server.wait()
