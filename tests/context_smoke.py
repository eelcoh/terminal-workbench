#!/usr/bin/env python3
"""Local-process context follows real shells and Git without attaching a viewer."""
from pathlib import Path
import json
import os
import shlex
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from session_smoke import attach, send, read_frame, wait_until, expect

binary = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix='wb-context-test-') as directory:
    root=Path(directory)
    repo=root/'repository λ'; repo.mkdir()
    def git(*args):
        return subprocess.run(['git','-C',str(repo),*args],check=True,capture_output=True,text=True).stdout.strip()
    git('init','-b','main');git('-c','user.name=Test','-c','user.email=test@example.invalid','commit','--allow-empty','-m','initial')
    sub=repo/'src';sub.mkdir()
    tree=root/'topic tree';git('worktree','add','-b','topic',str(tree))
    fake=root/'bin';fake.mkdir();marker=root/'unexpected-git'
    (fake/'git').write_text('#!/bin/sh\ntouch '+shlex.quote(str(marker))+'\nexit 1\n');(fake/'git').chmod(0o700)
    path=root/'session.sock';control=path.with_suffix('.ctl')
    log=(root/'server.log').open('w+')
    server=subprocess.Popen([binary,str(path),'/bin/sh'],cwd=repo,stderr=log,
        env=dict(os.environ,PATH=str(fake)+':'+os.environ['PATH']))
    connection=None;watch=None;other=None
    try:
        wait_until(control.exists)
        def context():
            return json.loads(subprocess.run([binary,'context',str(path)],capture_output=True,text=True,check=True,timeout=4).stdout)
        def until(**expected):
            def match():
                current=context()
                return current if all(current[key]==value for key,value in expected.items()) else None
            return wait_until(match,timeout=7)
        until(cwd=str(repo),repository=repo.name,branch='main',worktree='',detached=False)
        watch=socket.socket(socket.AF_UNIX);watch.settimeout(7);watch.connect(str(control));watch.sendall(b'CTX1\n')
        kind,payload=read_frame(watch);assert kind==10 and payload[:2]==b'\x01\x01'
        connection,_,sizes=attach(path);assert sizes[-1]==(80,24)
        send(connection,16,b"printf '\\nCONTEXT_PID:%s\\n' \"$$\"\n")
        pid=expect(connection,rb'\r?\nCONTEXT_PID:(\d+)\r?\n').group(1)
        def cd(folder):
            send(connection,16,('cd '+shlex.quote(str(folder))+'\n').encode())
        cd(sub);until(cwd=str(sub),branch='main')
        # Cwd remains unchanged while branch metadata changes from another process.
        git('checkout','-b','feature/context');until(cwd=str(sub),branch='feature/context')
        cd(tree);until(cwd=str(tree),repository=repo.name,branch='topic',worktree=tree.name)
        subprocess.run(['git','-C',str(tree),'checkout','--detach'],check=True,capture_output=True)
        until(detached=True,branch=git('rev-parse','--short=8','HEAD'))
        # A foreground process may use a different directory from its parent shell.
        send(connection,16,('sh -c '+shlex.quote('cd '+shlex.quote(str(root))+'; sleep 4')+'\n').encode())
        until(cwd=str(root),repository='',branch='')
        until(cwd=str(tree),detached=True)
        # Remote clients must not advertise their local directory as remote context.
        shutil.copy('/bin/sleep',fake/'ssh')
        send(connection,16,(shlex.quote(str(fake/'ssh'))+' 4\n').encode())
        until(available=False,source='remote-unavailable',cwd='')
        until(cwd=str(tree),detached=True)
        cd(root);until(cwd=str(root),repository='',worktree='')
        # Screen text/OSC cannot overwrite trusted process-owned context.
        send(connection,16,b"printf '\\033]7;file://fake/forged\\007\\n'\n")
        time.sleep(1.1);assert context()['cwd']==str(root)
        other=socket.socket(socket.AF_UNIX);other.settimeout(5);other.connect(str(path));assert read_frame(other)[0]==4
        other.close();other=None
        # Detached readers continue following the same runtime without taking ownership.
        connection.close();connection=None
        assert context()['cwd']==str(root)
        os.kill(int(pid),0);assert not marker.exists(),'context discovery spawned git'
        connection,_,sizes=attach(path)
        send(connection,16,b"printf '\\nCONTEXT_PID:%s\\n' \"$$\"\n")
        assert expect(connection,rb'\r?\nCONTEXT_PID:(\d+)\r?\n').group(1)==pid
        assert sizes[-1]==(80,24)
        assert all(read_frame(watch)[0]==10 for _ in range(2))
        print('PASS: process cwd, branch changes, linked worktree, detached HEAD, independent watch, unchanged shell/grid, no Git subprocess or OSC spoofing')
    finally:
        for client in (connection,watch,other):
            if client:client.close()
        subprocess.run([binary,'stop',str(path)],capture_output=True,timeout=10)
        try:server.wait(timeout=5)
        except subprocess.TimeoutExpired:server.kill();server.wait()
        log.close()
