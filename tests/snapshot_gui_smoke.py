#!/usr/bin/env python3
"""Real viewer restores beyond the journal cap and falls back to old control endpoints."""
from pathlib import Path
import os, select, socket, subprocess, sys, tempfile, threading, time
from session_smoke import wait_until
from gui_smoke import close_window

def main():
    binary,viewer_binary=[str(Path(value).resolve()) for value in sys.argv[1:3]]
    with tempfile.TemporaryDirectory(prefix='wb-snapshot-gui-') as directory:
        root=Path(directory);path=root/'session.sock';processes=[]
        helper=root/'fixture'
        helper.write_text('#!/usr/bin/env python3\n'+r"""
import os, select, time, tty
from pathlib import Path
tty.setraw(0)
Path('pid').write_text(str(os.getpid()))
os.write(1,b'\x1b[2J\x1b[HFALLBACK_VISIBLE')
while not Path('flood').exists():time.sleep(.01)
for _ in range(544):os.write(1,b'\0'*32768)
os.write(1,b'\x1b[2J\x1b[HPRIMARY_RESTORED\x1b[?1049h\x1b[2J\x1b[HRESTORED_ALTERNATE\x1b[31')
Path('ready').touch()
while True:
    key=os.read(0,1)
    if key==b'c':os.write(1,b'm LIVE_AFTER_RESTORE\x1b[0m')
    elif key==b'q':
        os.write(1,b'\x1b[6n');reply=b''
        while not reply.endswith(b'R') and select.select([0],[],[],3)[0]:reply+=os.read(0,1)
        Path('reply').write_bytes(reply)
        if select.select([0],[],[],.2)[0]:Path('extra').write_bytes(os.read(0,4096))
    elif key==b'x':os.write(1,b'\x1b[?1049l')
""")
        helper.chmod(0o700)
        with (root/'gui.log').open('w+') as log:
            try:
                read_fd,write_fd=os.pipe()
                xvfb=subprocess.Popen(['Xvfb','-displayfd',str(write_fd),'-screen','0','1280x900x24','-nolisten','tcp'],pass_fds=(write_fd,),stdout=log,stderr=log);processes.append(xvfb);os.close(write_fd)
                assert select.select([read_fd],[],[],5)[0]
                display=os.read(read_fd,32).decode().strip();os.close(read_fd)
                environment=dict(os.environ,DISPLAY=':'+display,LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_SOCKET=str(path))
                environment.pop('WAYLAND_DISPLAY',None);environment.pop('WORKBENCH_WORKSPACES',None)
                runtime=subprocess.Popen([binary,str(path),str(helper)],cwd=root,stdout=log,stderr=log);processes.append(runtime)
                wait_until(path.exists);wait_until(lambda:(root/'pid').exists());pid=int((root/'pid').read_text())
                def command(*args):return subprocess.run(['xdotool',*args],env=environment,capture_output=True,text=True,timeout=5)
                def launch(session):
                    env=dict(environment,WORKBENCH_SOCKET=str(session))
                    viewer=subprocess.Popen([viewer_binary],cwd=root,env=env,stdout=log,stderr=log);processes.append(viewer)
                    def find():
                        assert viewer.poll() is None,'viewer exited'
                        result=command('search','--onlyvisible','--pid',str(viewer.pid))
                        return result.stdout.strip().splitlines()[-1] if result.returncode==0 else None
                    window=wait_until(find,timeout=10);command('windowfocus',window).check_returncode();time.sleep(.5)
                    return viewer,window
                def copy_row(window,end):
                    command('mousemove','--window',window,'6','44').check_returncode();command('mousedown','1').check_returncode();time.sleep(.06)
                    command('mousemove','--window',window,str(4+end*7+5),'44').check_returncode();time.sleep(.06);command('mouseup','1').check_returncode();time.sleep(.06)
                    command('keydown','--window',window,'ctrl+shift+c').check_returncode();time.sleep(.06);command('keyup','--window',window,'ctrl+shift+c').check_returncode();time.sleep(.06)
                    return subprocess.check_output(['xclip','-o','-selection','clipboard'],env=environment,timeout=5).strip()
                def key(window,value):
                    command('type','--window',window,'--delay','20',value).check_returncode();time.sleep(.2)
                # Model an older .ctl endpoint: it consumes the unknown method
                # then closes, including the Linux reset with unread revision bytes.
                old=root/'old.sock';old.symlink_to(path)
                listener=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);listener.bind(str(old.with_suffix('.ctl')));listener.listen()
                def reject():
                    while True:
                        try:connection,_=listener.accept()
                        except OSError:return
                        connection.settimeout(1)
                        try:connection.recv(5)
                        except OSError:pass
                        connection.close()
                threading.Thread(target=reject,daemon=True).start()
                viewer,window=launch(old)
                assert copy_row(window,15)==b'FALLBACK_VISIBLE'
                log.flush();assert 'legacy replay fallback' in (root/'gui.log').read_text()
                close_window(window,environment);assert viewer.wait(timeout=5)==0;time.sleep(.2)
                (root/'flood').touch();wait_until(lambda:(root/'ready').exists(),timeout=15)
                wait_until(lambda:b'RESTORED_ALTERNATE' in subprocess.check_output([binary,'read',str(path)],timeout=5))
                viewer,window=launch(path)
                assert copy_row(window,17)==b'RESTORED_ALTERNATE'
                # Clearing selection returns to the terminal's current cursor.
                command('mousemove','--window',window,'400','100');command('click','1');time.sleep(.1)
                key(window,'c');assert b'LIVE_AFTER_RESTORE' in copy_row(window,36)
                command('mousemove','--window',window,'400','100');command('click','1');time.sleep(.1)
                other,other_window=launch(path)
                log.flush();assert 'This terminal is open in another window.' in (root/'gui.log').read_text()
                close_window(other_window,environment);assert other.wait(timeout=5)==0
                command('windowfocus',window).check_returncode()
                # Rejection of the second viewer must leave the first connection usable.
                key(window,'q');wait_until(lambda:(root/'reply').exists())
                reply=(root/'reply').read_bytes();assert reply.startswith(b'\x1b[1;') and reply.endswith(b'R'),reply
                time.sleep(.3);assert not (root/'extra').exists(),'duplicate historical/live query reply'
                key(window,'x');assert copy_row(window,15)==b'PRIMARY_RESTORED'
                close_window(window,environment);assert viewer.wait(timeout=5)==0;time.sleep(.2)
                viewer,window=launch(path);assert copy_row(window,15)==b'PRIMARY_RESTORED'
                os.kill(pid,0);close_window(window,environment);assert viewer.wait(timeout=5)==0
                subprocess.run([binary,'stop',str(path)],check=True,timeout=12);assert runtime.wait(timeout=5)==0
                listener.close()
            finally:
                for process in reversed(processes):
                    if process.poll() is None:
                        process.terminate()
                        try:process.wait(timeout=5)
                        except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
                if sys.exc_info()[0]:log.flush();print((root/'gui.log').read_text(),file=sys.stderr)
    print('PASS: real viewer legacy fallback, >16 MiB snapshot restore, clipboard, incomplete parser continuation, query ownership, second-window rejection without displacing the first, alternate/primary return and same-PID reconnect')
if __name__=='__main__':main()
