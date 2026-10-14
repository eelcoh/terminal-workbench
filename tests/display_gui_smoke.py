#!/usr/bin/env python3
"""Density transitions and horizontal strip input with live disposable shells."""
from pathlib import Path
import fcntl,json,os,select,shutil,subprocess,sys,tempfile,termios,time
from gui_smoke import close_window
from session_smoke import attach,send,wait_until
from chrome_gui_actions import position
from theme_gui_smoke import pixel
from navigation_gui_smoke import PANE_COLORS
ROOT=Path(__file__).resolve().parent.parent

def main():
    runtime,viewer=[str(Path(x).resolve()) for x in sys.argv[1:3]];total=int(sys.argv[3]);active=min(total,8)
    with tempfile.TemporaryDirectory(prefix='wb-display-') as directory:
        root=Path(directory);processes=[];pids={};inodes={};scale=root/'density';scale.write_text('1\n')
        rows=['workbench-catalog-v3','W\tmain\tDisplay test']
        for p in range(active):rows.append(f'T\tmain\tp{p}\tTerminal {p+1}\t{root}/p{p}.sock')
        rows.append('W\tbackground\tBackground')
        for p in range(active,total):rows.append(f'T\tbackground\tp{p}\tTerminal {p+1}\t{root}/p{p}.sock')
        (root/'catalog').write_text('\n'.join(rows)+'\n')
        with (root/'test.log').open('w+') as log:
            try:
                for p in range(total):
                    sock=root/f'p{p}.sock';server=subprocess.Popen([runtime,str(sock),'/bin/sh'],cwd=root,stdout=log,stderr=log);processes.append(server);wait_until(sock.exists)
                    connection,_,_=attach(sock);red,green,blue=PANE_COLORS[p%8]
                    send(connection,16,f'keep=p{p}; echo $$ > p{p}.pid; printf "\\033]11;#{red:02x}{green:02x}{blue:02x}\\007\\033[2J\\033[H"\n'.encode());connection.close()
                    pids[p]=int(wait_until(lambda:(root/f'p{p}.pid').exists() and (root/f'p{p}.pid').read_text().strip()));inodes[p]=sock.stat().st_ino
                rd,wr=os.pipe();xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(xvfb);os.close(wr)
                assert select.select([rd],[],[],5)[0]
                with os.fdopen(rd) as ready:display=ready.readline().strip()
                env=dict(os.environ,DISPLAY=':'+display,LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(root/'catalog'),WORKBENCH_RENDER_SCALE_FILE=str(scale),WORKBENCH_WINDOW_STATE=str(root/'view'))
                for name in ['WAYLAND_DISPLAY','WORKBENCH_SOCKET','WORKBENCH_CATALOG_SCRIPT','WORKBENCH_RENDER_SCALE']:env.pop(name,None)
                def cmd(*args):
                    out=subprocess.run(['xdotool',*map(str,args)],env=env,capture_output=True,text=True,timeout=5);out.check_returncode();return out.stdout.strip()
                wait_until(lambda:subprocess.run(['xdotool','getdisplaygeometry'],env=env,capture_output=True).returncode==0)
                process=subprocess.Popen([viewer],cwd=root,env=env,stdout=log,stderr=log);processes.append(process)
                def find():
                    assert process.poll() is None
                    out=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],env=env,capture_output=True,text=True)
                    return out.stdout.strip().splitlines()[-1] if out.returncode==0 else None
                window=wait_until(find);cmd('windowfocus',window);time.sleep(.5)
                def key(value):cmd('keydown','--window',window,value);time.sleep(.07);cmd('keyup','--window',window,value);time.sleep(.08)
                def click(x,y):cmd('mousemove','--window',window,x,y);cmd('mousedown',1);time.sleep(.06);cmd('mouseup',1);time.sleep(.2)
                def verify(p,name):
                    cmd('type','--clearmodifiers','--window',window,'--delay',5,'--',f'printf "%s:%s" "$$" "$keep" > {name}');key('Return')
                    value=wait_until(lambda:(root/name).exists() and (root/name).read_text());assert value==f'{pids[p]}:p{p}',(name,value,p)
                def geometry():
                    result={}
                    for p,pid in pids.items():
                        fd=os.open(f'/proc/{pid}/fd/0',os.O_RDONLY|os.O_NOCTTY)
                        try:result[p]=fcntl.ioctl(fd,termios.TIOCGWINSZ,bytes(8))
                        finally:os.close(fd)
                    return result
                def capture(tag):
                    started=time.time_ns();key('F12');image=wait_until(lambda:next((p for p in root.glob('screenshot*.png') if p.stat().st_mtime_ns>=started),None))
                    target=ROOT/'.local/test-results'/f'display-{total}-{tag}.png';target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(image,target);key('ctrl+u');return target
                for p in range(active):click(position(p,active),20);verify(p,f'initial-{p}')
                click(position(0,active),20);time.sleep(1.35);baseline=geometry();image=capture('1x')
                def borders(image):
                    edge=pixel(image,54,200)
                    assert edge!=PANE_COLORS[0], 'outline matches terminal background'
                    assert all(pixel(image,x,y)==edge for x,y in [(300,50),(636,200),(300,749)]), 'a pane outline edge disappeared'
                borders(image)
                # Retained help and pane textures must refresh, without changing terminal grids or parsing state.
                key('super+alt+slash');capture('help-1x')
                for density in [2,1]:
                    scale.write_text(str(density)+'\n')
                    wait_until(lambda:f'Display backing scale updated: {density};' in (root/'test.log').read_text());time.sleep(.4)
                    assert geometry()==baseline
                    capture(f'help-transition-{density}x')
                key('Escape');time.sleep(.3);verify(0,'after-scale');borders(capture('returned-1x'))
                assert geometry()==baseline
                # A physical 1x window resize is not a density change and must not reload fonts.
                changes=(root/'test.log').read_text().count('Display backing scale updated:')
                cmd('windowsize',window,900,500);time.sleep(.4);cmd('windowsize',window,1240,760);time.sleep(.4)
                assert (root/'test.log').read_text().count('Display backing scale updated:')==changes
                assert geometry()==baseline;verify(0,'after-resize')
                if active>2:
                    cmd('mousemove','--window',window,800,200);cmd('click',7);time.sleep(.3)
                    first=capture('swipe-partial');assert pixel(first,620,700)==PANE_COLORS[1], 'horizontal swipe snapped back'
                    verify(0,'small-swipe-focus')
                    cmd('mousemove','--window',window,800,200);cmd('click','--repeat',15,'--delay',25,7);time.sleep(.3)
                    verify(1,'swipe-next-focus');assert geometry()==baseline
                    cmd('click','--repeat',16,'--delay',25,6);time.sleep(.3);verify(1,'swipe-return-retains-visible-focus')
                    assert pixel(capture('swipe-return'),220,700)==PANE_COLORS[0]
                    click(position(0,active),20);time.sleep(.3)
                    # Vertical wheel still belongs to the terminal, not pane navigation.
                    cmd('mousemove','--window',window,800,200);cmd('click',5);time.sleep(.2);verify(0,'vertical-keeps-focus')
                    # Horizontal input behind a modal cannot move the strip.
                    key('super+alt+slash');cmd('click','--repeat',16,'--delay',25,7);key('Escape');verify(0,'modal-keeps-focus')
                else:
                    cmd('mousemove','--window',window,300,200);cmd('click',7);verify(0,'single-boundary')
                for p,pid in pids.items():os.kill(pid,0);assert (root/f'p{p}.sock').stat().st_ino==inodes[p]
                close_window(window,env);assert process.wait(timeout=5)==0
                print(f'PASS ({total}): dynamic 1→2→1 atlases/help/canvases, all four pane borders, stable logical cells/PID/socket/state, resize without font reload, horizontal pan/focus/boundaries, vertical and modal input isolation')
            except Exception:
                log.flush();print((root/'test.log').read_text()[-4500:],file=sys.stderr);raise
            finally:
                for p in reversed(processes):
                    if p.poll() is None:p.terminate();p.wait(timeout=5)
if __name__=='__main__':main()
