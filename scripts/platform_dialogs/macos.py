"""macOS Standard Additions folder chooser, arguments kept out of script source."""
from pathlib import Path
import subprocess

def choose(initial):
    script=Path(__file__).with_name('choose-folder.applescript')
    result=subprocess.run(['/usr/bin/osascript',str(script),str(initial)],capture_output=True,text=True)
    if result.returncode:
        raise ValueError('Could not open the Mac folder picker; enter a folder manually')
    reply=result.stdout.removesuffix('\n')
    if reply=='CANCEL':return None
    if not reply.startswith('PICK\t'):raise ValueError('Invalid folder-picker reply')
    return reply[5:]
