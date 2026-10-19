"""Real zsh startup from a Finder-like PATH; no user startup files are changed."""
import os
from pathlib import Path
import shlex
import shutil
import socket
import subprocess
import sys
import tempfile
from session_smoke import attach,send,wait_until,expect,read_frame

runtime=Path(sys.argv[1]).resolve();zsh=shutil.which('zsh')
if not zsh:raise SystemExit('Install zsh to run the login-shell startup check')
with tempfile.TemporaryDirectory(prefix='wb-login-',dir='/tmp') as directory:
    root=Path(directory)
    for name,arguments,login in (
        ('explicit',[zsh],False),
        ('login',['--login-shell',zsh],True),
        ('default',[],sys.platform=='darwin'),
    ):
        case=root/name;case.mkdir();home=case/'home';home.mkdir();dot=case/'dotfiles';dot.mkdir()
        tools=case/'profile tools';tools.mkdir();trace=case/'trace';lookups=case/'lookups';proof=case/'proof'
        for tool in ('pyenv','atuin','zoxide'):
            path=tools/tool;path.write_text('#!/bin/sh\nexit 0\n');path.chmod(0o755)
        # A typical .zprofile supplies the package-manager path needed by .zshrc.
        (dot/'.zprofile').write_text('print -r -- profile >> '+shlex.quote(str(trace))+'\nexport PATH='+shlex.quote(str(tools))+':"$PATH"\n')
        (dot/'.zshrc').write_text("PROMPT='WB-READY> '\n"+'print -r -- rc >> '+shlex.quote(str(trace))+'\nfor tool in pyenv atuin zoxide; do\n print -r -- "$tool=$(whence -p "$tool")" >> '+shlex.quote(str(lookups))+'\ndone\n')
        (dot/'.zlogin').write_text('print -r -- login >> '+shlex.quote(str(trace))+'\n')
        env={key:value for key,value in os.environ.items() if key not in ('ZDOTDIR','ENV','BASH_ENV') and not key.startswith(('WORKBENCH_','PYENV_','ATUIN_','ZOXIDE_','XDG_'))}
        env.update(HOME=str(home),ZDOTDIR=str(dot),PATH='/usr/bin:/bin',SHELL=zsh)
        path=case/'session.sock'
        with (case/'runtime.log').open('w+') as log:
            server=subprocess.Popen([str(runtime),str(path),*arguments],cwd=home,env=env,stdout=log,stderr=log)
            connection=None
            try:
                wait_until(path.exists);connection,output,_=attach(path)
                if b'WB-READY> ' not in output:expect(connection,rb'WB-READY> ')
                command="printf '%s:%s:%s:%s\\n' \"$$\" \"$TERM_PROGRAM\" \"${options[login]}\" \"${options[interactive]}\" > "+shlex.quote(str(proof))+'\n'
                send(connection,16,command.encode());wait_until(lambda:proof.exists() and len(proof.read_text().strip().split(':'))==4)
                pid,program,is_login,is_interactive=proof.read_text().strip().split(':')
                assert program=='terminal-workbench' and is_interactive=='on'
                assert is_login==('on' if login else 'off'),(name,proof.read_text())
                assert trace.read_text().splitlines()==(['profile','rc','login'] if login else ['rc'])
                resolved=dict(line.split('=',1) for line in lookups.read_text().splitlines())
                for tool in ('pyenv','atuin','zoxide'):
                    assert (resolved[tool]==str(tools/tool))==login,(name,resolved)
                original=trace.read_bytes();connection.close();connection=None
                def reconnect():
                    # Closing a viewer is asynchronous. Retry only the expected
                    # ownership rejection, closing each rejected socket promptly.
                    candidate=socket.socket(socket.AF_UNIX);candidate.settimeout(5)
                    ready=False
                    try:
                        candidate.connect(str(path))
                        while True:
                            kind,payload=read_frame(candidate)
                            if kind==4:
                                assert payload==b'a window is already attached',payload
                                return None
                            if kind==3:
                                ready=True
                                return candidate
                    finally:
                        if not ready:candidate.close()
                connection=wait_until(reconnect)
                again=case/'again';send(connection,16,('print -r -- "$$" > '+shlex.quote(str(again))+'\n').encode())
                wait_until(lambda:again.exists() and again.read_text().strip()==pid)
                assert trace.read_bytes()==original,'reattachment reran shell startup files'
            except AssertionError as error:
                snapshot=subprocess.run([str(runtime),'read',str(path)],capture_output=True,text=True,timeout=5)
                raise AssertionError((name,str(error),snapshot.stdout,snapshot.stderr,trace.read_text() if trace.exists() else 'no startup trace')) from error
            finally:
                if connection:connection.close()
                if server.poll() is None:
                    subprocess.run([str(runtime),'stop',str(path)],capture_output=True,timeout=15)
                    server.wait(timeout=10)
print('PASS: real zsh with minimal PATH, login profile before interactive rc, profile-provided tool lookup, explicit/default platform policy and same-PID reattachment without repeated startup')
