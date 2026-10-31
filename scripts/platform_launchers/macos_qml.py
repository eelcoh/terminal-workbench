"""Trial: let Qt CMake deploy its scanned QML closure into a private prefix."""
import os
from pathlib import Path
import shutil
import tempfile
from .macos_tools import progress,run_tool


def run_import_deployment(script,prefix,output,log):
    environment=dict(os.environ)
    # CMake file(INSTALL) honors DESTDIR independently of the prefix.
    environment.pop('DESTDIR',None)
    run_tool(['cmake','-DCMAKE_INSTALL_PREFIX='+str(prefix),'-DQT_DEPLOY_PREFIX='+str(prefix),
        '-DQT_DEPLOY_QML_DIR=qml','-DQT_DEPLOY_PLUGINS_DIR=plugins','-P',str(script)],
        output,'Deploying scanner-selected QML modules',log,timeout=120,env=environment)


def _merge(source,destination):
    if destination.is_symlink() or (destination.exists() and not destination.is_dir()):destination.unlink()
    destination.mkdir(parents=True,exist_ok=True)
    for path in source.iterdir():
        target=destination/path.name
        if path.is_dir() and not path.is_symlink():_merge(path,target)
        else:
            if target.is_dir() and not target.is_symlink():shutil.rmtree(target)
            else:target.unlink(missing_ok=True)
            shutil.move(path,target)


def materialize(contents,prefix,qt_prefix):
    # Homebrew's merged Qt prefix links each file into its formula's keg with a
    # relative path, and Qt's deployment copies those links verbatim. Replace
    # each one that leaves the deployment with the file it names in Qt's tree.
    bases=[Path(qt_prefix).resolve(),*Path(qt_prefix).resolve().parents]
    for path in sorted(path for path in contents.rglob('*') if path.is_symlink()):
        if path.exists() and path.resolve().is_relative_to(prefix):continue
        target=Path(os.readlink(path));parts=target.parts
        while parts and parts[0]=='..':parts=parts[1:]
        candidates=[target] if target.is_absolute() else [base.joinpath(*parts) for base in bases if parts]
        source=next((candidate.resolve() for candidate in candidates if candidate.exists()),None)
        if source is None:continue
        path.unlink()
        if source.is_dir():shutil.copytree(source,path)
        else:shutil.copy2(source,path)


def deploy(bundle,script,output,log,qt_prefix):
    with tempfile.TemporaryDirectory(prefix='.qt-qml-',dir=bundle.parent) as directory:
        prefix=Path(directory).resolve()
        run_import_deployment(script,prefix,output,log)
        contents=prefix/'band-ui.app/Contents'
        materialize(contents,prefix,qt_prefix)
        qml=contents/'Resources/qml';plugins=contents/'PlugIns'
        if not qml.is_dir() or not plugins.is_dir():raise ValueError('Qt CMake did not produce the expected Mac QML deployment layout')
        for path in contents.rglob('*'):
            if path.is_symlink() and not (path.exists() and path.resolve().is_relative_to(prefix)):
                raise ValueError(f'Qt QML deployment contains an external or dangling symlink: '
                    f'{path.relative_to(prefix)} -> {os.readlink(path)}')
        modules=sorted(str(path.parent.relative_to(qml)) for path in qml.rglob('qmldir'))
        destination=bundle/'Contents/Resources/qml'
        if destination.is_symlink():destination.unlink()
        elif destination.exists():shutil.rmtree(destination)
        _merge(qml,destination)
        _merge(plugins,bundle/'Contents/PlugIns')
        progress(f'QML deployment selected {len(modules)} modules')
        return dict(mode='qt-cmake-import-closure',modules=modules)
