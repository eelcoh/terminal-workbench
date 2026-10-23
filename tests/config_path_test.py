#!/usr/bin/env python3
"""`band-session config-path` discovery and --init preserve managed files."""
from pathlib import Path
import os,subprocess,sys,tempfile

def main():
    runtime=Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='wb-config-path-') as directory:
        root=Path(directory);home=root/'home';home.mkdir()
        base={k:v for k,v in os.environ.items() if not k.startswith(('XDG_','BAND_'))}
        def run(*args,**env):
            return subprocess.run([str(runtime),'config-path',*args],env=dict(base,HOME=str(home),**env),capture_output=True,text=True,check=True).stdout
        assert run()==f'{home}/.config/band/config.conf\n'
        assert run(XDG_CONFIG_HOME=str(root/'xdg'))==f'{root}/xdg/band/config.conf\n'
        assert not (root/'xdg').exists() and run('--prepare',XDG_CONFIG_HOME=str(root/'xdg')).endswith('/config.conf\n')
        managed=root/'managed';managed.mkdir();link=root/'config-link';link.symlink_to(managed,target_is_directory=True)
        assert run(BAND_CONFIG_DIR=str(link))==f'{link}/config.conf\n'
        config=managed/'config.conf';config.write_text('# managed unchanged\n')
        assert run('--init',BAND_CONFIG_DIR=str(link)).startswith('Existing configuration preserved')
        assert config.read_text()=='# managed unchanged\n'
        config.unlink();target=root/'tracked-config';target.write_text('# tracked\n');config.symlink_to(target)
        run('--init',BAND_CONFIG_DIR=str(link))
        assert config.is_symlink() and target.read_text()=='# tracked\n'
        fresh=root/'fresh'
        assert run('--init',BAND_CONFIG_DIR=str(fresh))==f'{fresh}/config.conf\n'
        assert (fresh/'config.conf').read_text().startswith('band-config-v1\n')
        bad=subprocess.run([str(runtime),'config-path','--bogus'],capture_output=True,text=True)
        assert bad.returncode==2 and 'usage' in bad.stderr
    print('PASS: config discovery (home, XDG, explicit symlinked roots), --prepare, --init example and managed-file/symlink preservation')

if __name__=='__main__':main()
