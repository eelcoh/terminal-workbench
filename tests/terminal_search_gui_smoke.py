#!/usr/bin/env python3
"""Search real retained output without leaking input or changing PTY ownership."""
from pathlib import Path
import fcntl,os,re,select,shutil,subprocess,sys,tempfile,termios,time
from gui_smoke import close_window
from font_gui_smoke import pixels
from session_smoke import attach,send,wait_until
ROOT=Path(__file__).resolve().parent.parent
runtime,viewer=[str(Path(arg).resolve()) for arg in sys.argv[1:3]]
total=int(sys.argv[3]) if len(sys.argv)>3 else 1
with tempfile.TemporaryDirectory(prefix='wb-search-gui-') as directory:
    root=Path(directory);processes=[];sockets=[];pids=[];rows=['workbench-catalog-v3']
    for w in range((total+7)//8):
        rows.append(f'W\tw{w}\tSearch {w+1}')
        for p in range(w*8,min(total,(w+1)*8)):
            sockets.append(root/f'p{p}.sock');rows.append(f'T\tw{w}\tp{p}\tShell {p+1}\t{sockets[-1]}')
    (root/'catalog').write_text('\n'.join(rows)+'\n')
    with (root/'test.log').open('w+') as log:
        try:
            for p,sock in enumerate(sockets):
                server=subprocess.Popen([runtime,str(sock),'/bin/sh'],cwd=root,stdout=log,stderr=log);processes.append(server);wait_until(sock.exists)
                connection,_,_=attach(sock);send(connection,16,f'keep=search; echo $$ > p{p}.pid\n'.encode());wait_until((root/f'p{p}.pid').exists);pids.append((root/f'p{p}.pid').read_text().strip());connection.close()
            rd,wr=os.pipe();xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(xvfb);os.close(wr)
            assert select.select([rd],[],[],5)[0]
            with os.fdopen(rd) as pipe:display=pipe.readline().strip()
            env=dict(os.environ,DISPLAY=':'+display,LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(root/'catalog'),WORKBENCH_WINDOW_STATE=str(root/'window'),WORKBENCH_REDUCED_MOTION='1')
            for name in ('WAYLAND_DISPLAY','WORKBENCH_SOCKET','WORKBENCH_CONFIG','WORKBENCH_SHORTCUTS','WORKBENCH_CATALOG_SCRIPT'):env.pop(name,None)
            def cmd(*args):return subprocess.run(['xdotool',*map(str,args)],env=env,capture_output=True,text=True,check=True,timeout=5).stdout.strip()
            wait_until(lambda:subprocess.run(['xdotool','getdisplaygeometry'],env=env,capture_output=True).returncode==0)
            process=subprocess.Popen([viewer],cwd=root,env=env,stdout=log,stderr=log);processes.append(process)
            def find():
                assert process.poll() is None
                out=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],env=env,capture_output=True,text=True)
                return out.stdout.strip().splitlines()[-1] if out.returncode==0 else None
            window=wait_until(find);cmd('windowfocus',window);time.sleep(.5)
            def key(value):cmd('keydown','--window',window,value);time.sleep(.07);cmd('keyup','--window',window,value);time.sleep(.1)
            def text(value):cmd('type','--clearmodifiers','--window',window,'--delay',12,'--',value)
            def line(value):text(value);key('Return')
            def capture(name):
                start=time.time_ns();key('F12');image=wait_until(lambda:next((p for p in root.glob('screenshot*.png') if p.stat().st_mtime_ns>=start),None))
                target=ROOT/'.local/artifacts/search'/f'{total}-{env.get("WORKBENCH_RENDER_SCALE","1")}x-{name}.png';target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(image,target);key("ctrl+u");return target
            def geometry():
                result=[]
                for pid in pids:
                    fd=os.open(f'/proc/{pid}/fd/0',os.O_RDONLY|os.O_NOCTTY)
                    try:result.append(fcntl.ioctl(fd,termios.TIOCGWINSZ,bytes(8)))
                    finally:os.close(fd)
                return result
            # Feed literal output through a file so command echo does not add matches.
            specimen=''.join(('find-me café 界\r\n' if i in (4,60,115) else f'plain row {i}\r\n') for i in range(120))
            (root/'specimen').write_text(specimen);line('cat specimen; echo done > ready');wait_until((root/'ready').exists);time.sleep(.2)
            baseline=geometry();inodes=[s.stat().st_ino for s in sockets];before=capture('before')
            def log_text():return (root/'test.log').read_text()
            def search(query):key('ctrl+shift+s');text(query);wait_until(lambda:'Terminal search: 3 matches' in log_text())
            search('find-me');assert 'Terminal search selected: 3/3;' in log_text();matched=capture('matches')
            original_pixels=pixels(before);matched_pixels=pixels(matched)
            assert any(original_pixels[y][58:630]!=matched_pixels[y][58:630] for y in range(200,720)),'Matches were not visibly highlighted'
            key('ctrl+a');key('ctrl+c')
            assert subprocess.check_output(['xclip','-selection','clipboard','-o'],env=env,timeout=5).decode()=='find-me'
            key('End')
            key('Return');wait_until(lambda:'Terminal search selected: 1/3;' in log_text());capture('wrapped-next')
            key('shift+Return');wait_until(lambda:log_text().count('Terminal search selected: 3/3;')>=2)
            # Field clipboard editing and spaces remain owned by search.
            key('ctrl+a');text('touch leaked-search');key('Return');time.sleep(.3);assert not (root/'leaked-search').exists();capture('no-matches')
            key('Escape');after=capture('restored');restored_pixels=pixels(after)
            # Raylib's diagnostic F12 is also encoded to the shell outside overlays.
            # Exclude its prompt echo; compare the actual retained-output reading area.
            assert original_pixels[86:720]==restored_pixels[86:720],'Escape did not restore original terminal view'
            line('printf "%s:%s" "$$" "$keep" > continuity');assert wait_until(lambda:(root/'continuity').exists() and (root/'continuity').read_text())==pids[0]+':search'
            assert geometry()==baseline and [s.stat().st_ino for s in sockets]==inodes
            # Command palette dispatch must consume Enter before editing the search field.
            key('ctrl+shift+p');text('Find in terminal');key('Return');key('ctrl+a')
            subprocess.run(['xclip','-selection','clipboard','-i'],input='café'.encode(),env=env,stdout=subprocess.DEVNULL,stderr=log,check=True,timeout=5)
            key('ctrl+v');time.sleep(.35);key('ctrl+a');key('ctrl+c')
            assert subprocess.check_output(['xclip','-selection','clipboard','-o'],env=env,timeout=5).decode()=='café'
            key('Escape')
            # Output invalidates old highlights/results; next stable scan finds new text.
            line('(sleep 1; cat extra) &');(root/'extra').write_text('new-needle\r\n')
            key('ctrl+shift+s');text('new-needle');time.sleep(1.4);wait_until(lambda:'Terminal search: 1 matches' in log_text());capture('live-output');key('Escape')
            assert geometry()==baseline
            close_window(window,env);assert process.wait(timeout=5)==0
            print(f'PASS ({total}): scrollback search, matching highlights, next/previous wrap, query ownership/clipboard, Commands, live refresh, Escape restoration and retained shell/grid/socket')
        finally:
            for sock in sockets:subprocess.run([runtime,'stop',str(sock)],capture_output=True,timeout=12)
            for process in reversed(processes):
                if process.poll() is None:process.terminate()
                process.wait(timeout=5)
            if sys.exc_info()[0]:log.seek(0);print(log.read(),file=sys.stderr)
