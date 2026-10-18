"""Real PTY input remains ordered and attached across small-key and paste bursts."""
from pathlib import Path
import os
import shlex
import struct
import subprocess
import sys
import tempfile
from session_smoke import attach, send, read_frame, wait_until

runtime=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='wb-input-burst-',dir='/tmp') as directory:
    root=Path(directory);path=root/'terminal.sock';client=None
    expected=bytes(i%251 for i in range(2048))+b'paste-'*20000
    reader=root/'reader.py'
    reader.write_text("import os,tty\nfrom pathlib import Path\ntty.setraw(0)\nPath('pid').write_text(str(os.getpid()))\nos.write(1,b'BURST-READY')\nresult=bytearray()\nwhile True:\n result.extend(os.read(0,65536))\n if len(result)>="+str(len(expected))+":Path('input').write_bytes(result)\n")
    with (root/'runtime.log').open('w+') as log:
        process=subprocess.Popen([str(runtime),str(path),'/bin/sh'],cwd=root,stdout=log,stderr=log)
        try:
            wait_until(path.exists);client,_,_=attach(path)
            send(client,16,(shlex.join(['exec',sys.executable,str(reader)])+'\n').encode())
            wait_until((root/'pid').exists)
            pid=int((root/'pid').read_text())
            packets=b''.join(struct.pack('!BI',16,1)+bytes([value]) for value in expected[:2048])
            for start in range(2048,len(expected),65536):
                data=expected[start:start+65536];packets+=struct.pack('!BI',16,len(data))+data
            client.sendall(packets)
            wait_until((root/'input').exists,timeout=10)
            assert (root/'input').read_bytes()==expected,'lost/reordered/duplicate input'
            assert (root/'pid').read_text()==str(pid);os.kill(pid,0)
            send(client,17,struct.pack('!HH',91,29))
            while True:
                kind,payload=read_frame(client)
                assert kind!=4,payload
                if kind==2 and payload==struct.pack('!HH',91,29):break
            assert 'PTY input queue unavailable' not in (root/'runtime.log').read_text()
            print('PASS: 2048 separate key frames plus 120 KiB paste, exact ordered delivery, retained reader PID/attachment and subsequent resize')
        except BaseException:
            log.flush();print((root/'runtime.log').read_text(),file=sys.stderr);raise
        finally:
            if client:client.close()
            subprocess.run([str(runtime),'stop',str(path)],capture_output=True,timeout=10)
            process.wait(timeout=5)
