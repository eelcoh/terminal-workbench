#!/usr/bin/env python3
"""Live font changes, window overrides and density transitions without restarting shells."""
from pathlib import Path
import fcntl,os,select,shutil,struct,subprocess,sys,tempfile,termios,time
from gui_smoke import close_window
from session_smoke import attach,send,wait_until
ROOT=Path(__file__).resolve().parent.parent

def main():
    runtime,viewer=[str(Path(x).resolve()) for x in sys.argv[1:3]];total=int(sys.argv[3])
    with tempfile.TemporaryDirectory(prefix='wb-text-') as directory:
        root=Path(directory);processes=[];pids={};inodes={};scale=root/'density';scale.write_text('1\n')
        config=root/'config';config.write_text('workbench-config-v1\n[font]\nfamily = JetBrains Mono\nsize = 16\n')
        rows=['workbench-catalog-v3','W\tmain\tFonts']
        for p in range(min(total,8)):rows.append(f'T\tmain\tp{p}\tTerminal {p+1}\t{root}/p{p}.sock')
        rows.append('W\tbackground\tBackground')
        for p in range(8,total):rows.append(f'T\tbackground\tp{p}\tTerminal {p+1}\t{root}/p{p}.sock')
        (root/'catalog').write_text('\n'.join(rows)+'\n')
        with (root/'test.log').open('w+') as log:
            try:
                for p in range(total):
                    sock=root/f'p{p}.sock';server=subprocess.Popen([runtime,str(sock),'/bin/sh'],cwd=root,stdout=log,stderr=log);processes.append(server);wait_until(sock.exists)
                    connection,_,_=attach(sock);send(connection,16,f'keep=p{p}; echo $$ > p{p}.pid\n'.encode());connection.close()
                    pids[p]=int(wait_until(lambda:(root/f'p{p}.pid').exists() and (root/f'p{p}.pid').read_text().strip()));inodes[p]=sock.stat().st_ino
                rd,wr=os.pipe();xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(xvfb);os.close(wr)
                assert select.select([rd],[],[],5)[0]
                with os.fdopen(rd) as ready:display=ready.readline().strip()
                env=dict(os.environ,DISPLAY=':'+display,LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(root/'catalog'),WORKBENCH_CONFIG=str(config),WORKBENCH_RENDER_SCALE_FILE=str(scale),WORKBENCH_WINDOW_STATE=str(root/'view'))
                for name in ['WAYLAND_DISPLAY','WORKBENCH_SOCKET','WORKBENCH_CATALOG_SCRIPT','WORKBENCH_RENDER_SCALE','WORKBENCH_SHORTCUTS','WORKBENCH_THEME','WORKBENCH_UI_APPEARANCE']:env.pop(name,None)
                def cmd(*args):
                    out=subprocess.run(['xdotool',*map(str,args)],env=env,capture_output=True,text=True,timeout=5);out.check_returncode();return out.stdout.strip()
                def start(view='view'):
                    process=subprocess.Popen([viewer],cwd=root,env=dict(env,WORKBENCH_WINDOW_STATE=str(root/view)),stdout=log,stderr=log);processes.append(process)
                    def find():
                        assert process.poll() is None
                        out=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],env=env,capture_output=True,text=True)
                        return out.stdout.strip().splitlines()[-1] if out.returncode==0 else None
                    window=wait_until(find);cmd('windowfocus',window);time.sleep(.6);return process,window
                process,window=start()
                def key(value):cmd('keydown','--window',window,value);time.sleep(.07);cmd('keyup','--window',window,value);time.sleep(.15)
                def click(x,y):cmd('mousemove','--window',window,x,y);cmd('mousedown',1);time.sleep(.06);cmd('mouseup',1);time.sleep(.25)
                def text(value):cmd('type','--clearmodifiers','--window',window,'--delay',5,'--',value)
                def palette(query):key('ctrl+shift+p');text(query);key('Return');time.sleep(.3)
                def geometry(p=0):
                    fd=os.open(f'/proc/{pids[p]}/fd/0',os.O_RDONLY|os.O_NOCTTY)
                    try:return struct.unpack('HHHH',fcntl.ioctl(fd,termios.TIOCGWINSZ,bytes(8)))[:2]
                    finally:os.close(fd)
                def safe(p,name):
                    text(f'printf "%s:%s" "$$" "$keep" > {name}');key('Return')
                    assert wait_until(lambda:(root/name).exists() and (root/name).read_text())==f'{pids[p]}:p{p}'
                def saved(line,view='view'):return wait_until(lambda:(root/view).exists() and line in (root/view).read_text())
                def capture(tag):
                    started=time.time_ns();key('F12');image=wait_until(lambda:next((p for p in root.glob('screenshot*.png') if p.stat().st_mtime_ns>=started),None))
                    target=ROOT/'.local/test-results'/f'font-settings-{total}-{tag}.png';target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(image,target);key('ctrl+u');return target
                safe(0,'initial');baseline=geometry();assert baseline==(41,82),baseline
                # Default shortcuts change logical grids and are recorded in the help registry.
                key('ctrl+shift+equal');wait_until(lambda:geometry()[0]==38);assert geometry()!=baseline
                saved('F 17 0\n');safe(0,'zoomed')
                key('ctrl+shift+minus');wait_until(lambda:geometry()==baseline)
                key('ctrl+shift+equal');key('ctrl+shift+0');wait_until(lambda:geometry()==baseline);saved('F 0 0\n')
                # Live family/size preview doesn't get persisted by periodic save, and Cancel restores.
                palette('Terminal text settings');click(756,295)
                # Dialog 560x346 is centered at (340,207); Fira y=279..313, plus x=534..568/y=333..365.
                click(550,349);time.sleep(1.2);saved('F 0 0\n');capture('fira-preview')
                assert geometry()[0]==38
                key('Escape');wait_until(lambda:geometry()==baseline);safe(0,'cancelled')
                palette('Terminal text settings');click(756,295);click(550,349);click(810,521)
                saved('F 17 2\n');assert geometry()[0]==38;chosen=geometry();safe(0,'applied')
                # Unicode primary + fallback still render after the face switch.
                text("printf '\\033[2J\\033[H▐▛███▛█  ← ❯ ⏵ ◐ ✻ 📦  café Ελληνικά\\n'");key('Return');capture('fira-terminal')
                # Selection maps pointer cells using the new face's metrics.
                text("printf '\\033[2J\\033[HFONT_COPY_TEST\\n'");key('Return');time.sleep(.2)
                cell_width=575//chosen[1]
                cmd('mousemove','--window',window,60,94);cmd('mousedown',1);time.sleep(.08)
                cmd('mousemove','--window',window,58+14*cell_width+2,94);time.sleep(.08);cmd('mouseup',1);key('ctrl+shift+c')
                copied=subprocess.check_output(['xclip','-selection','clipboard','-o'],env=env,timeout=5).decode()
                assert copied=='FONT_COPY_TEST',repr(copied)
                click(200,200);safe(0,'after-copy')
                palette('Terminal width: one third');assert geometry()[1]<chosen[1];safe(0,'third-width')
                palette('Terminal width: one half');wait_until(lambda:geometry()==chosen)
                if total>1:
                    key('ctrl+shift+Right');safe(1,'next-at-zoom');key('ctrl+shift+Left');safe(0,'previous-at-zoom')
                # Retained help and overview/navigation canvases refresh at the selected logical size.
                key('ctrl+shift+slash');capture('help')
                for density in [2,1]:
                    scale.write_text(str(density)+'\n')
                    wait_until(lambda:f'Display backing scale updated: {density};' in (root/'test.log').read_text());time.sleep(.3)
                    assert geometry()==chosen
                key('Escape');key('ctrl+shift+o');capture('overview');key('ctrl+shift+o');time.sleep(.5)
                safe(0,'after-density');assert geometry()==chosen
                if total>8:
                    key('ctrl+shift+Down');safe(8,'background-font');assert geometry(8)[0]==38
                    key('ctrl+shift+Up');safe(0,'returned')
                close_window(window,env);assert process.wait(timeout=5)==0
                process,window=start();assert geometry()==chosen;safe(0,'restored');saved('F 17 2\n')
                # Explicit window choices win; a fresh slot inherits edited managed defaults.
                close_window(window,env);assert process.wait(timeout=5)==0
                config.write_text('workbench-config-v1\n[font]\nfamily = Fira Code\nsize = 20\n[linux.font]\nsize = 18\n')
                process,window=start('other-view');safe(0,'fresh-default');assert geometry()[0]==36
                palette('Terminal text settings');click(390,521);click(810,521);saved('F 0 0\n','other-view')
                # Compact geometry still exposes Apply/Cancel and blocks key leakage.
                cmd('windowsize',window,800,300);time.sleep(.3);palette('Terminal text settings');key('Tab');key('Escape');safe(0,'compact-cancel')
                for p,pid in pids.items():os.kill(pid,0);assert (root/f'p{p}.sock').stat().st_ino==inodes[p]
                close_window(window,env);assert process.wait(timeout=5)==0
                assert config.read_text().endswith('[linux.font]\nsize = 18\n')
                print(f'PASS ({total}): text shortcuts/grid resize/reset, bundled Fira preview/cancel/apply, per-window restore/default inheritance, 1→2→1 density stability, help/Overview/background workspace/compact modal, same shell PID/socket/state')
            except Exception:
                log.flush();print((root/'test.log').read_text()[-4500:],file=sys.stderr);raise
            finally:
                for p in reversed(processes):
                    if p.poll() is None:p.terminate();p.wait(timeout=5)
if __name__=='__main__':main()
