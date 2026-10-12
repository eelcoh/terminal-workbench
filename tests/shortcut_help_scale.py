#!/usr/bin/env python3
"""Wide-workspace navigation, immediate typing and stable PTY geometry in Xvfb."""
from pathlib import Path
import fcntl
import os
import select
import shutil
import subprocess
import sys
import tempfile
import termios
import time
from gui_smoke import close_window
from session_smoke import attach, send, wait_until
ROOT=Path(__file__).resolve().parent.parent
PANE_COLORS=[(40,20,20),(20,40,20),(20,20,40),(40,40,20),(40,20,40),(20,40,40),(50,30,20),(30,30,50)]

def main():
    runtime, binary = [str(Path(a).resolve()) for a in sys.argv[1:3]]
    total=int(sys.argv[3]) if len(sys.argv)>3 else 15
    active=min(total,8)
    with tempfile.TemporaryDirectory(prefix='wb-help-scale-') as directory:
        root=Path(directory); catalog=root/'workspaces.tsv'; processes=[]; pids={}; inodes={}
        rows=['workbench-catalog-v3','W\tmain\tWorkbench']
        for p in range(active): rows.append(f'T\tmain\tp{p}\tTerminal {p+1}\t{root}/p{p}.sock')
        rows.append('W\tarchive\tBackground')
        for p in range(active,total): rows.append(f'T\tarchive\tp{p}\tTerminal {p+1}\t{root}/p{p}.sock')
        rows.append('W\tempty\tEmpty')
        catalog.write_text('\n'.join(rows)+'\n')
        with (root/'test.log').open('w+') as log:
            try:
                for p in range(total):
                    path=root/f'p{p}.sock'
                    server=subprocess.Popen([runtime,str(path),'/bin/sh'],cwd=root,stdout=log,stderr=log); processes.append(server)
                    wait_until(path.exists)
                    connection,_,_=attach(path)
                    red,green,blue=PANE_COLORS[p%8]
                    send(connection,16,f'keep=p{p}; echo $$ > p{p}.pid; printf "\\033]11;#{red:02x}{green:02x}{blue:02x}\\007\\033[2J\\033[H"; i=0; while [ "$i" -lt 32 ]; do printf "Terminal {p+1} | build output %s | ready\\n" "$i"; i=$((i+1)); done\n'.encode())
                    wait_until(lambda:(root/f'p{p}.pid').exists())
                    pids[p]=int((root/f'p{p}.pid').read_text()); inodes[p]=path.stat().st_ino; connection.close()
                read_fd,write_fd=os.pipe()
                xvfb=subprocess.Popen(['Xvfb','-displayfd',str(write_fd),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(write_fd,),stdout=log,stderr=log); processes.append(xvfb)
                os.close(write_fd); assert select.select([read_fd],[],[],5)[0]
                with os.fdopen(read_fd) as ready: display=ready.readline().strip()
                env=dict(os.environ,DISPLAY=':'+display,WORKBENCH_WORKSPACES=str(catalog),WORKBENCH_WINDOW_STATE=str(root/'window.view'),
                    WORKBENCH_SHORTCUTS=str(root/'shortcuts.conf'),LIBGL_ALWAYS_SOFTWARE='1')
                for name in ('WAYLAND_DISPLAY','WORKBENCH_SOCKET','WORKBENCH_CATALOG_SCRIPT','WORKBENCH_THEME','WORKBENCH_STATE_DIR'): env.pop(name,None)
                viewer=subprocess.Popen([binary],env=env,cwd=root,stdout=log,stderr=log); processes.append(viewer)
                def cmd(*args):
                    result=subprocess.run(['xdotool',*map(str,args)],env=env,capture_output=True,text=True,timeout=5)
                    result.check_returncode(); return result.stdout.strip()
                def find():
                    assert viewer.poll() is None
                    result=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(viewer.pid)],env=env,capture_output=True,text=True)
                    return result.stdout.strip().splitlines()[-1] if result.returncode==0 else None
                window=wait_until(find,timeout=10); cmd('windowfocus',window); time.sleep(.4)
                def click(x,y,pause=.2):
                    # Separate synthetic clicks by multiple frames; focus is still checked during motion.
                    cmd('mousemove','--window',window,x,y); cmd('mousedown',1,'mouseup',1); time.sleep(max(.06,pause))
                def key(name):
                    cmd('keydown','--window',window,name); time.sleep(.06); cmd('keyup','--window',window,name); time.sleep(.06)
                def verify(p,name):
                    cmd('type','--clearmodifiers','--window',window,'--delay',5,'--',f'printf "%s:%s" "$$" "$keep" > {name}'); key('Return')
                    actual=wait_until(lambda:(root/name).exists() and (root/name).read_text())
                    assert actual==f'{pids[p]}:p{p}',(name,actual,p)
                def geometry():
                    values={}
                    for p,pid in pids.items():
                        fd=os.open(f'/proc/{pid}/fd/0',os.O_RDONLY|os.O_NOCTTY)
                        try: values[p]=fcntl.ioctl(fd,termios.TIOCGWINSZ,bytes(8))
                        finally: os.close(fd)
                    return values
                def alive():
                    for p,pid in pids.items(): os.kill(pid,0); assert (root/f'p{p}.sock').stat().st_ino==inodes[p]
                def capture(tag):
                    started=time.time_ns();key('F12')
                    image=wait_until(lambda:next((path for path in root.glob('screenshot*.png') if path.stat().st_mtime_ns>=started),None),timeout=5)
                    artifacts=ROOT/'.local/test-results'; artifacts.mkdir(exist_ok=True)
                    mode='reduced' if env.get('WORKBENCH_REDUCED_MOTION')=='1' else 'animated'
                    target=artifacts/f'help-scale-{env.get("WORKBENCH_RENDER_SCALE","1")}x-{total}-{mode}.png'; shutil.copyfile(image,target); key('ctrl+u'); return target
                verify(0,'initial')
                # Equal terminal estate: one full column versus two half columns.
                if active==1: key('ctrl+shift+f');time.sleep(.3)
                baseline=geometry();key('ctrl+shift+slash');key('Tab');time.sleep(.3);capture('help-scale')
                ticks_per_second=os.sysconf('SC_CLK_TCK')
                def ticks():return sum(map(int,Path(f'/proc/{viewer.pid}/stat').read_text().split(') ',1)[1].split()[11:13]))
                started=time.monotonic();start=ticks();time.sleep(2)
                cpu=(ticks()-start)/ticks_per_second/(time.monotonic()-started)*100
                scale=int(env.get('WORKBENCH_RENDER_SCALE','1'))
                log.flush();assert (root/'test.log').read_text().count(f'Texture loaded successfully ({920*scale}x{578*scale}')==1, 'idle help repeatedly allocated its body'
                assert geometry()==baseline;key('ctrl+shift+slash');verify(0,'help-scale-input')
                alive();close_window(window,env);assert viewer.wait(timeout=5)==0
                print(f'PASS ({total}, help at 1240x760 / {scale}x backing): populated panes, retained PID/socket/geometry, input after toggle; 2s idle CPU={cpu:.1f}%')
            except Exception:
                log.flush(); print((root/'test.log').read_text()[-6000:],file=sys.stderr); raise
            finally:
                for process in reversed(processes):
                    if process.poll() is None: process.terminate(); process.wait(timeout=5)


if __name__=='__main__': main()
