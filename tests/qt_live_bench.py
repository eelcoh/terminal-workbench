"""Fresh-process Qt + real PTY 1/15-terminal CPU/RSS benchmark, Mac/Linux."""
import json
import os
from pathlib import Path
import platform
import socket
import statistics
import subprocess
import sys
import tempfile
import time
from session_smoke import attach,send,wait_until
import shlex

ROOT=Path(__file__).resolve().parent.parent
runtime,viewer=[str(Path(value).resolve()) for value in sys.argv[1:3]]
headless='--headless' in sys.argv[3:]
artifacts=ROOT/'.local/artifacts'/('qt-live-test' if headless else 'qt-live-performance')
artifacts.mkdir(parents=True,exist_ok=True)
scenarios=('idle','one-row','full','one-visible','all-hidden')
SECONDS=1.0;FRAMES=60;REPEATS=3

def atomic(path,data):
    temporary=path.with_suffix('.tmp');temporary.write_text(json.dumps(data));temporary.replace(path)

def process_sample(groups):
    """Checkpoint RSS and CPU; no sampler in the renderer/parser hot paths."""
    pids=[pid for values in groups.values() for pid in values]
    values={}
    if sys.platform.startswith('linux'):
        ticks=os.sysconf('SC_CLK_TCK')
        for pid in pids:
            fields=Path(f'/proc/{pid}/stat').read_text().split(') ',1)[1].split()
            status=Path(f'/proc/{pid}/status').read_text().splitlines()
            rss=int(next(line for line in status if line.startswith('VmRSS:')).split()[1])*1024
            values[pid]={'cpu_seconds':(int(fields[11])+int(fields[12]))/ticks,'rss_bytes':rss,'cpu_resolution_seconds':1/ticks}
    else:
        result=subprocess.check_output(['ps','-o','pid=','-o','time=','-o','rss=','-p',','.join(map(str,pids))],text=True,timeout=10)
        for line in result.splitlines():
            pid,clock,rss=line.split();days=0
            if '-' in clock:day,clock=clock.split('-',1);days=int(day)
            parts=clock.split(':');seconds=days*86400+sum(float(value)*60**index for index,value in enumerate(reversed(parts)))
            resolution=10**(-len(parts[-1].split('.')[1])) if '.' in parts[-1] else 1
            values[int(pid)]={'cpu_seconds':seconds,'rss_bytes':int(rss)*1024,'cpu_resolution_seconds':resolution}
    assert set(values)==set(pids),'a benchmark process disappeared'
    return {name:{'processes':[{"pid":pid,**values[pid]} for pid in ids],
                  'cpu_seconds':sum(values[pid]['cpu_seconds'] for pid in ids),
                  'rss_bytes':sum(values[pid]['rss_bytes'] for pid in ids),
                  'cpu_quantization_bound_seconds':sum(values[pid]['cpu_resolution_seconds'] for pid in ids)} for name,ids in groups.items()}

