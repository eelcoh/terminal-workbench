#!/usr/bin/env python3
"""Sustained real runtimes, output, handoff, detached queries and service loss."""
from pathlib import Path
import argparse,json,os,re,shlex,signal,socket,struct,subprocess,sys,tempfile,threading,time
from session_smoke import read_frame,send,wait_until
from snapshot_attach_smoke import snapshot_attach,collect
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
import catalog_files
from session_service import request,exchange,Unavailable,ReplyClosed


class Client:
    def __init__(self,path,decoder,takeover=False):
        self.stream=snapshot_attach(path,takeover=takeover)
        started=time.monotonic();snapshot=collect(self.stream);self.latency=time.monotonic()-started
        self.text=subprocess.check_output([str(decoder)],input=snapshot,timeout=5)
        self.sequence=0;self.size=None;self.revoked=threading.Event();self.failure=None;self.closing=False
        self.reader=threading.Thread(target=self.read,daemon=True);self.reader.start()
    def read(self):
        tail=b''
        try:
            while not self.closing:
                kind,payload=read_frame(self.stream)
                if kind==10:self.revoked.set();return
                if kind==2:self.size=struct.unpack('!HH',payload)
                elif kind==1:
                    tail=(tail+payload)[-4096:]
                    matches=re.findall(rb'SOAK:t[0-9]+:([0-9]+)\x1b',tail)
                    if matches:self.sequence=max(self.sequence,*[int(value) for value in matches])
                else:raise AssertionError('Unexpected live frame '+str(kind))
        except (OSError,EOFError,AssertionError,struct.error) as error:
            if not self.closing:self.failure=error
    def healthy(self):
        if self.failure:raise AssertionError('Live stream failed: '+str(self.failure))
        return True
    def close(self):
        self.closing=True
        try:self.stream.shutdown(socket.SHUT_RDWR)
        except OSError:pass
        self.stream.close();self.reader.join(timeout=5)
        assert not self.reader.is_alive(),'client reader leaked'


def record(path):return json.loads(path.read_text())
def alive(pid):
    try:os.kill(pid,0)
    except ProcessLookupError:return False
    return True
def replace(path,value):
    temporary=path.with_name(path.name+'.tmp');temporary.write_text(json.dumps(value));temporary.replace(path)
