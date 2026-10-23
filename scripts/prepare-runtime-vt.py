#!/usr/bin/env python3
"""Build the pinned runtime VT library without a GUI toolkit or a Ghostty install."""
from pathlib import Path
import importlib.util
import os
import subprocess
import sys

ROOT=Path(__file__).resolve().parent.parent
REVISION='f64f4aca2c29b554d111b36c3d946a9bddd159ff'
REPOSITORY='git@github.com:ghostty-org/ghostty.git'
spec=importlib.util.spec_from_file_location('workbench_build',ROOT/'scripts/build-runtime.py')
build=importlib.util.module_from_spec(spec); spec.loader.exec_module(build)

def pinned(source):
    if not (source/'.git').exists(): return False
    result=subprocess.run(['git','-C',str(source),'rev-parse','HEAD'],capture_output=True,text=True)
    return result.returncode==0 and result.stdout.strip()==REVISION and not subprocess.check_output(['git','-C',str(source),'status','--porcelain','--untracked-files=no']).strip()

def main():
    try:
        compiler,environment=build.build_environment(ROOT,os.environ)
        source=ROOT/'.local/deps/ghostty-vt'
        if not source.exists():
            source.parent.mkdir(parents=True,exist_ok=True)
            subprocess.run(['git','clone',REPOSITORY,str(source)],check=True)
        subprocess.run(['git','-C',str(source),'remote','set-url','origin',REPOSITORY],check=True)
        if not pinned(source):
            if subprocess.check_output(['git','-C',str(source),'status','--porcelain','--untracked-files=no']).strip():
                raise RuntimeError('Runtime Ghostty checkout has local changes; preserve them before updating it')
            subprocess.run(['git','-C',str(source),'checkout','--detach',REVISION],check=True)
        subprocess.run(['git','-C',str(source),'remote','set-url','origin',REPOSITORY],check=True)
        destination=ROOT/'.local/runtime-vt'
        subprocess.run([str(compiler),'build','-Demit-lib-vt','-Doptimize=ReleaseFast','--prefix',str(destination)],cwd=source,env=environment,check=True)
        print(f'Runtime libghostty-vt {REVISION} prepared at {destination}',flush=True)
    except (OSError,RuntimeError,subprocess.CalledProcessError) as error:
        print(f'Runtime VT build failed: {error}',file=sys.stderr); return 1
    return 0

if __name__=='__main__': sys.exit(main())
