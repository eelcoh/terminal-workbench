"""Portable discovery/migration never rewrites managed files or moves sessions."""
from pathlib import Path
import os,sys,tempfile,subprocess
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from config_paths import config_directory,prepare_preferences,global_appearance,prepare_config_directory

def main():
    with tempfile.TemporaryDirectory(prefix='wb-dotfiles-') as directory:
        root=Path(directory);home=root/'home';home.mkdir();repo=root/'repo';(repo/'.local').mkdir(parents=True)
        sessions=repo/'.local/sessions';sessions.mkdir();env=dict(HOME=str(home))
        assert config_directory(env,'linux')==home/'.config/terminal-workbench'
        assert config_directory(env,'darwin')==home/'.config/terminal-workbench'
        assert config_directory(dict(env,XDG_CONFIG_HOME=str(root/'xdg')),'darwin')==root/'xdg/terminal-workbench'
        assert config_directory(dict(env,XDG_CONFIG_HOME=str(root/'xdg')),'linux')==root/'xdg/terminal-workbench'
        legacy=home/'Library/Application Support/Terminal Workbench';(legacy/'themes/imports').mkdir(parents=True)
        (legacy/'ui/macos').mkdir(parents=True);(legacy/'config.conf').write_text('managed Mac settings')
        (legacy/'themes/miasma-default').write_text('background = #222222\n')
        (legacy/'themes/imports/imports.json').write_text('{"version":1}')
        (legacy/'themes/imports/imports.lock').write_text('stale lock')
        (legacy/'ui/macos/terminal-font.json').write_text('{"version":1,"size":19}')
        (legacy/'ui/macos/.migrated-v1').touch();(legacy/'workspaces.json').write_text('never copy sessions')
        target_theme=root/'managed-theme';target_theme.write_text('background = #111111\n');(legacy/'themes/linked').symlink_to(target_theme)
        migrated=prepare_config_directory(env,'darwin')
        assert migrated==home/'.config/terminal-workbench' and (migrated/'config.conf').read_text()=='managed Mac settings'
        assert (migrated/'themes/miasma-default').read_bytes()==(legacy/'themes/miasma-default').read_bytes()
        assert (migrated/'ui/macos/terminal-font.json').exists() and (migrated/'ui/macos/.migrated-v1').exists()
        assert (migrated/'themes/linked').is_symlink() and (migrated/'themes/linked').read_bytes()==target_theme.read_bytes()
        assert not (migrated/'workspaces.json').exists() and not (migrated/'themes/imports/imports.lock').exists()
        (migrated/'themes/miasma-default').unlink();prepare_config_directory(env,'darwin')
        assert not (migrated/'themes/miasma-default').exists(),'deleted themes must not remigrate'
        assert (legacy/'themes/miasma-default').exists(),'legacy files must remain untouched'
        merged=root/'merged';merged.mkdir();(merged/'config.conf').write_text('new choice')
        existing=root/'existing-themes';existing.mkdir();(merged/'themes').symlink_to(existing,target_is_directory=True)
        assert prepare_config_directory(dict(env,XDG_CONFIG_HOME=str(root/'merge-root')),'linux')==root/'merge-root/terminal-workbench'
        # Explicit overrides are never populated from the previous native Mac folder.
        explicit_root=root/'explicit-root';prepare_config_directory(dict(env,WORKBENCH_CONFIG_DIR=str(explicit_root)),'darwin');assert not explicit_root.exists()
        merge_env=dict(env,XDG_CONFIG_HOME=str(root/'merge-xdg'));merge_target=root/'merge-xdg/terminal-workbench';merge_target.mkdir(parents=True)
        (merge_target/'config.conf').write_text('new choice');(merge_target/'themes').symlink_to(existing,target_is_directory=True)
        prepare_config_directory(merge_env,'darwin');assert (merge_target/'config.conf').read_text()=='new choice' and not list(existing.iterdir())
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