def resources(pids):
    result={}
    # Batch ps works on Linux and Mac; RSS is observational, not a leak threshold.
    output=subprocess.check_output(['ps','-o','pid=,rss=','-p',','.join(map(str,pids))],text=True,timeout=5)
    for line in output.splitlines():
        pid,rss=map(int,line.split());entry=dict(rss_kib=rss)
        fds=Path('/proc')/str(pid)/'fd'
        if fds.is_dir():entry['fds']=len(list(fds.iterdir()))
        result[str(pid)]=entry
    assert len(result)==len(pids),'runtime disappeared during resource sample'
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime',type=Path)
    parser.add_argument('--seconds',type=int,default=300)
    parser.add_argument('--terminals',type=int,default=15)
    parser.add_argument('--report',type=Path)
    args=parser.parse_args()
    if not 1<=args.seconds<=86400 or not 1<=args.terminals<=64:parser.error('seconds 1..86400; terminals 1..64')
    if args.report is None:args.report=ROOT/f'.local/test-results/session-soak-{args.terminals}.json'
    runtime=args.runtime.resolve();os.environ['SHELL']='/bin/sh'
    summary=dict(version=1,platform=sys.platform,requested_seconds=args.seconds,terminals=args.terminals,passed=False)
    clients={};samples=[];attachments=0;max_latency=0;cycles=0;service_restarts=0
    def sample(pids):
        if len(samples)>=256:del samples[1]  # Retain baseline plus bounded recent observations.
        samples.append(resources(pids))
    with tempfile.TemporaryDirectory(prefix='wb-soak-') as temporary:
        root=Path(temporary).resolve();state=root/'s';state.mkdir(mode=0o700)
        ids=['t'+str(i) for i in range(args.terminals)]
        data=dict(version=1,workspaces=[dict(id='w'+str(start),name='Soak '+str(start),terminals=[dict(id=i,name=i) for i in ids[start:start+8]]) for start in range(0,len(ids),8)])
        paths={i:state/(i+'.sock') for i in ids};shells={};runtimes={};inodes={};sizes={}
        def call(method='initialize',identity=''):return request(state,runtime,root,method,identity)
        decoder=root/'decode'
        command=[os.environ.get('CC','cc'),'-std=c11','-O2','-Wall','-Wextra','-Werror',
            '-I',str(ROOT/'.local/runtime-vt/include'),str(ROOT/'tests/fixtures/session_soak_decode.c'),
            str(ROOT/'crates/runtime/native/terminal_core.c'),str(ROOT/'.local/runtime-vt/lib/libghostty-vt.a'),
            '-o',str(decoder),'-lm','-lpthread']
        if sys.platform.startswith('linux'):command.append('-ldl')
        subprocess.run(command,check=True)
        def attach(identity,takeover=False):
            nonlocal attachments,max_latency
            client=Client(paths[identity],decoder,takeover);attachments+=1;max_latency=max(max_latency,client.latency)
            try:
                assert ('SOAK:'+identity+':').encode() in client.text or cycles==0,'decoded snapshot lost active output'
            except BaseException:
                client.close();raise
            return client
        def service_gone():
            try:exchange(state,dict(protocol=1,method='health'));return False
            except (Unavailable,ReplyClosed,ConnectionResetError,BrokenPipeError):return True
        try:
            catalog_files.save(state,data);call('session.recover');service=call()['service_pid']
            saved=(state/'workspaces.json').read_bytes();(state/'appearance.state').write_bytes(b'soak preferences')
            for identity in ids:
                client=clients[identity]=attach(identity)
                receipt=wait_until(lambda:(state/(identity+'.lifecycle')).exists() and record(state/(identity+'.lifecycle')).get('shell_pid'))
                shells[identity]=receipt;runtimes[identity]=record(state/(identity+'.lifecycle'))['runtime_pid'];inodes[identity]=paths[identity].stat().st_ino
                command='keep='+identity+'; '+shlex.quote(sys.executable)+' '+shlex.quote(str(ROOT/'tests/fixtures/session_soak_workload.py'))+' '+shlex.quote(str(root))+' '+identity+'\n'
                send(client.stream,16,command.encode())
                wait_until(lambda:client.healthy() and client.sequence>0)
            sample(list(runtimes.values()))
            start=time.monotonic();deadline=start+args.seconds;last_notice=start
            print(f'Soak started: {args.terminals} terminals, {args.seconds} seconds',flush=True)
            while time.monotonic()<deadline or cycles<2:
                cycles+=1
                # Close every presentation stream, then query each running TUI.
                for client in clients.values():client.close()
                clients.clear();time.sleep(.15)
                for identity in ids:replace(root/(identity+'.query'),dict(epoch=cycles))
                for identity in ids:
                    reply=wait_until(lambda:(root/(identity+'.reply')).exists() and record(root/(identity+'.reply')).get('epoch')==cycles and record(root/(identity+'.reply')),timeout=10)
                    cols,rows=sizes.get(identity,(80,24))
                    assert reply['cursor']==b'\x1b[4;7R'.hex() and reply['size']==f'\x1b[8;{rows};{cols}t'.encode().hex() and not reply['extra'],'detached query ownership/geometry failed'
                for index,identity in enumerate(ids):
                    owner=clients[identity]=attach(identity)
                    wait_until(lambda:owner.healthy() and owner.sequence>0)
                    replacement=attach(identity,True)
                    # Keep both readers alive until explicit revocation is observed.
                    wait_until(lambda:owner.healthy() and owner.revoked.is_set())
                    try:
                        send(owner.stream,16,b'!');send(owner.stream,17,struct.pack('!HH',10,10))
                    except OSError:pass
                    owner.close();clients[identity]=replacement
                    size=(80+(cycles+index)%20,24+(cycles+index)%8)
                    send(replacement.stream,17,struct.pack('!HH',*size));sizes[identity]=size
                    wait_until(lambda:replacement.healthy() and replacement.size==size)
                    wait_until(lambda:replacement.healthy() and replacement.sequence>0)
                    assert record(root/(identity+'.progress'))['unexpected']==0,'revoked owner input reached the TUI'
                    assert paths[identity].stat().st_ino==inodes[identity]
                    assert record(state/(identity+'.lifecycle'))['shell_pid']==shells[identity]
                if cycles%5 in (1,2):
                    old=service
                    if cycles%5==1:call('service.shutdown')
                    else:os.kill(service,signal.SIGKILL)
                    wait_until(service_gone);service=call()['service_pid'];service_restarts+=1
                    assert service!=old and (state/'workspaces.json').read_bytes()==saved
                    assert (state/'appearance.state').read_bytes()==b'soak preferences'
                    assert all(t['state']=='running' for t in call('health')['terminals'])
                sample(list(runtimes.values()))
                if time.monotonic()-last_notice>=10:
                    print(f'Soak: {time.monotonic()-start:.0f}s, {cycles} cycles, {service_restarts} service restarts',flush=True);last_notice=time.monotonic()
            # Return to the original shells and verify invocation-local variables.
            for identity in ids:
                client=clients[identity];workload=record(root/(identity+'.progress'))['pid']
                # Type the shell command only once the TUI has quit, or it reads both.
                send(client.stream,16,b'X');wait_until(lambda:not alive(workload))
                finish=root/(identity+'.final')
                send(client.stream,16,("printf '%s:%s' \"$$\" \"$keep\" > "+shlex.quote(str(finish))+"\n").encode())
                assert wait_until(lambda:finish.exists() and finish.read_text())==str(shells[identity])+':'+identity
                text=subprocess.check_output([str(runtime),'read',str(paths[identity])],timeout=5)
                assert ('SOAK:'+identity+':').encode() in text,'runtime retained text lost workload'
                assert record(root/(identity+'.progress'))['unexpected']==0
            sample(list(runtimes.values()))
            summary.update(passed=True,elapsed_seconds=round(time.monotonic()-start,3),cycles=cycles,service_restarts=service_restarts,
                           snapshot_attachments=attachments,snapshot_max_seconds=round(max_latency,4),
                           output_bytes=sum(record(root/(i+'.progress'))['bytes'] for i in ids),resource_samples=samples)
        except BaseException as error:
            summary.update(error=type(error).__name__+': '+str(error),cycles=cycles,service_restarts=service_restarts,resource_samples=samples)
            for identity in ids:
                log=state/(identity+'.log')
                if log.exists():print(identity+': '+log.read_text(errors='replace')[-1500:],file=sys.stderr)
            raise
        finally:
            cleanup_errors=[]
            for client in clients.values():
                try:client.close()
                except (OSError,AssertionError) as error:cleanup_errors.append(str(error))
            for path in paths.values():
                try:
                    stopped=subprocess.run([str(runtime),'stop',str(path)],capture_output=True,timeout=15)
                    if stopped.returncode:cleanup_errors.append(path.name+': '+stopped.stderr.decode(errors='replace')[:300])
                except (OSError,subprocess.SubprocessError) as error:cleanup_errors.append(path.name+': '+str(error))
            try:call('service.shutdown')
            except (ValueError,OSError) as error:cleanup_errors.append(str(error))
            if cleanup_errors:summary.update(passed=False,cleanup_errors=cleanup_errors)
            args.report.parent.mkdir(parents=True,exist_ok=True);catalog_files.atomic_write(args.report,json.dumps(summary)+'\n')
            print('Report: '+str(args.report),flush=True)
            if cleanup_errors:raise AssertionError('Disposable cleanup failed: '+str(cleanup_errors))
            if summary['passed']:
                print(f'PASS: {args.terminals} terminals, {cycles} cycles, {service_restarts} service restarts; same shells/variables, snapshots, detached queries, resize, revoked-owner isolation and saved organization',flush=True)


if __name__=='__main__':main()
