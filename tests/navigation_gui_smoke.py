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
from theme_gui_smoke import pixel
from session_smoke import attach, send, wait_until
ROOT=Path(__file__).resolve().parent.parent


def main():
    runtime, binary = [str(Path(a).resolve()) for a in sys.argv[1:3]]
    total=int(sys.argv[3]) if len(sys.argv)>3 else 15
    active=min(total,8)
    with tempfile.TemporaryDirectory(prefix='wb-nav-') as directory:
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
                    send(connection,16,f'keep=p{p}; echo $$ > p{p}.pid; printf "\\033]11;#{40 if p==0 else 20:02x}{40 if p==1 else 20:02x}{40 if p==2 else 20:02x}\\007\\033[2J\\033[H"; i=0; while [ "$i" -lt 32 ]; do printf "Terminal {p+1} | build output %s | ready\\n" "$i"; i=$((i+1)); done\n'.encode())
                    wait_until(lambda:(root/f'p{p}.pid').exists())
                    pids[p]=int((root/f'p{p}.pid').read_text()); inodes[p]=path.stat().st_ino; connection.close()
                read_fd,write_fd=os.pipe()
                xvfb=subprocess.Popen(['Xvfb','-displayfd',str(write_fd),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(write_fd,),stdout=log,stderr=log); processes.append(xvfb)
                os.close(write_fd); assert select.select([read_fd],[],[],5)[0]
                env=dict(os.environ,DISPLAY=':'+os.read(read_fd,32).decode().strip(),WORKBENCH_WORKSPACES=str(catalog),LIBGL_ALWAYS_SOFTWARE='1')
                os.close(read_fd)
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
                    cmd('mousemove','--window',window,x,y); cmd('mousedown',1,'mouseup',1); time.sleep(pause)
                def key(name):
                    cmd('keydown','--window',window,name); time.sleep(.025); cmd('keyup','--window',window,name); time.sleep(.025)
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
                    key('F12'); time.sleep(.1)
                    image=max(root.glob('screenshot*.png'),key=lambda path:path.stat().st_mtime_ns)
                    artifacts=ROOT/'.local/test-results'; artifacts.mkdir(exist_ok=True)
                    mode='reduced' if env.get('WORKBENCH_REDUCED_MOTION')=='1' else 'animated'
                    target=artifacts/f'nav-{total}-{mode}-{tag}.png'; shutil.copyfile(image,target); key('ctrl+u'); return target
                verify(0,'initial')
                for p in range(active):
                    click(363+p*14,16,.01); verify(p,f'position-{p}'); time.sleep(.2)
                baseline=geometry(); alive(); capture('positions')
                if active>2:
                    # Confirm actual pixel movement, not just a completed focus change.
                    click(363,16,.25); start_image=capture('start')
                    click(377,16,.04); click(286,16,.06); middle=capture('moving')
                    time.sleep(.2); end_image=capture('settled')
                    if env.get('WORKBENCH_REDUCED_MOTION')!='1':
                        assert pixel(middle,220,700)==pixel(start_image,220,700), 'outgoing terminal vanished before the slide'
                        assert pixel(middle,1220,700)==pixel(end_image,1220,700) and pixel(middle,1220,700)!=pixel(start_image,1220,700), 'slide had not advanced before completion'
                    assert pixel(end_image,220,700)!=pixel(start_image,220,700), 'working-view pixels did not move'
                    # Retarget repeatedly before the previous 180ms move finishes.
                    click(363,16,.04); click(286,16,.04); click(286,16,.04); click(222,16,.04); click(222,16,.04)
                    verify(0,'retarget-input')
                    for _ in range(8): assert geometry()==baseline; time.sleep(.025)
                    # Header vertical/horizontal wheel owns navigation; content wheel doesn't.
                    click(377,16,.04)
                    cmd('mousemove','--window',window,335,16); cmd('click',5); time.sleep(.02); verify(2,'wheel-next')
                    time.sleep(.15); cmd('click',6); time.sleep(.02); verify(1,'wheel-horizontal-prev')
                    time.sleep(.15); cmd('click',7); time.sleep(.02); verify(2,'wheel-horizontal-next')
                    cmd('mousemove','--window',window,800,180); cmd('click',5); cmd('click',7); time.sleep(.1); verify(2,'content-wheel-keeps-focus')
                    assert geometry()==baseline
                    # Clicking terminal content during motion settles it before hit testing.
                    click(363+7*14,16,.01); click(300,100,.01); verify(6,'transition-content-click')
                    click(363+7*14,16,.01); verify(7,'last-pane-immediate-input')
                    cmd('mousemove','--window',window,712,300); cmd('mousedown',1); time.sleep(.06)
                    cmd('mousemove','--window',window,900,300); time.sleep(.06); cmd('mouseup',1); time.sleep(.1)
                    click(363,16,.04); click(363+3*14,16,.04); verify(3,'asymmetric-divider-input')
                    capture('asymmetric')
                    cmd('mousemove','--window',window,900,300); cmd('mousedown',1); time.sleep(.06)
                    cmd('mousemove','--window',window,712,300); time.sleep(.06); cmd('mouseup',1); time.sleep(.1)
                    click(363+7*14,16,.04)
                    click(286,16); verify(7,'last-boundary')
                    click(363,16,.01); click(222,16); verify(0,'first-boundary')
                    # Modals and window changes finish motion and preserve their input isolation.
                    click(363+7*14,16,.01); key('ctrl+shift+o'); time.sleep(.25); key('Escape'); time.sleep(.25); verify(7,'overview-return')
                    cmd('windowsize',window,800,300); time.sleep(.25); click(363+3*14,16,.01); verify(3,'narrow-input'); capture('narrow')
                    cmd('windowsize',window,1240,760); time.sleep(.25); click(363+7*14,16); capture('wide')
                else:
                    click(286,16); click(222,16); verify(0,'single-boundary')
                # Empty workspaces have no navigation target and no phantom pane.
                click(60,194); cmd('mousemove','--window',window,335,16); cmd('click',5); click(286,16); click(363,16)
                click(60,106); click(363,16); verify(0,'empty-return')
                time.sleep(.3)
                ticks_per_second=os.sysconf('SC_CLK_TCK')
                def ticks(): return sum(map(int,Path(f'/proc/{viewer.pid}/stat').read_text().split(') ',1)[1].split()[11:13]))
                start_ticks=ticks(); started=time.monotonic(); time.sleep(2)
                cpu=(ticks()-start_ticks)/ticks_per_second/(time.monotonic()-started)*100
                alive(); close_window(window,env); assert viewer.wait(timeout=5)==0
                mode='reduced' if env.get('WORKBENCH_REDUCED_MOTION')=='1' else 'animated'
                print(f'PASS ({total}, {mode}): positions, rapid retarget/input, header-only wheel, terminal scrolling, pointer settlement, boundaries, overview, narrow/empty states and unchanged PID/socket/settled geometry; 2s idle CPU={cpu:.1f}%')
            except Exception:
                log.flush(); print((root/'test.log').read_text()[-6000:],file=sys.stderr); raise
            finally:
                for process in reversed(processes):
                    if process.poll() is None: process.terminate(); process.wait(timeout=5)


if __name__=='__main__': main()
