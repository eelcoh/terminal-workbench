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
from x11_window import close_window

ROOT=Path(__file__).resolve().parent.parent
ARTIFACTS=ROOT/".local/artifacts/qt-ime"


def main():
    same_window="--same-window" in sys.argv
    two_terminals="--two-terminals" in sys.argv or same_window
    mode_flags=["--same-window"] if same_window else ["--two-terminals"] if two_terminals else []
    arguments=[arg for arg in sys.argv if arg not in ("--two-terminals","--same-window")]
    runtime,viewer=[str(Path(arg).resolve()) for arg in arguments[1:3]]
    if len(arguments)==3:
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
                subprocess.run(["dbus-run-session","--",sys.executable,__file__,runtime,viewer,str(root),backend,*mode_flags],env=env,cwd=ROOT,check=True,timeout=100)
        return
    root=Path(arguments[3]);backend=arguments[4]
    artifacts=(ROOT/".local/artifacts/qt-ime-pane" if same_window else ROOT/".local/artifacts/qt-ime-transfer" if two_terminals else ARTIFACTS)/backend;artifacts.mkdir(parents=True,exist_ok=True)
    for name in ("result.json","received.bin"):(artifacts/name).unlink(missing_ok=True)
    processes=[];sock=root/"test.sock";peer_sock=None;niri_log=None
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
                (root/"niri.kdl").write_text("hotkey-overlay { skip-at-startup; }\nanimations { off; }\nprefer-no-csd\n")
                niri_log=(root/"niri.log").open("w")
                compositor=spawn(["niri","-c",str(root/"niri.kdl")],env=nested,stdout=niri_log,stderr=niri_log)
                def endpoints():
                    assert compositor.poll() is None,"Private Niri exited"
                    value=(root/"niri.log").read_text()
                    display=re.search(r"Wayland socket: (\S+)",value);ipc=re.search(r"IPC listening on: (\S+)",value)
                    return (display[1],ipc[1]) if display and ipc else None
                wayland,ipc=wait_until(endpoints,timeout=15)
                env.update(WAYLAND_DISPLAY=wayland,NIRI_SOCKET=ipc,QT_QPA_PLATFORM="wayland",QT_WAYLAND_DISABLE_WINDOWDECORATION="1")
                def host_mouse(*args):
                    return subprocess.run(["xdotool",*map(str,args)],env=nested,capture_output=True,check=True,timeout=10)
                host_window=wait_until(lambda:host_mouse("search","--onlyvisible","--pid",compositor.pid).stdout.decode().strip()).splitlines()[0]
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
            peer_root=root/"peer"
            if same_window:
                peer_root.mkdir();peer_sock=peer_root/"test.sock"
                spawn([runtime,str(peer_sock),"/bin/sh"],cwd=peer_root,stdout=log,stderr=log)
                wait_until(peer_sock.exists);peer_connection,_,_=attach(peer_sock)
                try:send(peer_connection,16,("python3 "+shlex.quote(str(reader))+"\n").encode())
                finally:peer_connection.close()
                wait_until((peer_root/"reader-ready").exists)
            command=[viewer,"--control",str(sock.with_suffix(".ctl")),"--test-report",str(report_path)]
            if same_window:command.extend(["--second-control",str(peer_sock.with_suffix(".ctl"))])
            child=spawn(command,cwd=root,env=env,stdout=log,stderr=log)
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
            else:
                cmd("niri","msg","action","move-window-to-floating")
                cmd("niri","msg","action","set-window-width","800")
                cmd("niri","msg","action","set-window-height","500")
                cmd("niri","msg","action","move-floating-window","--x","70","--y","50")
            time.sleep(.3)
            # Switching engines may enable the context only after the window focuses.
            cmd(sys.executable,"-c", "import gi; gi.require_version('IBus','1.0'); from gi.repository import IBus; IBus.init(); bus=IBus.Bus.new(); assert bus.is_connected(); assert bus.set_global_engine('anthy'); assert bus.get_global_engine().get_name()=='anthy'")
            time.sleep(.2)
            def received():return (root/"received").read_bytes() if (root/"received").exists() else b""
            def type_text(value,target=None):
                if backend=="wayland":cmd(str(root/"keyboard"),"text",value)
                else:xrun("type","--window",window if same_window else target or window,"--delay",60,value)
            def key(value,target=None):
                if backend=="wayland":
                    args=("c","Control") if value=="ctrl+c" else (value,)
                    cmd(str(root/"keyboard"),"key",*args)
                else:xrun("key","--window",window if same_window else target or window,value)
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
            def click_scene(x,y):
                if backend=="wayland":
                    layout=next(item for item in json.loads(cmd("niri","msg","-j","windows").stdout) if item["id"]==window)["layout"]
                    origin=layout["tile_pos_in_workspace_view"];offset=layout["window_offset_in_tile"]
                    host_mouse("mousemove","--window",host_window,round(origin[0]+offset[0]+x),round(origin[1]+offset[1]+y))
                    host_mouse("click",1)
                else:
                    xrun("mousemove","--window",window,round(x),round(y));xrun("click",1)
            for cycle in range(2):
                type_text("nihonn");wait_until(lambda:report().get("preedit")=="にほん")
                click_scene(*report()["copy_button"])
                wait_until(lambda:not report().get("terminal_focus",True))
                wait_until(lambda:report().get("preedit","")=="")
                assert received()==b"",("focus loss committed pending input",cycle,received())
                type_text("q");time.sleep(.2)
                assert received()==b"",("chrome input reached PTY",cycle,received())
                data=report();click_scene(data["x"]+30,data["y"]+100)
                wait_until(lambda:report().get("terminal_focus"))
                assert report()["preedit"]=="",("stale preedit on refocus",report())
            # Ordinary mode tests a refused owner; transfer mode gives the peer
            # its own real PTY, input context and independently captured bytes.
            peer_root=root/"peer"
            if two_terminals and not same_window:
                peer_root.mkdir();peer_sock=peer_root/"test.sock"
                spawn([runtime,str(peer_sock),"/bin/sh"],cwd=peer_root,stdout=log,stderr=log)
                wait_until(peer_sock.exists);peer_connection,_,_=attach(peer_sock)
                try:send(peer_connection,16,("python3 "+shlex.quote(str(reader))+"\n").encode())
                finally:peer_connection.close()
                wait_until((peer_root/"reader-ready").exists)
            type_text("nihonn");wait_until(lambda:report().get("preedit")=="にほん")
            reader_pid=(root/"reader-pid").read_text()
            blocked_path=artifacts/("peer.json" if two_terminals else "blocked.json")
            if not same_window:
                blocked_path.unlink(missing_ok=True)
                other=spawn([viewer,"--control",str((peer_sock or sock).with_suffix(".ctl")),"--test-report",str(blocked_path)],cwd=root,env=env,stdout=log,stderr=log)
            else:other=child
            def peer_report():
                try:return report().get("peer",{}) if same_window else json.loads(blocked_path.read_text())
                except (OSError,ValueError):return {}
            def peer_received():return (peer_root/"received").read_bytes() if (peer_root/"received").exists() else b""
            def focus_window(target):
                if same_window:
                    data=peer_report() if target=="peer" else report()
                    click_scene(data["x"]+30,data["y"]+100)
                elif backend=="wayland":cmd("niri","msg","action","focus-window","--id",str(target))
                else:xrun("windowfocus",target)
            def other_mapped():
                assert other.poll() is None,"Second Qt window exited"
                if backend=="wayland":
                    windows=json.loads(cmd("niri","msg","-j","windows").stdout)
                    return next((item["id"] for item in windows if item["id"]!=window),None)
                result=subprocess.run(["xdotool","search","--onlyvisible","--pid",str(other.pid)],env=env,capture_output=True,timeout=5)
                return result.stdout.decode().strip().splitlines()[-1] if result.stdout.strip() else None
            other_window="peer" if same_window else wait_until(other_mapped)
            def blocked():
                try:return "already attached" in json.loads(blocked_path.read_text()).get("status","")
                except (OSError,ValueError):return False
            wait_until(lambda:peer_report().get("ready")) if two_terminals else wait_until(blocked)
            focus_window(other_window)
            wait_until(lambda:not report().get("terminal_focus",True))
            wait_until(lambda:report().get("preedit","")=="")
            assert received()==b"",("window activation committed pending input",received())
            if two_terminals:
                peer_pid=(peer_root/"reader-pid").read_text()
                wait_until(lambda:peer_report().get("terminal_focus"))
                assert peer_received()==b"" and received()==b"",("peer startup input",peer_received(),received())
                for cycle in range(2):
                    type_text("nihonn",other_window);wait_until(lambda:peer_report().get("preedit")=="にほん")
                    assert peer_received()==b"" and received()==b"",("peer preedit leaked",cycle)
                    focus_window(window)
                    wait_until(lambda:report().get("terminal_focus") and not peer_report().get("terminal_focus",True))
                    wait_until(lambda:peer_report().get("preedit","")=="")
                    assert peer_received()==b"",("peer focus loss committed",cycle,peer_received())
                    type_text("nihonn");wait_until(lambda:report().get("preedit")=="にほん")
                    focus_window(other_window)
                    wait_until(lambda:peer_report().get("terminal_focus") and not report().get("terminal_focus",True))
                    wait_until(lambda:report().get("preedit","")=="")
                    assert received()==b"" and peer_received()==b"",("primary focus loss leaked",cycle)
                type_text("nihonn",other_window);wait_until(lambda:peer_report().get("preedit")=="にほん")
                key("space",other_window);wait_until(lambda:"日本" in peer_report().get("preedit",""))
                assert received()==b"" and peer_received()==b"",("peer conversion leaked",received(),peer_received())
                key("Return",other_window);wait_until(lambda:peer_received()=="日本".encode())
                wait_until(lambda:peer_report().get("preedit","")=="")
                type_text("nihonn",other_window);wait_until(lambda:peer_report().get("preedit")=="にほん")
                key("Return",other_window);wait_until(lambda:peer_received()=="日本にほん".encode())
                wait_until(lambda:peer_report().get("preedit","")=="")
                key("Return",other_window);key("ctrl+c",other_window)
                peer_expected="日本にほん".encode()+b"\r\x03"
                wait_until(lambda:peer_received()==peer_expected)
                assert received()==b"",("peer commit or keys reached primary",received())
                assert (peer_root/"reader-pid").read_text()==peer_pid
                os.kill(int(peer_pid),0);os.kill(int(reader_pid),0)
                if same_window:
                    # Toolbar focus must preserve B as the paste destination.
                    marker="PEER-PASTE"
                    if backend=="wayland":
                        spawn(["wl-copy","--foreground","--",marker],env=env,stdin=subprocess.DEVNULL,stdout=log,stderr=log)
                    else:
                        provider=spawn(["xclip","-selection","clipboard","-in","-quiet"],env=env,stdin=subprocess.PIPE,stdout=log,stderr=log)
                        provider.stdin.write(marker.encode());provider.stdin.close()
                    def clipboard_ready():
                        command=["wl-paste","--no-newline"] if backend=="wayland" else ["xclip","-o","-selection","clipboard"]
                        value=subprocess.run(command,env=env,capture_output=True,timeout=5)
                        return value.returncode==0 and value.stdout==marker.encode()
                    wait_until(clipboard_ready)
                    click_scene(*report()["paste_button"])
                    peer_expected+=marker.encode();wait_until(lambda:peer_received()==peer_expected)
                    assert received()==b"",("toolbar paste reached primary",received())
            if not same_window:
                if backend=="wayland":cmd("niri","msg","action","close-window","--id",str(other_window))
                else:close_window(other_window,env)
                other.wait(timeout=5)
            focus_window(window)
            wait_until(lambda:report().get("terminal_focus"))
            assert report()["preedit"]=="" and report()["ready"],("stale preedit or lost attachment on activation",report())
            assert (root/"reader-pid").read_text()==reader_pid,"Shell reader replaced"
            if two_terminals:
                os.kill(int(peer_pid),0)
                assert peer_received()==peer_expected,"Peer changed after detach"
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
            result=dict(backend=report()["platform"],input_context="ibus",ime_transport="direct D-Bus",ibus_version=cmd("ibus","version").stdout.decode().strip(),display_unset="DISPLAY" not in env,engine="anthy",preedit="にほん",converted="日本",window_focus_cancel=True,focus_cancel=True,chrome_input_isolated=True,composition_cursor=True,cancel_no_input=True,commit_exact_once=True,post_commit_keys=True)
            if two_terminals:
                assert peer_received()==peer_expected,"Primary input reached peer"
                assert (peer_root/"reader-pid").read_text()==peer_pid
                os.kill(int(peer_pid),0);os.kill(int(reader_pid),0)
                result.update(editable_terminal_transfers=True,transfer_cycles=2,peer_commits_exact_once=True,
                              peer_input_isolated=True,readers_preserved=True,same_window=same_window)
                if same_window:
                    result.pop("window_focus_cancel",None);result.update(pane_focus_cancel=True,focused_paste_button_isolated=True)
                else:result["peer_detach_preserved"]=True
            (artifacts/"result.json").write_text(json.dumps(result,indent=2,ensure_ascii=False))
            if backend=="wayland":cmd("niri","msg","action","close-window")
            else:close_window(window,env)
            child.wait(timeout=5)
            print("PASS:",backend,"real IBus/Anthy preedit, cancel, conversion and repeated exact-once commits; control/window focus isolation; post-commit Enter/Ctrl-C"+("; two editable panes in one window with isolated input and retained readers" if same_window else "; two editable terminals with isolated input and retained readers" if two_terminals else ""),flush=True)
        finally:
            if (root/"received").exists():(artifacts/"received.bin").write_bytes((root/"received").read_bytes())
            if (root/"niri.log").exists():(artifacts/"niri.log").write_bytes((root/"niri.log").read_bytes())
            if two_terminals and peer_sock:
                if (root/"peer/received").exists():(artifacts/"peer-received.bin").write_bytes((root/"peer/received").read_bytes())
                subprocess.run([runtime,"stop",str(peer_sock)],capture_output=True,timeout=12)
            subprocess.run([runtime,"stop",str(sock)],capture_output=True,timeout=12)
            for process in reversed(processes):
                if process.poll() is None:process.terminate()
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
            if niri_log:niri_log.close()


if __name__=="__main__":main()
