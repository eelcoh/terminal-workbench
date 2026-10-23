"""Disposable real-runtime Qt host integration; never opens saved user state."""
import json
import os
from pathlib import Path
import select
import shlex
import socket
import subprocess
import sys
import tempfile
import time
from session_smoke import attach, send, wait_until, read_frame
from x11_window import close_window

runtime, viewer = [Path(arg).resolve() for arg in sys.argv[1:3]]
artifacts = Path(__file__).resolve().parent.parent / ".local/artifacts/qt-terminal"
artifacts.mkdir(parents=True, exist_ok=True)
accessibility = os.environ.get("WORKBENCH_TEST_ACCESSIBILITY") == "1"
processes = []
with tempfile.TemporaryDirectory(prefix="wb-qt-test-") as directory:
    root = Path(directory)
    sock = root / "terminal.sock"
    with (artifacts / "run.log").open("w") as log:
        try:
            server = subprocess.Popen([str(runtime), str(sock), "/bin/sh"], cwd=root, stdout=log, stderr=log)
            processes.append(server)
            wait_until(sock.exists)
            c, _, _ = attach(sock)
            reader = root / "reader.py"
            reader.write_text("""import os,tty,time,select,termios,struct
from pathlib import Path
original=termios.tcgetattr(0);tty.setraw(0)
try:
 Path('reader-pid').write_text(str(os.getpid()))
 os.write(1,b''.join(b'history-%03d\\r\\n'%i for i in range(80)))
 os.write(1,'\\x1b[2J\\x1b[Hcopy sample\\r\\ncafé λ\\r\\nQT-SNAPSHOT\\r\\n\\x1b[32mgreen text\\x1b[0m\\r\\ncluster: é 漢 😀\\r\\n\\r\\nshape: -> != ===\\r\\nemoji: 👩🏽‍💻 X\\x1b[?2004h'.encode())
 Path('ready').touch();data=bytearray();updated=False
 while True:
  if select.select([0],[],[],.02)[0]:
   chunk=os.read(0,65536)
   if not chunk:break
   data.extend(chunk);Path('input').write_bytes(data)
  rows,cols,_,_=struct.unpack('HHHH',__import__('fcntl').ioctl(0,termios.TIOCGWINSZ,b'\\0'*8))
  Path('size').write_text(str(cols)+','+str(rows))
  if Path('live').exists() and not updated:
   os.write(1,b'\\x1b[6;1HLIVE-OUTPUT');updated=True
  if Path('mouse').exists():
   os.write(1,b'\\x1b[?1000h\\x1b[?1006h');Path('mouse').rename('mouse-on')
  if Path('nomouse').exists():
   os.write(1,b'\\x1b[?1000l\\x1b[?1006l');Path('nomouse').rename('mouse-off')
finally:termios.tcsetattr(0,termios.TCSANOW,original)
""")
            send(c,16,("python3 " + shlex.quote(str(reader)) + "\n").encode())
            wait_until((root / "ready").exists)
            pid = (root / "reader-pid").read_text()
            c.close()
            rd, wr = os.pipe()
            x = subprocess.Popen(["Xvfb", "-displayfd", str(wr), "-screen", "0", "1500x1100x24", "-noreset", "-nolisten", "tcp"], pass_fds=(wr,), stdout=log, stderr=log)
            processes.append(x)
            os.close(wr)
            assert select.select([rd],[],[],5)[0]
            with os.fdopen(rd) as f:
                display = f.readline().strip()
            env = dict(os.environ, DISPLAY=":"+display, QT_QPA_PLATFORM="xcb", LIBGL_ALWAYS_SOFTWARE="1")
            env.pop("WAYLAND_DISPLAY",None)
            def cmd(*args):
                return subprocess.run(["xdotool",*map(str,args)],env=env,check=True,capture_output=True,text=True).stdout.strip()
            def report(path):
                try:return json.loads(path.read_text())
                except (OSError, ValueError):return {}
            def start(name, ime=False):
                path = artifacts / (name+".json")
                path.unlink(missing_ok=True)
                args=[str(viewer),"--control",str(sock.with_suffix(".ctl")),"--test-report",str(path)]
                if ime:args.append("--test-ime")
                if accessibility:args.append("--test-accessibility")
                v=subprocess.Popen(args,cwd=root,env=env,stdout=log,stderr=log)
                processes.append(v)
                wait_until(lambda:report(path).get("ready"),timeout=10)
                wait_until(lambda:"QT-SNAPSHOT" in report(path).get("text",""))
                found=cmd("search","--onlyvisible","--pid",v.pid).splitlines()[-1]
                cmd("windowfocus",found)
                return v,found,path
            v,window,path=start("first",True)
            wait_until(lambda:report(path).get("ime_done"))
            if accessibility:
                wait_until(lambda:report(path).get("accessibility_ok"))
                subprocess.run([sys.executable,str(Path(__file__).with_name("qt_atspi_probe.py")),str(artifacts/"atspi.json"),str(root/"live")],env=env,check=True,timeout=25)
            wait_until(lambda:(root/"input").exists())
            assert (root/"input").read_bytes()=="日本".encode(), "preedit/cancel leaked or commit duplicated"
            initial=report(path)
            assert "café λ" in initial["text"]
            assert initial["caret"][2]>0 and initial["caret"][3]>0
            (root/"live").touch()
            wait_until(lambda:"LIVE-OUTPUT" in report(path).get("text",""))
            cmd("type","--clearmodifiers","--window",window,"--delay",1,"Aa9")
            cmd("key","--window",window,"Return","ctrl+c","Tab","Left")
            expected="日本".encode()+b"Aa9\r\x03\t\x1b[D"
            wait_until(lambda:(root/"input").read_bytes()==expected)
            # Real native mouse drag and clipboard shortcut; use reported logical geometry.
            data=report(path)
            x0=int(data["x"]+8+data["cell_width"]*.2)
            y0=int(data["y"]+8+data["cell_height"]*.5)
            # Ghostty includes the final cell once the pointer passes its midpoint.
            x1=int(data["x"]+8+data["cell_width"]*10.7)
            cmd("mousemove","--window",window,x0,y0)
            cmd("mousedown",1)
            cmd("mousemove","--window",window,x1,y0)
            cmd("mouseup",1)
            cmd("key","--window",window,"ctrl+shift+c")
            def clipboard():
                result=subprocess.run(["xclip","-o","-selection","clipboard"],env=env,capture_output=True)
                return result.stdout if result.returncode==0 else b""
            wait_until(lambda:clipboard()==b"copy sample")
            cmd("key","--window",window,"ctrl+shift+v")
            expected+=b"\x1b[200~copy sample\x1b[201~"
            wait_until(lambda:(root/"input").read_bytes()==expected)
            # A shaped operator still owns the original two source cells.
            data=report(path);yshape=int(data["y"]+8+data["cell_height"]*6.5)
            cmd("mousemove","--window",window,int(data["x"]+8+data["cell_width"]*7.2),yshape)
            cmd("mousedown",1)
            cmd("mousemove","--window",window,int(data["x"]+8+data["cell_width"]*8.7),yshape)
            cmd("mouseup",1);cmd("key","--window",window,"ctrl+shift+c")
            wait_until(lambda:clipboard()==b"->")
            cmd("key","--window",window,"ctrl+shift+v")
            expected+=b"\x1b[200~->\x1b[201~"
            wait_until(lambda:(root/"input").read_bytes()==expected)
            # Double-click selects a word and triple-click a line, through VT gestures.
            def cell(column,row):
                data=report(path)
                return int(data["x"]+8+data["cell_width"]*(column+.5)),int(data["y"]+8+data["cell_height"]*(row+.5))
            cmd("mousemove","--window",window,*cell(7,0));cmd("click","--repeat",2,"--delay",80,1)
            cmd("key","--window",window,"ctrl+shift+c")
            wait_until(lambda:clipboard()==b"sample")
            cmd("mousemove","--window",window,*cell(2,1));cmd("click","--repeat",3,"--delay",80,1)
            cmd("key","--window",window,"ctrl+shift+c")
            wait_until(lambda:clipboard()=="café λ".encode())
            # With mouse tracking enabled, clicks and the wheel go to the application;
            # Shift keeps local selection. SGR coordinates are one-based.
            time.sleep(.6)  # end the click sequence so the next press is a fresh click
            (root/"mouse").touch();wait_until((root/"mouse-on").exists);time.sleep(.1)
            cmd("mousemove","--window",window,*cell(3,2));cmd("click",1)
            expected+=b"\x1b[<0;4;3M\x1b[<0;4;3m"
            wait_until(lambda:(root/"input").read_bytes()==expected)
            cmd("click",4)
            wait_until(lambda:(root/"input").read_bytes().startswith(expected+b"\x1b[<64;4;3M"))
            expected=(root/"input").read_bytes()
            cmd("keydown","shift");cmd("click",1);cmd("keyup","shift");time.sleep(.3)
            assert (root/"input").read_bytes()==expected,"Shift+click reached the mouse-tracking application"
            (root/"nomouse").touch();wait_until((root/"mouse-off").exists);time.sleep(.1)
            # Scroll the Ghostty viewport, then typing returns to the live terminal.
            cmd("click",4)
            wait_until(lambda:"history-" in report(path).get("text",""))
            cmd("type","--window",window,"z")
            expected+=b"z"
            wait_until(lambda:"LIVE-OUTPUT" in report(path).get("text",""))
            cmd("windowsize",window,1100,760)
            wait_until(lambda:report(path).get("width",0)>1000)
            def resized():
                data=report(path)
                if not data:return False
                wanted=(int((data["width"]-16)/data["cell_width"]),int((data["height"]-16)/data["cell_height"]))
                return (root/"size").read_text()==f"{wanted[0]},{wanted[1]}"
            wait_until(resized)
            # An extra owner must fail without affecting the first viewer.
            blocked_path=artifacts/"blocked.json"
            blocked_path.unlink(missing_ok=True)
            blocked=subprocess.Popen([str(viewer),"--control",str(sock.with_suffix(".ctl")),"--test-report",str(blocked_path)],cwd=root,env=env,stdout=log,stderr=log)
            processes.append(blocked)
            wait_until(lambda:"already attached" in report(blocked_path).get("status",""))
            assert report(path)["ready"]
            blocked.terminate();blocked.wait(timeout=5)
            close_window(window,env);v.wait(timeout=5)
            time.sleep(.1)
            assert (root/"reader-pid").read_text()==pid
            os.kill(int(pid),0)
            v,window,path=start("reopened")
            wait_until(lambda:"LIVE-OUTPUT" in report(path).get("text",""))
            cmd("type","--window",window,"reopened")
            expected+=b"reopened"
            wait_until(lambda:(root/"input").read_bytes()==expected)
            assert (root/"reader-pid").read_text()==pid
            close_window(window,env);v.wait(timeout=5)
            print("PASS: Qt/Ghostty snapshot + live output, IME commit-only, native keys, drag/word/line selection, copy/bracketed paste, application mouse/wheel reporting with Shift bypass, scrollback, PTY resize, owner rejection and same-PID reopen")
        finally:
            if (root/"input").exists():
                (artifacts/"input.bin").write_bytes((root/"input").read_bytes())
            subprocess.run([str(runtime),"stop",str(sock)],capture_output=True,timeout=12)
            for p in reversed(processes):
                if p.poll() is None:p.terminate()
                try:p.wait(timeout=5)
                except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
