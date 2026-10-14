"""Read bounded runtime evidence. Missing/corrupt evidence never blocks recovery."""
import json,os,stat,time

def read_record(socket):
    descriptor=None
    try:
        descriptor=os.open(socket.with_suffix('.lifecycle'),os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
        metadata=os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_uid!=os.getuid() or metadata.st_mode&0o022 or metadata.st_size>4096:return None
        value=json.loads(os.read(descriptor,4097))
        if not isinstance(value,dict) or not {'version','event','runtime_pid','shell_pid','started_ms','exit_code','signal'}.issubset(value):return None
        if type(value['version']) is not int or value['version']!=1:return None
        if not isinstance(value.get('event'),str) or value['event'] not in {'launching','starting','running','exited','stopped','runtime_error','startup_failed'}:return None
        if type(value.get('started_ms')) is not int or value['started_ms']<1:return None
        if value['event']=='launching':
            if value.get('runtime_pid') is not None:return None
        elif type(value.get('runtime_pid')) is not int or value['runtime_pid']<1:return None
        if value.get('shell_pid') is not None and (type(value['shell_pid']) is not int or value['shell_pid']<1):return None
        code,signal=value.get('exit_code'),value.get('signal')
        if code is not None and (type(code) is not int or not 0<=code<=0xffffffff):return None
        if signal is not None and (not isinstance(signal,str) or not signal or len(signal)>80 or any(ord(ch)<32 or ord(ch)==127 for ch in signal)):return None
        if code is not None and signal is not None:return None
        if value['event'] in {'exited','stopped'} and code is None and signal is None:return None
        if value['event'] in {'launching','starting','running'} and (code is not None or signal is not None):return None
        return value
    except (OSError,ValueError,UnicodeError,RecursionError):return None
    finally:
        if descriptor is not None:os.close(descriptor)

def detail(socket,state):
    if state=='running':return {'reason':'running','message':'Shell is running.'}
    if state!='stopped':return {'reason':'unavailable','message':'Runtime status unavailable. Check again.'}
    value=read_record(socket)
    if value is None:return {'reason':'unknown','message':'Runtime stopped; no exit reason was recorded.'}
    event=value['event']
    if event=='launching':reason,message='unknown','Runtime stopped; no exit reason was recorded for this launch.'
    elif event=='stopped':reason,message='stopped','Stopped by Workbench.'
    elif event=='startup_failed':reason,message='startup_failed','Shell startup failed.'
    elif event=='starting':reason,message='startup_incomplete','Runtime disappeared; shell startup was not recorded.'
    elif event=='running':reason,message='missing_exit','Runtime disappeared; no shell exit was recorded.'
    elif event=='runtime_error':reason,message='runtime_error','Runtime failed while handling the terminal.'
    elif value['signal'] is not None:reason,message='signal','Shell ended by signal: '+value['signal']+'.'
    elif value['exit_code']==0:reason,message='exited','Shell exited normally (code 0).'
    else:reason,message='exited',f"Shell exited with code {value['exit_code']}."
    return {'reason':reason,'message':message,'record':value}

