#!/usr/bin/env python3
"""Diagnostic Linux 1/15-runtime parsing profile at fixed 80x24 geometry."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile
import time
from session_smoke import wait_until

ROOT=Path(__file__).resolve().parent.parent
PAYLOAD=64*1024*1024

def sample(binary,count):
    with tempfile.TemporaryDirectory(prefix='wb-vt-scale-') as directory:
        root=Path(directory); helper=root/'paint'; processes=[]
        helper.write_text('#!'+sys.executable+'\n'+r"""
from pathlib import Path
import os,time,tty
root=Path.cwd(); identity=os.environ['WB_SCALE_ID']; tty.setraw(0)
(root/('ready-'+identity)).touch()
while not (root/'start').exists(): time.sleep(.005)
line=b'\x1b[32m'+b'x'*70+b'\x1b[0m\r\n'
data=(line*(67108864//len(line)+1))[:67108864]
offset=0
while offset<len(data): offset+=os.write(1,data[offset:offset+32768])
os.write(1,b'\x1b[0m\r\nDONE-'+identity.encode()+b'\r\n')
(root/('done-'+identity)).touch()
while True: time.sleep(1)
""")
        helper.chmod(0o700)
        with (root/'runtime.log').open('w+') as log:
            try:
                for i in range(count):
                    processes.append(subprocess.Popen([binary,str(root/f'p{i}.sock'),str(helper)],cwd=root,stdout=log,stderr=log,env=dict(os.environ,WB_SCALE_ID=str(i))))
                wait_until(lambda: all((root/f'ready-{i}').exists() for i in range(count)),timeout=15)
                def ticks(process): return sum(map(int,Path(f'/proc/{process.pid}/stat').read_text().split(') ',1)[1].split()[11:13]))
                baseline=sum(ticks(p) for p in processes); started=time.monotonic(); (root/'start').touch()
                wait_until(lambda: all((root/f'done-{i}').exists() for i in range(count)),timeout=60)
                for i in range(count):
                    wait_until(lambda: f'DONE-{i}'.encode() in subprocess.check_output([binary,'read',str(root/f'p{i}.sock')],timeout=5),timeout=15)
                elapsed=time.monotonic()-started
                cpu=(sum(ticks(p) for p in processes)-baseline)/os.sysconf('SC_CLK_TCK')
                rss=[]
                for process in processes:
                    values=Path(f'/proc/{process.pid}/status').read_text().splitlines()
                    rss.append(int(next(line for line in values if line.startswith('VmRSS:')).split()[1]))
                return {'runtimes':count,'geometry':'80x24','bytes_per_runtime':PAYLOAD,'wall_seconds':round(elapsed,3),'runtime_cpu_seconds':round(cpu,3),'cpu_seconds_per_runtime':round(cpu/count,3),'rss_kib_per_runtime':rss}
            finally:
                for i,process in enumerate(processes):
                    if process.poll() is None:
                        subprocess.run([binary,'stop',str(root/f'p{i}.sock')],capture_output=True,timeout=12)
                        if process.poll() is None: process.terminate()
                        process.wait(timeout=5)


def main():
    if sys.platform!='linux': raise SystemExit('This diagnostic profile uses Linux /proc; run native Mac measurements separately')
    binary=str(Path(sys.argv[1]).resolve())
    results=[sample(binary,n) for n in (1,15)]
    artifact=ROOT/'.local/test-results/runtime-core-scale.json'; artifact.parent.mkdir(exist_ok=True)
    artifact.write_text(json.dumps(results,indent=2)+'\n')
    for result in results: print(json.dumps(result))
    print(f'Diagnostic output: {artifact}. No wall-clock pass/fail threshold or pre-change comparison.')

if __name__=='__main__': main()