def sample(count,scale,trial):
    name=f'{count}-{scale or "native"}x-trial{trial}'
    report_path=artifacts/(name+'.json');report_path.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='wb-live-',dir='/tmp') as directory:
        root=Path(directory);servers=[];client=None;connection=None
        with (artifacts/(name+'.log')).open('w') as log:
            try:
                actors=[];sockets=[]
                for identity in range(count):
                    actor=root/str(identity);actor.mkdir();actors.append(actor);sock=actor/'p.sock';sockets.append(sock)
                    servers.append(subprocess.Popen([runtime,str(sock),'/bin/sh'],cwd=actor,stdout=log,stderr=log))
                    wait_until(sock.exists,timeout=15);attached,_,_=attach(sock)
                    command=shlex.join(['exec',sys.executable,str(ROOT/'tests/fixtures/live_terminal_output.py'),str(actor),str(identity)])
                    try:send(attached,16,(command+'\n').encode())
                    finally:attached.close()
                    wait_until((actor/'ready.json').exists,timeout=15)
                control=root/'bench.ctl';config=root/'config.json'
                config.write_text(json.dumps({'control':str(control),'sessions':[str(path.with_suffix('.ctl')) for path in sockets]}))
                env=dict(os.environ,QSG_INFO='1')
                if headless:
                    env.update(QT_QPA_PLATFORM='xcb',LIBGL_ALWAYS_SOFTWARE='1',QSG_RHI_BACKEND='opengl',QT_SCALE_FACTOR=str(scale))
                    for key in ('WAYLAND_DISPLAY','QT_QUICK_BACKEND','QT_SCREEN_SCALE_FACTORS','QT_DEVICE_PIXEL_RATIO'):env.pop(key,None)
                client=subprocess.Popen([viewer,str(config)],cwd=root,env=env,stdout=log,stderr=log)
                wait_until(control.exists,timeout=20)
                connection=socket.socket(socket.AF_UNIX);connection.settimeout(75);connection.connect(str(control));stream=connection.makefile('rb')
                def exchange(data):
                    connection.sendall(json.dumps(data).encode()+b'\n');line=stream.readline()
                    assert line,'benchmark control closed; see '+str(artifacts/(name+'.log'))
                    value=json.loads(line)
                    if 'error' in value:raise AssertionError(value['error'])
                    return value
                def ready():
                    value=exchange({'op':'status'})
                    return value if value.get('ready') else None
                metadata=wait_until(ready,timeout=30)
                assert (metadata['panes'],metadata['columns'],metadata['rows'])==(count,80,24)
                assert metadata['graphics_api'] not in ('unknown','null')
                if headless:assert metadata['graphics_api']=='OpenGL' and metadata['device_pixel_ratio']==scale
                groups={'viewer':[client.pid],'runtimes':[server.pid for server in servers],
                        'producers':[json.loads((actor/'ready.json').read_text())['pid'] for actor in actors]}
                results=[]
                for generation,scenario in enumerate(scenarios):
                    visible=0 if scenario=='all-hidden' else 1 if scenario=='one-visible' else count
                    assert exchange({'op':'visibility','visible':visible})['visibility_ready']
                    before=process_sample(groups);assert exchange({'op':'begin','scenario':scenario})['begun']
                    done=[]
                    if scenario=='idle':time.sleep(SECONDS)
                    else:
                        atomic(root/'job.json',{'generation':generation,'scenario':scenario,'frames':FRAMES,'seconds':SECONDS})
                        wait_until(lambda:all((actor/f'done-{generation}.json').exists() for actor in actors),timeout=60)
                        done=[json.loads((actor/f'done-{generation}.json').read_text()) for actor in actors]
                    result=exchange({'op':'finish','bytes':[value['bytes'] for value in done] if done else [0]*count})
                    after=process_sample(groups)
                    result.update(visible=visible,producer_frames=FRAMES if done else 0,producer_reports=done,processes_before=before,processes_after=after)
                    result['process_cpu_seconds']={key:after[key]['cpu_seconds']-before[key]['cpu_seconds'] for key in groups}
                    assert result['received_bytes']==sum(value['bytes'] for value in done)
                    for item in result['items']:
                        if not item['visible'] or scenario=='idle':assert item['rasterized_rows']==item['paints']==0
                        elif done:assert item['rasterized_rows']>0 and item['paints']>0
                    if done:
                        markers=[value['marker'] for value in done]
                        assert exchange({'op':'verify','markers':markers})['markers_match']
                        for marker,sock in zip(markers,sockets):
                            assert marker.encode() in subprocess.check_output([runtime,'read',str(sock)],timeout=10)
                        result['client_and_runtime_markers_match']=True
                    if visible<count:
                        reveal=exchange({'op':'reveal'})
                        assert reveal['rasterized_rows']==(count-visible)*24,(scenario,reveal)
                        result['reveal']=reveal
                    results.append(result)
                    print(f'  {name} {scenario}: viewer CPU {result["viewer_cpu_seconds"]:.3f}s; wall {result["wall_seconds"]:.3f}s; rows {result["rasterized_rows"]:.0f}; paints {result["paints"]:.0f}',flush=True)
                assert exchange({'op':'capture','path':str(report_path)+'.png'})['captured']
                assert exchange({'op':'quit'})['done'];stream.close();connection.close();connection=None
                deadline=time.monotonic()+10
                while True:
                    pid,status,usage=os.wait4(client.pid,os.WNOHANG)
                    if pid:
                        client.returncode=os.waitstatus_to_exitcode(status);assert client.returncode==0;break
                    if time.monotonic()>deadline:raise AssertionError('benchmark did not quit')
                    time.sleep(.02)
                report={'panes':count,'scale_override':scale,**metadata,'scenarios':results,
                        'viewer_peak_rss_bytes':usage.ru_maxrss if sys.platform=='darwin' else usage.ru_maxrss*1024,
                        'viewer_lifetime_cpu_seconds':usage.ru_utime+usage.ru_stime}
                report_path.write_text(json.dumps(report,indent=2)+'\n')
                return report
            finally:
                if connection:connection.close()
                if client and client.returncode is None:
                    client.terminate()
                    try:client.wait(timeout=5)
                    except subprocess.TimeoutExpired:client.kill();client.wait()
                for sock,server in zip(locals().get('sockets',[]),servers):
                    subprocess.run([runtime,'stop',str(sock)],capture_output=True,timeout=12)
                    try:server.wait(timeout=5)
                    except subprocess.TimeoutExpired:server.kill();server.wait()

def main():
    (artifacts/'report.json').unlink(missing_ok=True)
    try:source={'revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
                'tracked_changes':bool(subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=ROOT,text=True).strip())}
    except subprocess.CalledProcessError:source={'revision':'unknown','tracked_changes':None}
    results=[]
    for scale in ((1,2) if headless else (None,)):
        for count in (1,15):
            for trial in range(1,REPEATS+1):
                print(f'Running {count} real terminals, scale {scale or "native"}, trial {trial}',flush=True)
                results.append(sample(count,scale,trial))
    medians=[]
    for scale in ((1,2) if headless else (None,)):
        for count in (1,15):
            cases=[value for value in results if value['panes']==count and value['scale_override']==scale]
            for index,scenario in enumerate(scenarios):
                entries=[value['scenarios'][index] for value in cases]
                item={'panes':count,'scale_override':scale,'scenario':scenario}
                for key in ('wall_seconds','viewer_cpu_seconds','paint_wall_ms','row_image_bytes','received_bytes'):
                    values=[value[key] for value in entries];item[key]=statistics.median(values);item[key+'_range']=[min(values),max(values)]
                for key in ('viewer','runtimes','producers'):
                    item[key+'_checkpoint_cpu_seconds']=statistics.median(value['process_cpu_seconds'][key] for value in entries)
                    item[key+'_rss_bytes']=statistics.median(value['processes_after'][key]['rss_bytes'] for value in entries)
                medians.append(item)
    path=artifacts/'report.json';path.write_text(json.dumps({'source':source,'headless':headless,'system':platform.platform(),
        'repeats':REPEATS,'seconds_per_workload':SECONDS,'frames_per_producer':FRAMES,'trials':results,'medians':medians},indent=2)+'\n')
    print('PASS: real runtime/client byte and marker agreement; idle/hidden raster and paint isolation; exact reveal catch-up',flush=True)
    print('Artifacts:',artifacts,flush=True)
if __name__=='__main__':main()
