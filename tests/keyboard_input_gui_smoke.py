"""Actual GLFW/Ghostty raw input, including same-frame text/key bursts."""
import os, signal, select, shlex, subprocess, sys, tempfile, time, re
from pathlib import Path
runtime,viewer=[Path(arg).resolve() for arg in sys.argv[1:3]]
delay=int(sys.argv[3]) if len(sys.argv)>3 else 12
from session_smoke import attach,send,wait_until
from gui_smoke import close_window
processes=[]
with tempfile.TemporaryDirectory(prefix='wb-raw-input-') as d:
    root=Path(d);sock=root/'session.sock';log=(root/'run.log').open('w')
    try:
        server=subprocess.Popen([str(runtime),str(sock),'/bin/sh'],cwd=root,stdout=log,stderr=log);processes.append(server)
        wait_until(sock.exists);c,_,_=attach(sock)
        reader=root/'reader.py'
        reader.write_text('import os,sys,tty,termios,time,select\nfrom pathlib import Path\ns=termios.tcgetattr(0);tty.setraw(0)\ntry:\n for flags in (0,1,3,31):\n  os.write(1,b"\\x1b[>"+str(flags).encode()+b"u");Path("ready-"+str(flags)).touch();buf=bytearray();started=False\n  while select.select([0],[],[],10 if not started else .5)[0]:\n   buf.extend(os.read(0,8192));started=True\n  Path("bytes-"+str(flags)).write_bytes(buf);os.write(1,b"\\x1b[<u")\nfinally:termios.tcsetattr(0,termios.TCSADRAIN,s)\n')
        send(c,16,('python3 '+shlex.quote(str(reader))+'\n').encode());c.close()
        rd,wr=os.pipe();x=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(x);os.close(wr)
        assert select.select([rd],[],[],5)[0]
        with os.fdopen(rd) as f:display=f.readline().strip()
        (root/'catalog').write_text('workbench-catalog-v3\nW\tmain\tInput\nT\tmain\tsession\tShell\t'+str(sock)+'\n')
        env=dict(os.environ,DISPLAY=':'+display,LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(root/'catalog'),WORKBENCH_WINDOW_STATE=str(root/'window'))
        for name in ('WAYLAND_DISPLAY','WORKBENCH_SOCKET','WORKBENCH_CONFIG','WORKBENCH_SHORTCUTS','WORKBENCH_CATALOG_SCRIPT'):env.pop(name,None)
        def cmd(*a):return subprocess.run(['xdotool',*map(str,a)],env=env,check=True,capture_output=True,text=True).stdout.strip()
        wait_until(lambda:subprocess.run(['xdotool','getdisplaygeometry'],env=env,capture_output=True).returncode==0)
        v=subprocess.Popen([str(viewer)],cwd=root,env=env,stdout=log,stderr=log);processes.append(v)
        def find():
            p=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(v.pid)],env=env,capture_output=True,text=True)
            return p.stdout.strip().splitlines()[-1] if p.returncode==0 else None
        window=wait_until(find);cmd('windowfocus',window);time.sleep(.3)
        expected='/exit: AaBbCc abcdefghijklmnopqrstuvwxyz 0123456789 /exit aaaaa bbbbb ???? !!!!\nlast-line\n'
        for flags in (0,1,3,31):
            wait_until((root/('ready-'+str(flags))).exists)
            # Freeze presentation while injecting to force a whole callback burst.
            os.kill(v.pid,signal.SIGSTOP)
            try:
                parts=expected.split('\n')
                for index,part in enumerate(parts):
                    if part:cmd('type','--clearmodifiers','--window',window,'--delay',delay,'--',part)
                    if index<len(parts)-1:cmd('key','--window',window,'Return')
            finally:os.kill(v.pid,signal.SIGCONT)
            wait_until((root/('bytes-'+str(flags))).exists)
            raw=(root/('bytes-'+str(flags))).read_bytes()
            decoded=[];i=0;saw_release=False
            while i<len(raw):
                m=re.match(rb'\x1b\[([0-9:]+)(?:;([0-9:]*))?(?:;([0-9:]*))?u',raw[i:])
                if m:
                    event=(m[2] or b'1').split(b':')
                    if len(event)>1 and event[1]==b'3':saw_release=True
                    if len(event)<2 or event[1]!=b'3':
                        text=m[3]
                        if text:decoded.extend(chr(int(p)) for p in text.split(b':') if p)
                        else:
                            cp=int(m[1].split(b':')[0])
                            if cp<57344:decoded.append(chr(cp))
                    i+=m.end()
                else:decoded.append(chr(raw[i]));i+=1
            if flags in (3,31):assert saw_release,'negotiated release events missing'
            actual=''.join(decoded)
            assert actual==expected.replace('\n','\r'),(flags,repr(actual),repr(raw))
            print('PASS: rapid input, Kitty flags',flags,'delay',delay,'ms; exact case/repeated characters and Enter order',flush=True)
        close_window(window,env);v.wait(timeout=5)
    finally:
        subprocess.run([str(runtime),'stop',str(sock)],capture_output=True,timeout=12)
        for p in reversed(processes):
            if p.poll() is None:p.terminate()
            try:p.wait(timeout=5)
            except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
        log.close()
