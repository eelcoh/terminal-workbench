#!/usr/bin/env python3
"""Independent real window views survive close, crash and catalog edits."""
from pathlib import Path
import os
import select
import signal
import subprocess
import sys
import tempfile
import time
from gui_smoke import close_window
from session_smoke import wait_until, attach, send
from launch_diagnostics_test import launch
ROOT=Path(__file__).resolve().parent.parent

def main():
    runtime,viewer=[str(Path(arg).resolve()) for arg in sys.argv[1:3]]
    with tempfile.TemporaryDirectory(prefix='wb-view-') as directory:
        root=Path(directory); processes=[]; viewers=[]; catalog=root/'workspaces.tsv'
        rows=['workbench-catalog-v3','W\tmain\tMain']
        for p in range(4): rows.append(f'T\tmain\tp{p}\tPane {p}\t{root}/p{p}.sock')
        rows+=['W\tempty\tEmpty','W\tother\tOther',f'T\tother\tp4\tPane 4\t{root}/p4.sock']
        catalog.write_text('\n'.join(rows)+'\n')
        with (root/'test.log').open('w+') as log:
            try:
                pids=[]; inodes=[]
                for p in range(5):
                    sock=root/f'p{p}.sock'
                    process=subprocess.Popen([runtime,str(sock),'/bin/sh'],cwd=root,stdout=log,stderr=log);processes.append(process)
                    wait_until(sock.exists); connection,_,_=attach(sock)
                    send(connection,16,f'keep=p{p}; echo $$ > p{p}.pid\n'.encode())
                    pids.append(wait_until(lambda:(root/f'p{p}.pid').exists() and (root/f'p{p}.pid').read_text().strip()))
                    inodes.append(sock.stat().st_ino);connection.close()
                read_fd,write_fd=os.pipe()
                xvfb=subprocess.Popen(['Xvfb','-displayfd',str(write_fd),'-screen','0','1600x1000x24','-noreset','-nolisten','tcp'],pass_fds=(write_fd,),stdout=log,stderr=log)
                processes.append(xvfb);os.close(write_fd);assert select.select([read_fd],[],[],5)[0]
                env=dict(os.environ,DISPLAY=':'+os.read(read_fd,32).decode().strip(),LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(catalog))
                os.close(read_fd)
                for name in ('WAYLAND_DISPLAY','WORKBENCH_SOCKET','WORKBENCH_CATALOG_SCRIPT','WORKBENCH_STATE_DIR','WORKBENCH_WINDOW_STATE','WORKBENCH_WINDOW_SLOT_FD'):env.pop(name,None)
                def cmd(*args):
                    result=subprocess.run(['xdotool',*map(str,args)],env=env,capture_output=True,text=True,timeout=5)
                    result.check_returncode();return result.stdout.strip()
                def opened(slot):
                    result=launch(Path(viewer),root,dict(env,TERM_PROGRAM='terminal-workbench'),nested=True)
                    assert result.returncode==0,result.stderr
                    assert f'Window view: {slot}.view'.encode() in result.stdout,result.stdout
                    pid=int(result.stdout.split(b'Viewer started independently (pid ')[1].split(b')')[0]);viewers.append(pid)
                    def find():
                        result=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(pid)],env=env,capture_output=True,text=True)
                        return result.stdout.strip().splitlines()[-1] if result.returncode==0 else None
                    window=wait_until(find,timeout=10);cmd('windowraise',window);cmd('windowfocus',window);time.sleep(1.3)
                    return pid,window
                def read(slot):return (root/f'windows/{slot}.view').read_text()
                def saved(slot,check):return wait_until(lambda:(root/f'windows/{slot}.view').exists() and check(read(slot)) and read(slot),timeout=10)
                def click(window,x,y):
                    cmd('windowraise',window);cmd('windowfocus',window);cmd('mousemove','--window',window,x,y)
                    cmd('mousedown',1);time.sleep(.06);cmd('mouseup',1);time.sleep(.2)
                def key(window,name):
                    cmd('keydown','--window',window,name);time.sleep(.06);cmd('keyup','--window',window,name);time.sleep(.2)
                def closed(pid,window):
                    close_window(window,env)
                    wait_until(lambda:not subprocess.run(['xdotool','search','--onlyvisible','--pid',str(pid)],env=env,capture_output=True).returncode==0)
                    viewers.remove(pid);time.sleep(.2)
                first,a=opened(1)
                # Select pane four; first visible follows by identity. Resize and split.
                click(a,405,16)
                cmd('windowsize',a,1360,820);time.sleep(.3)
                cmd('mousemove','--window',a,777,400);cmd('mousedown',1);time.sleep(.08)
                cmd('mousemove','--window',a,650,400);time.sleep(.1);cmd('mouseup',1)
                before=saved(1,lambda s:'V 1360 820 ' in s and 'W main p3 p2 ' in s)
                second,b=opened(2);click(b,70,150)
                second_view=saved(2,lambda s:' empty ' in s.splitlines()[1]);assert ' main ' in read(1).splitlines()[1]
                closed(first,a)
                # Reordering workspaces and terminals preserves focus and first by ID.
                reordered=[rows[0],rows[6],rows[7],rows[8],rows[1],rows[3],rows[2],rows[4],rows[5]]
                replacement=root/'replacement.tsv';replacement.write_text('\n'.join(reordered)+'\n');replacement.replace(catalog)
                first,a=opened(1)
                restored=saved(1,lambda s:'W main p3 p2 ' in s and 'V 1360 820 ' in s)
                assert restored.splitlines()[1].split()[3]==before.splitlines()[1].split()[3]
                cmd('type','--clearmodifiers','--window',a,'--delay',5,'--','printf "%s:%s" "$$" "$keep" > restored');key(a,'Return')
                assert wait_until(lambda:(root/'restored').exists() and (root/'restored').read_text())==pids[3]+':p3'
                assert read(2).splitlines()[1].split()[1:5]==second_view.splitlines()[1].split()[1:5]
                # Smaller geometry makes both overview axes nonzero.
                cmd('windowsize',a,900,500);time.sleep(.3);click(a,518,16)
                spatial=saved(1,lambda s:s.splitlines()[1].split()[6]=='1' and float(s.splitlines()[1].split()[10])>0
                    and any(line.startswith('W main ') and float(line.split()[-1])>0 for line in s.splitlines()))
                closed(first,a);first,a=opened(1)
                restored_spatial=saved(1,lambda s:s.splitlines()[1].split()[6]=='1')
                assert restored_spatial.splitlines()[1].split()[10]==spatial.splitlines()[1].split()[10]
                assert next(line for line in restored_spatial.splitlines() if line.startswith('W main '))==next(line for line in spatial.splitlines() if line.startswith('W main '))
                # Search context is restored together with its coordinates.
                key(a,'ctrl+a');cmd('type','--clearmodifiers','--window',a,'--delay',5,'--','Pane')
                overview=saved(1,lambda s:s.splitlines()[1].split()[6]=='1' and s.splitlines()[1].endswith('50616e65 1'))
                closed(first,a);first,a=opened(1)
                saved(1,lambda s:s.splitlines()[1].split()[6]=='1' and s.splitlines()[1].endswith('50616e65 1'))
                # Crash releases the lease, and restores the latest checkpoint.
                os.kill(first,signal.SIGKILL);viewers.remove(first);time.sleep(.3)
                first,a=opened(1);saved(1,lambda s:s.splitlines()[1].split()[6]=='1')
                closed(first,a)
                # Missing saved IDs and invalid state fall back without touching runtimes.
                catalog.write_text('workbench-catalog-v3\nW\tempty\tEmpty\n')
                first,a=opened(1);saved(1,lambda s:' empty ' in s.splitlines()[1]);closed(first,a)
                (root/'windows/1.view').write_text('workbench-window-v999\nbroken\n')
                first,a=opened(1);saved(1,lambda s:s.startswith('workbench-window-v1\n'));closed(first,a)
                closed(second,b)
                for p in range(5):os.kill(int(pids[p]),0);assert (root/f'p{p}.sock').stat().st_ino==inodes[p]
            finally:
                for pid in viewers:
                    try:os.kill(pid,signal.SIGTERM)
                    except ProcessLookupError:pass
                for process in reversed(processes):
                    if process.poll() is None:process.terminate()
                    try:process.wait(timeout=5)
                    except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
    print('PASS: independent detached window slots, size/split/focus/visible IDs, reorder, same live shell input, overview query restoration, crash checkpoint, deleted IDs and invalid-state fallback')
if __name__=='__main__':main()
