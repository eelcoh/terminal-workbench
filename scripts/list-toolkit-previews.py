#!/usr/bin/env python3
"""List managed preview installs and observed runtime/helper references."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import stat
import sys

from installed_paths import session_directory
from preview_activation import inspect,payload_usage

spec=importlib.util.spec_from_file_location('preview_install',Path(__file__).with_name('install-toolkit-preview.py'))
installer=importlib.util.module_from_spec(spec);spec.loader.exec_module(installer)


def record(directory):
    marker=directory/'installation.json'
    metadata=marker.lstat()
    if not stat.S_ISREG(metadata.st_mode) or metadata.st_uid!=os.getuid():
        raise ValueError('Installation record must be a regular file owned by you')
    value=json.loads(marker.read_text())
    if not isinstance(value,dict) or value.get('schema')!=1:raise ValueError('Unsupported installation record')
    relative=value.get('payload')
    if not isinstance(relative,str) or not relative or Path(relative).is_absolute():raise ValueError('Invalid payload path')
    payload=(directory/relative).resolve()
    if not payload.is_relative_to(directory.resolve()):raise ValueError('Payload path escapes installation')
    preview_path=payload/'preview.json'
    if preview_path.is_symlink():raise ValueError('Payload metadata must not be a symlink')
    preview=json.loads(preview_path.read_text())
    revision=value.get('revision')
    if not isinstance(revision,str) or len(revision)!=40 or any(c not in '0123456789abcdef' for c in revision):
        raise ValueError('Invalid source revision')
    if not isinstance(preview,dict) or preview.get('format')!=1 or preview.get('source')!=revision:
        raise ValueError('Payload metadata does not match installation record')
    return dict(directory=str(directory),payload=str(payload),revision=revision,
        source_dirty=value.get('source_dirty',False),qt_bundled=preview.get('qt_bundled',False),
        platform=preview.get('platform'))


def inventory(root,state):
    root=root.expanduser().resolve();state=state.expanduser().resolve()
    health=inspect(state)
    usage=payload_usage('',health)
    references={row['directory']:row for row in usage['builds']}
    installs=[];invalid=[]
    if root.exists():
        for directory in sorted(root.iterdir()):
            if not directory.name.startswith('preview-') or directory.is_symlink() or not directory.is_dir():continue
            try:row=record(directory)
            except (OSError,ValueError,RuntimeError) as error:
                invalid.append(dict(directory=str(directory),error=str(error)));continue
            refs=references.get(row['payload'],{})
            row.update(service=bool(refs.get('service')),service_available=health is not None,
                runtime_terminals=refs.get('runtime_terminals',[]),helper_terminals=refs.get('helper_terminals',[]))
            installs.append(row)
    managed={row['payload'] for row in installs}
    other=[row for row in usage['builds'] if row['directory'] not in managed]
    return dict(schema=1,installation_root=str(root),state_directory=str(state),service_available=health is not None,
        installations=installs,invalid_installations=invalid,references_outside_root=other,
        unknown_terminals=usage['unknown_terminals'],
        scope='Observed references in this session namespace only; other namespaces and child-program dependencies are not inspected. No removal assessment; installed payloads and session data are unchanged.')


def display(value):
    print('Installed previews: '+value['installation_root'])
    if not value['installations']:print('No managed preview installations found.')
    for row in value['installations']:
        name=Path(row['directory']).name
        if row['source_dirty']:name+=' (dirty build)'
        counts=(f"{len(row['runtime_terminals'])} runtimes, {len(row['helper_terminals'])} helpers" if value['service_available'] else 'references unknown')
        service=('active service' if row['service'] else 'no service reference') if value['service_available'] else 'service unknown'
        print(f"  {name} | {service} | {counts}")
        print('    '+row['payload'])
    for row in value['invalid_installations']:print(f"  Unreadable installation: {row['directory']}: {row['error']}")
    for row in value['references_outside_root']:
        print(f"  Referenced outside install root: {row['directory']} ({'active service; ' if row['service'] else ''}{len(row['runtime_terminals'])} runtimes, {len(row['helper_terminals'])} helpers)")
    if not value['service_available']:print('Session service unavailable; runtime/helper references are unknown.')
    if value['unknown_terminals']:print(f"{len(value['unknown_terminals'])} terminal(s) have incomplete or legacy origin information.")
    print('This list observes references in one namespace; it does not assess whether a build can be removed.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,help='Installation parent (same as installer --root)')
    parser.add_argument('--state-dir',type=Path,help='Session namespace to inspect')
    parser.add_argument('--json',action='store_true',help='Print full machine-readable inventory')
    args=parser.parse_args()
    try:value=inventory(args.root or installer.installation_root(os.environ,sys.platform),args.state_dir or session_directory(os.environ))
    except (OSError,ValueError,RuntimeError) as error:parser.exit(1,f'Preview listing failed: {error}\n')
    if args.json:print(json.dumps(value,indent=2))
    else:display(value)


if __name__=='__main__':main()
