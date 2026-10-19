"""Packaging evidence outside the signed app; sizes count regular-file bytes."""
import json
import time
from .macos_tools import progress


def inventory(bundle):
    groups={name:dict(files=0,bytes=0) for name in ('frameworks','plugins','qml','other')}
    symlinks=0
    for path in bundle.rglob('*'):
        if path.is_symlink():symlinks+=1;continue
        if not path.is_file():continue
        relative=path.relative_to(bundle).parts
        group=('frameworks' if relative[:2]==('Contents','Frameworks') else
               'plugins' if relative[:2]==('Contents','PlugIns') else
               'qml' if relative[:3]==('Contents','Resources','qml') else 'other')
        groups[group]['files']+=1;groups[group]['bytes']+=path.stat().st_size
    return dict(regular_file_bytes=sum(row['bytes'] for row in groups.values()),
                regular_files=sum(row['files'] for row in groups.values()),symlinks=symlinks,groups=groups)


def publish(directory,bundle,started,phases,report,*,signed):
    value=dict(schema=1,phases_seconds=phases,packaging_seconds=time.monotonic()-started,
        signed=signed,size=inventory(bundle),qt_version=report.get('qt_version'),
        deployment_options=report.get('deployment_options',[]),
        scope='Packaging only; excludes compilation. Size is regular-file bytes, not disk allocation or archive size.')
    # Writing into the app after signing would invalidate its resource signature.
    path=directory/'bundle-metrics.json'
    path.write_text(json.dumps(value,indent=2)+'\n')
    summary=', '.join(f'{name} {seconds:.1f}s' for name,seconds in phases.items())
    progress(f'Packaging timings: {summary}')
    progress(f'Bundle: {value["size"]["regular_file_bytes"]/1024/1024:.1f} MiB, {value["size"]["regular_files"]} files. Metrics: {path}')
    return value
