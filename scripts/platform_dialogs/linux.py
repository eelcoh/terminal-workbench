"""Linux desktop folder chooser; no shell, no terminal/file mutation."""
import os
import shutil
import subprocess

def choose(initial):
    zenity=shutil.which('zenity')
    if not zenity:
        raise ValueError('Folder browsing needs Zenity on Linux; enter a folder manually')
    result=subprocess.run([zenity,'--file-selection','--directory','--title=Choose project folder',
        '--filename='+str(initial)+os.sep],capture_output=True,text=True)
    if result.returncode==1:return None
    if result.returncode:
        raise ValueError('Could not open the Linux folder picker; enter a folder manually')
    return result.stdout.removesuffix('\n')
