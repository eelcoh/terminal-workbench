#!/usr/bin/env python3
"""Compiler selection contract using executable fixtures, without downloads."""
from pathlib import Path
import importlib.util
import os
import sys
import tempfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('build_spike', ROOT/'scripts/build-spike.py')
build = importlib.util.module_from_spec(spec); spec.loader.exec_module(build)


def fixture(path, version):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f'#!{sys.executable}\nprint({version!r})\n')
    path.chmod(0o700)
    return path


def main():
    with tempfile.TemporaryDirectory(prefix='wb build env ') as directory:
        root = Path(directory)
        system_zig = fixture(root/'bin/zig', '0.17.0')
        environment = {'PATH': str(system_zig.parent), 'SHELL': '/bin/zsh'}
        for system, machine, target in [('Darwin','arm64','aarch64-macos'), ('Darwin','x86_64','x86_64-macos'), ('Linux','x86_64','x86_64-linux')]:
            local = fixture(root/f'.local/tools/zig-{target}-0.16.0/zig', '0.16.0')
            with patch.object(build.platform,'system',return_value=system), patch.object(build.platform,'machine',return_value=machine):
                compiler, child = build.build_environment(root, environment)
            assert compiler == local and child['ZIG'] == str(local)
            assert child['PATH'].split(os.pathsep)[0] == str(local.parent)
            assert child['SHELL'] == '/bin/zsh' and environment == {'PATH': str(system_zig.parent), 'SHELL': '/bin/zsh'}
        custom = fixture(root/'custom compiler/zig-pinned', '0.16.0')
        compiler, child = build.build_environment(root,dict(environment,ZIG=str(custom)))
        assert compiler == custom and child['ZIG'] == str(custom)
        # Explicit overrides are authoritative: an invalid one must not silently fall back.
        for override in [str(system_zig), str(root/'missing')]:
            try: build.build_environment(root,dict(environment,ZIG=override))
            except RuntimeError: pass
            else: raise AssertionError('Invalid explicit override accepted')
        with tempfile.TemporaryDirectory(prefix='wb-empty-') as empty:
            try: build.build_environment(Path(empty),environment)
            except RuntimeError as error: assert '0.16.0' in str(error)
            else: raise AssertionError('Wrong PATH version accepted')
            fixture(system_zig, '0.16.0')
            compiler, _ = build.build_environment(Path(empty),environment)
            assert compiler == system_zig
    for args, system, expected in [([], 'Linux', 'viewer'), (['--wayland'], 'Linux', 'viewer-wayland')]:
        with patch.object(sys,'argv',['build-spike.py',*args]), patch.object(build.platform,'system',return_value=system), patch.object(build,'build_environment',return_value=(Path('/zig'),{})), patch.object(build.subprocess,'run') as run:
            assert build.main()==0
            commands=[call.args[0] for call in run.call_args_list]
            configure=next(c for c in commands if c[0]=='cmake' and '-S' in c)
            assert str(build.ROOT/'.local/build'/expected) in configure
            assert ('-DGLFW_BUILD_WAYLAND=ON' in configure)==bool(args)
            assert ('-DGLFW_BUILD_X11=OFF' in configure)==bool(args)
    with patch.object(sys,'argv',['build-spike.py','--wayland']), patch.object(build.platform,'system',return_value='Darwin'), patch.object(build.subprocess,'run') as run:
        assert build.main()==2 and not run.called
    print('PASS: Apple Silicon/Intel/Linux local discovery, PATH fallback, explicit compiler override, version rejection, paths with spaces and child-only environment')


if __name__=='__main__': main()
