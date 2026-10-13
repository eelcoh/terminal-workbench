"""Portable discovery/migration never rewrites managed files or moves sessions."""
from pathlib import Path
import os,sys,tempfile,subprocess
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from config_paths import config_directory,prepare_preferences,global_appearance

def main():
    with tempfile.TemporaryDirectory(prefix='wb-dotfiles-') as directory:
        root=Path(directory);home=root/'home';home.mkdir();repo=root/'repo';(repo/'.local').mkdir(parents=True)
        sessions=repo/'.local/sessions';sessions.mkdir();env=dict(HOME=str(home))
        assert config_directory(env,'linux')==home/'.config/terminal-workbench'
        assert config_directory(env,'darwin')==home/'Library/Application Support/Terminal Workbench'
        assert config_directory(dict(env,XDG_CONFIG_HOME=str(root/'xdg')),'linux')==root/'xdg/terminal-workbench'
        managed=root/'managed';managed.mkdir();link=root/'config-link';link.symlink_to(managed,target_is_directory=True)
        env['WORKBENCH_CONFIG_DIR']=str(link)
        config=managed/'config.conf';config.write_text('# managed unchanged\n');original=config.read_bytes()
        (repo/'.local/shortcuts-linux.conf').write_text('workbench-shortcuts-v1\nforward=F8\n')
        (repo/'.local/shortcuts-macos.conf').write_text('workbench-shortcuts-v1\nforward=F9\n')
        scoped=b'workbench-appearance-v1\nfollow\t0\n@application\nbackground = #111111\n@default\nbackground = #222222\n@workspace\tw1\nbackground = #333333\n@terminal\tp1\nbackground = #444444\n'
        (sessions/'appearance.state').write_bytes(scoped)
        (sessions/'workspaces.json').write_text('runtime organization stays here')
        linux=prepare_preferences(repo,sessions,env,'linux');ui=managed/'ui/linux'
        assert Path(linux['WORKBENCH_CONFIG']).read_bytes()==original
        assert (ui/'shortcuts.conf').read_bytes()==b'workbench-shortcuts-migrated-v1\nforward=F8\n'
        assert b'@workspace' not in (ui/'appearance.state').read_bytes()
        assert b'@default' in (ui/'appearance.state').read_bytes()
        assert (sessions/'appearance.state').read_bytes()==scoped
        assert (ui/'shortcuts.conf').stat().st_mode & 0o777==0o600
        (ui/'shortcuts.conf').write_text('UI edit');prepare_preferences(repo,sessions,env,'linux')
        assert (ui/'shortcuts.conf').read_text()=='UI edit'
        (ui/'shortcuts.conf').unlink();prepare_preferences(repo,sessions,env,'linux')
        assert not (ui/'shortcuts.conf').exists(), 'removing overrides must not remigrate them'
        mac=prepare_preferences(repo,sessions,env,'darwin')
        assert Path(mac['WORKBENCH_SHORTCUTS']).read_bytes()==b'workbench-shortcuts-migrated-v1\nforward=F9\n'
        explicit=prepare_preferences(repo,sessions,dict(env,WORKBENCH_SHORTCUTS='/explicit'), 'linux')
        assert 'WORKBENCH_SHORTCUTS' not in explicit and 'WORKBENCH_LAYERED_SHORTCUTS' not in explicit
        assert config.read_bytes()==original and link.is_symlink()
        assert sorted(p.name for p in sessions.iterdir())==['appearance.state','workspaces.json']
        # Section order must not lose global settings when scopes appear first.
        interleaved=b'workbench-appearance-v1\nfollow\t0\n@workspace\tw1\nbackground = #333333\n@application\nbackground = #111111\n@terminal\tp1\nbackground = #444444\n@default\nbackground = #222222\n'
        stripped=global_appearance(interleaved)
        assert b'@application' in stripped and b'@default' in stripped and b'@workspace' not in stripped and b'@terminal' not in stripped
        cli_env=dict(os.environ,WORKBENCH_CONFIG_DIR=str(link))
        subprocess.run([sys.executable,str(ROOT/'scripts/config_paths.py'),'--init'],env=cli_env,check=True,capture_output=True)
        assert config.read_bytes()==original
        config.unlink();target=root/'tracked-config';target.write_bytes(original);config.symlink_to(target)
        subprocess.run([sys.executable,str(ROOT/'scripts/config_paths.py'),'--init'],env=cli_env,check=True,capture_output=True)
        assert config.is_symlink() and target.read_bytes()==original
        fresh=root/'fresh';cli_env['WORKBENCH_CONFIG_DIR']=str(fresh)
        subprocess.run([sys.executable,str(ROOT/'scripts/config_paths.py'),'--init'],env=cli_env,check=True,capture_output=True)
        assert (fresh/'config.conf').read_text().startswith('workbench-config-v1\n')

    print('PASS: Linux/Mac discovery, explicit/shared symlink roots, per-platform once-only migration, managed-file preservation and local scoped identities')
if __name__=='__main__':main()
