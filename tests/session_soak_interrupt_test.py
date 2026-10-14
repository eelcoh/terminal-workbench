#!/usr/bin/env python3
"""Ctrl+C during a running workload must stop disposable runtimes and report failure."""
from pathlib import Path
import json,os,select,signal,subprocess,sys,tempfile,time
ROOT=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='wb-soak-stop-') as temporary:
    report=Path(temporary)/'report.json'
    process=subprocess.Popen([sys.executable,str(ROOT/'tests/session_soak.py'),sys.argv[1],
        '--seconds','30','--terminals','1','--report',str(report)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    try:
        deadline=time.monotonic()+15
        while True:
            assert select.select([process.stdout],[],[],max(0,deadline-time.monotonic()))[0],'soak startup stalled'
            line=process.stdout.readline()
            assert line,'soak exited before startup: '+process.stderr.read()
            if line.startswith('Soak started:'):break
        process.send_signal(signal.SIGINT)
        stdout,stderr=process.communicate(timeout=25)
        assert process.returncode!=0 and 'KeyboardInterrupt' in stderr
        result=json.loads(report.read_text())
        assert not result['passed'] and result['error'].startswith('KeyboardInterrupt:')
        assert not result.get('cleanup_errors') and result['resource_samples']
        # The fixture runtime was sampled before interruption. It must be dead;
        # an unreaped zombie is also stopped and holds no session resources.
        for pid in result['resource_samples'][0]:
            status=subprocess.run(['ps','-o','stat=','-p',pid],capture_output=True,text=True,timeout=5)
            assert not status.stdout.strip() or status.stdout.strip().startswith('Z'),'disposable runtime survived Ctrl+C'
        print('PASS: Ctrl+C exits with failure, reports interruption and stops disposable runtimes')
    finally:
        if process.poll() is None:
            process.send_signal(signal.SIGINT)
            try:process.wait(timeout=25)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
