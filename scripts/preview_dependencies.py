"""One-shot preview dependency evidence; no process tree, user state or service I/O."""
import json
from pathlib import Path
import sys


def python_record():
    return dict(executable=sys.executable,resolved_executable=str(Path(sys.executable).resolve()),
        version='.'.join(map(str,sys.version_info[:3])),bundled=False)


def audit_record(payload):
    path=payload/'qt-dependencies.json'
    try:
        if path.is_symlink():raise ValueError('Qt dependency audit must not be a symlink')
        with path.open('rb') as file:data=file.read(4*1024*1024+1)
        if len(data)>4*1024*1024:raise ValueError('Qt dependency audit exceeds 4 MiB')
        value=json.loads(data)
        if not isinstance(value,dict) or value.get('schema')!=1:raise ValueError('Unsupported Qt dependency audit')
        links=value.get('external_non_qt')
        if not isinstance(links,list) or not all(isinstance(link,str) for link in links):
            raise ValueError('Qt dependency audit has invalid external links')
        return dict(available=True,external_non_qt_links=sorted(set(links)))
    except FileNotFoundError:return dict(available=False,reason='No Qt dependency audit was generated')
    except (OSError,ValueError) as error:return dict(available=False,error=str(error))


def summarize(payload,metadata,report,*,platform=None):
    platform=sys.platform if platform is None else platform
    payload=payload.resolve()
    if platform=='darwin':
        from platform_launchers.macos import system_dependency_path
        boundary=payload.parents[2]
        source='dyld loaded images from the viewer startup check'
    elif platform.startswith('linux'):
        from platform_launchers.linux import system_dependency_path
        boundary=payload
        source='/proc/self/maps files from the viewer startup check (includes non-library mappings)'
    else:raise ValueError('Dependency reporting supports Mac and Linux')
    raw=report.get('loaded_images') if isinstance(report,dict) else None
    observed=dict(available=False,source=source)
    if isinstance(raw,list) and raw and all(isinstance(path,str) for path in raw):
        external=set();unclassified=set();internal=set();system=set()
        for text in raw:
            path=Path(text)
            if not path.is_absolute():unclassified.add(text);continue
            try:resolved=path.resolve()
            except (OSError,RuntimeError):unclassified.add(text);continue
            if resolved.is_relative_to(boundary):internal.add(str(resolved))
            elif system_dependency_path(resolved):system.add(str(resolved))
            else:external.add(str(resolved))
        observed.update(available=True,bundled_path_count=len(internal),system_path_count=len(system),
            outside_preview_paths=sorted(external),unclassified_paths=sorted(unclassified))
    else:observed['reason']='No valid viewer mapping evidence was returned'
    built=metadata.get('python') if isinstance(metadata,dict) else None
    build_python=dict(available=False)
    if isinstance(built,dict) and isinstance(built.get('executable'),str) and isinstance(built.get('version'),str):
        build_python=dict(available=True,executable=built['executable'],version=built['version'])
    return dict(schema=1,python=python_record(),build_python=build_python,
        qt_declared_bundled=metadata.get('qt_bundled') is True,viewer_mappings=observed,
        build_audit=audit_record(payload),
        scope='Observed viewer startup mappings, current Python and recorded build links only. Lazy libraries, Python module dependencies and shell children are not inventoried. No portability assessment.')
