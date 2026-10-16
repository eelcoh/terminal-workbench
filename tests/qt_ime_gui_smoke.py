"""Real IBus/Anthy input through Qt, with private settings/buses and raw PTY."""
import json
import os
from pathlib import Path
import select
import re
import shlex
import subprocess
import sys
import tempfile
import time
from session_smoke import attach,send,wait_until
from gui_smoke import close_window

ROOT=Path(__file__).resolve().parent.parent
ARTIFACTS=ROOT/".local/artifacts/qt-ime"


def main():
    runtime,viewer=[str(Path(arg).resolve()) for arg in sys.argv[1:3]]
    if len(sys.argv)==3:
        for backend in ("xcb","wayland"):
            with tempfile.TemporaryDirectory(prefix="wb-qt-ime-") as directory:
                root=Path(directory)
                for name in ("runtime","config","cache","data"):(root/name).mkdir(mode=0o700)
                env=dict(os.environ,QT_IM_MODULE="ibus",QT_NO_XDG_DESKTOP_PORTAL="1",GSETTINGS_BACKEND="memory",
                    XDG_RUNTIME_DIR=str(root/"runtime"),XDG_CONFIG_HOME=str(root/"config"),
                    XDG_CACHE_HOME=str(root/"cache"),XDG_DATA_HOME=str(root/"data"))
                for name in ("DBUS_SESSION_BUS_ADDRESS","IBUS_ADDRESS","IBUS_ADDRESS_FILE","IBUS_USE_PORTAL",
                             "DISPLAY","WAYLAND_DISPLAY","NIRI_SOCKET","AT_SPI_BUS_ADDRESS",
                             "QT_SCALE_FACTOR","QT_SCREEN_SCALE_FACTORS","QT_FONT_DPI",
                             "WORKBENCH_TEST_KEYBOARD_LAYOUT","WORKBENCH_TEST_KEYBOARD_VARIANT"):
                    env.pop(name,None)
                env["IBUS_ADDRESS"]="unix:path="+str(root/"ibus.sock")
                env["IBUS_ADDRESS_FILE"]=str(root/"ibus.address")
                subprocess.run(["dbus-run-session","--",sys.executable,__file__,runtime,viewer,str(root),backend],env=env,cwd=ROOT,check=True,timeout=100)
        return
    root=Path(sys.argv[3]);backend=sys.argv[4]
    artifacts=ARTIFACTS/backend;artifacts.mkdir(parents=True,exist_ok=True)
    for name in ("result.json","received.bin"):(artifacts/name).unlink(missing_ok=True)
    processes=[];sock=root/"test.sock";niri_log=None
    with (artifacts/"run.log").open("w") as log:
        def spawn(args,**kwargs):
            child=subprocess.Popen(args,**kwargs);processes.append(child);return child
        try:
            rd,wr=os.pipe()
            try:
                spawn(["Xvfb","-displayfd",str(wr),"-screen","0","1400x1000x24","-noreset","-nolisten","tcp"],pass_fds=(wr,),stdout=log,stderr=log)
                os.close(wr);wr=None
                assert select.select([rd],[],[],5)[0]
                with os.fdopen(rd) as pipe:display=pipe.readline().strip()
                rd=None
            finally:
                for fd in (rd,wr):
                    if fd is not None:os.close(fd)
            env=dict(os.environ,DISPLAY=":"+display,QT_QPA_PLATFORM="xcb",LIBGL_ALWAYS_SOFTWARE="1",
                     IBUS_ADDRESS="unix:path="+str(root/"ibus.sock"),IBUS_ADDRESS_FILE=str(root/"ibus.address"),QT_LOGGING_RULES="qt.qpa.input.methods.debug=true")
            def cmd(*args):
                result=subprocess.run(args,env=env,capture_output=True,timeout=10)
                if result.returncode:
                    log.write(result.stdout.decode(errors="replace")+result.stderr.decode(errors="replace"));log.flush()
                result.check_returncode();return result
            def xrun(*args):return cmd("xdotool",*map(str,args))
            if backend=="wayland":
                nested=dict(env,WINIT_UNIX_BACKEND="x11")
                (root/"niri.kdl").write_text("hotkey-overlay { skip-at-startup; }\nanimations { off; }\n")
                niri_log=(root/"niri.log").open("w")
                compositor=spawn(["niri","-c",str(root/"niri.kdl")],env=nested,stdout=niri_log,stderr=niri_log)
                def endpoints():
                    assert compositor.poll() is None,"Private Niri exited"
                    value=(root/"niri.log").read_text()
                    display=re.search(r"Wayland socket: (\S+)",value);ipc=re.search(r"IPC listening on: (\S+)",value)
                    return (display[1],ipc[1]) if display and ipc else None
                wayland,ipc=wait_until(endpoints,timeout=15)
                env.update(WAYLAND_DISPLAY=wayland,NIRI_SOCKET=ipc,QT_QPA_PLATFORM="wayland")
                env.pop("DISPLAY",None)
                protocol=ROOT/"tests/fixtures/virtual-keyboard-unstable-v1.xml"
                cmd("wayland-scanner","client-header",str(protocol),str(root/"virtual-keyboard.h"))
                cmd("wayland-scanner","private-code",str(protocol),str(root/"virtual-keyboard.c"))
                flags=cmd("pkg-config","--cflags","--libs","wayland-client","xkbcommon").stdout.decode().split()
                cmd("cc","-std=c11","-Wall","-Wextra","-Werror","-I"+str(root),str(ROOT/"tests/wayland_keyboard.c"),str(root/"virtual-keyboard.c"),"-o",str(root/"keyboard"),*flags)
            daemon=spawn(["ibus-daemon","--single","--emoji-extension=disable","--address="+env["IBUS_ADDRESS"],"--cache=none"],env=env,stdout=log,stderr=log)
            def ibus_ready():
                assert daemon.poll() is None,"Private IBus daemon exited"
                result=subprocess.run(["ibus","list-engine"],env=env,capture_output=True,timeout=5)
                return result.returncode==0 and b"anthy" in result.stdout
            wait_until(ibus_ready,timeout=15)
            # Qt reads this address file, rather than using the host IBus service.
            (root/"ibus.address").write_text("IBUS_ADDRESS="+env["IBUS_ADDRESS"]+"\nIBUS_DAEMON_PID="+str(daemon.pid)+"\n")
            spawn([runtime,str(sock),"/bin/sh"],cwd=root,stdout=log,stderr=log)
            wait_until(sock.exists);connection,_,_=attach(sock)
            reader=root/"reader.py"
            reader.write_text("""import os,tty,termios,select
from pathlib import Path
saved=termios.tcgetattr(0);tty.setraw(0)
try:
 Path('reader-pid').write_text(str(os.getpid()))
 os.write(1,b'\\x1b[2J\\x1b[HQT-SNAPSHOT\\r\\nIME-ready: ')
 Path('reader-ready').touch()
 while True:
  if select.select([0],[],[],.05)[0]:
   chunk=os.read(0,65536)
   if not chunk:break
   with open('received','ab') as f:f.write(chunk)
finally:termios.tcsetattr(0,termios.TCSANOW,saved)
""")
            send(connection,16,("python3 "+shlex.quote(str(reader))+"\n").encode())
            wait_until((root/"reader-ready").exists);connection.close()
            report_path=artifacts/"terminal.json";report_path.unlink(missing_ok=True)
            child=spawn([viewer,"--control",str(sock.with_suffix(".ctl")),"--test-report",str(report_path)],cwd=root,env=env,stdout=log,stderr=log)
            def report():
                try:return json.loads(report_path.read_text())
                except (OSError,ValueError):return {}
            def mapped():
                assert child.poll() is None,"Qt terminal exited"
                if backend=="wayland":
                    windows=json.loads(cmd("niri","msg","-j","windows").stdout)
                    return windows[0]["id"] if windows else None
                result=xrun("search","--onlyvisible","--pid",child.pid)
                return result.stdout.decode().strip().splitlines()[-1] if result.stdout.strip() else None
            wait_until(lambda:report().get("ready"));window=wait_until(mapped)
            assert report()["platform"]==backend,report()
            if backend=="xcb":xrun("windowfocus",window)
            time.sleep(.2)
            # Switching engines may enable the context only after the window focuses.
            cmd(sys.executable,"-c", "import gi; gi.require_version('IBus','1.0'); from gi.repository import IBus; IBus.init(); bus=IBus.Bus.new(); assert bus.is_connected(); assert bus.set_global_engine('anthy'); assert bus.get_global_engine().get_name()=='anthy'")
            time.sleep(.2)
            def received():return (root/"received").read_bytes() if (root/"received").exists() else b""
            def type_text(value):
                if backend=="wayland":cmd(str(root/"keyboard"),"text",value)
                else:xrun("type","--window",window,"--delay",60,value)
            def key(value):
                if backend=="wayland":
                    args=("c","Control") if value=="ctrl+c" else (value,)
                    cmd(str(root/"keyboard"),"key",*args)
                else:xrun("key","--window",window,value)
            def capture(name):
                request=Path(str(report_path)+".capture");request.touch();wait_until(lambda:not request.exists())
                (artifacts/(name+".png")).write_bytes(Path(str(report_path)+".png").read_bytes())
            type_text("nihonn")
            wait_until(lambda:report().get("preedit")=="にほん")
            assert received()==b"",received()
            def composition_caret():
                data=report();base=data["caret"];ime=data["ime_caret"]
                assert ime[0]>=base[0] and ime[1]==base[1] and ime[3]==base[3],data
                return ime
            initial_caret=composition_caret()
            key("Left")
            wait_until(lambda:report().get("ime_caret",[initial_caret[0]])[0]<initial_caret[0])
            assert received()==b"",("composition cursor leaked",received())
            key("Right")
            wait_until(lambda:report().get("ime_caret")==initial_caret)
            capture("hiragana-preedit")
            key("Escape")
            wait_until(lambda:report().get("preedit","")=="")
            assert received()==b"",("cancellation leaked",received())
            type_text("nihonn");wait_until(lambda:report().get("preedit")=="にほん")
            key("space");wait_until(lambda:"日本" in report().get("preedit",""))
            assert received()==b"",("conversion leaked",received())
            composition_caret()
            capture("converted-preedit")
            key("Return")
            wait_until(lambda:received()=="日本".encode())
            wait_until(lambda:report().get("preedit","")=="")
            assert report()["ime_caret"]==report()["caret"],report()
            # A second cycle catches double commits and stale composition state.
            type_text("nihonn");wait_until(lambda:report().get("preedit")=="にほん")
            key("Return");wait_until(lambda:received()=="日本にほん".encode())
            expected="日本にほん".encode()
            # Return after commit must be a real terminal key, not eaten by stale preedit.
            key("Return");expected+=b"\r";wait_until(lambda:received()==expected)
            key("ctrl+c");expected+=b"\x03";wait_until(lambda:received()==expected)
            result=dict(backend=report()["platform"],input_context="ibus",ime_transport="direct D-Bus",ibus_version=cmd("ibus","version").stdout.decode().strip(),display_unset="DISPLAY" not in env,engine="anthy",preedit="にほん",converted="日本",composition_cursor=True,cancel_no_input=True,commit_exact_once=True,post_commit_keys=True)
            (artifacts/"result.json").write_text(json.dumps(result,indent=2,ensure_ascii=False))
            if backend=="wayland":cmd("niri","msg","action","close-window")
            else:close_window(window,env)
            child.wait(timeout=5)
            print("PASS:",backend,"real IBus/Anthy preedit, cancel, conversion and repeated exact-once commits; post-commit Enter/Ctrl-C",flush=True)
        finally:
            if (root/"received").exists():(artifacts/"received.bin").write_bytes((root/"received").read_bytes())
            if (root/"niri.log").exists():(artifacts/"niri.log").write_bytes((root/"niri.log").read_bytes())
            subprocess.run([runtime,"stop",str(sock)],capture_output=True,timeout=12)
            for process in reversed(processes):
                if process.poll() is None:process.terminate()
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
            if niri_log:niri_log.close()


if __name__=="__main__":main()
