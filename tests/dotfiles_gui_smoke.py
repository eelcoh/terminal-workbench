"""Read-only managed settings, UI deltas, reopen and errors with the same live shell."""
from pathlib import Path
import fcntl,os,select,shutil,subprocess,sys,tempfile,termios,time
from gui_smoke import close_window
from session_smoke import attach,send,wait_until
from theme_gui_smoke import pixel
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from config_paths import prepare_preferences

def main():
    runtime,viewer=[str(Path(p).resolve()) for p in sys.argv[1:3]]
    with tempfile.TemporaryDirectory(prefix='wb-dotfiles-gui-') as directory:
        root=Path(directory);processes=[];state=root/'sessions';state.mkdir();managed=root/'managed';managed.mkdir()
        theme=managed/'theme';theme.write_text('background = #123456\nforeground = #eeeeee\n')
        config=managed/'config.conf';original='workbench-config-v1\n[appearance]\napplication-theme = Workbench Dawn\nterminal-theme = theme\n[shortcuts]\nforward = F8\n[linux.shortcuts]\nterminal.next = Ctrl+Alt+Right\n'
        config.write_text(original);link=root/'config';link.symlink_to(managed,target_is_directory=True)
        (root/'catalog').write_text(f'workbench-catalog-v3\nW\tmain\tDotfiles\nT\tmain\tp1\tTerminal\t{root}/term.sock\nW\tempty\tEmpty project\n')
        with (root/'test.log').open('w+') as log:
            try:
                server=subprocess.Popen([runtime,str(root/'term.sock'),'/bin/sh'],cwd=root,stdout=log,stderr=log);processes.append(server);wait_until((root/'term.sock').exists)
                connection,_,_=attach(root/'term.sock');send(connection,16,b'keep=dotfiles; echo $$ > original.pid\n');connection.close()
                pid=int(wait_until(lambda:(root/'original.pid').exists() and (root/'original.pid').read_text().strip()));inode=(root/'term.sock').stat().st_ino
                rd,wr=os.pipe();xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(xvfb);os.close(wr)
                assert select.select([rd],[],[],5)[0]
                with os.fdopen(rd) as ready:display=ready.readline().strip()
                env=dict(os.environ,DISPLAY=':'+display,LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(root/'catalog'),WORKBENCH_STATE_DIR=str(state),WORKBENCH_CONFIG_DIR=str(link))
                for name in ['WAYLAND_DISPLAY','WORKBENCH_SOCKET','WORKBENCH_CATALOG_SCRIPT','WORKBENCH_SHORTCUTS','WORKBENCH_THEME','WORKBENCH_UI_APPEARANCE','WORKBENCH_RENDER_SCALE_FILE','WORKBENCH_RENDER_SCALE']:env.pop(name,None)
                env.update(prepare_preferences(root,state,env,'linux'));ui=managed/'ui/linux'
                def cmd(*args):
                    result=subprocess.run(['xdotool',*map(str,args)],env=env,capture_output=True,text=True,timeout=5);result.check_returncode();return result.stdout.strip()
                def start():
                    process=subprocess.Popen([viewer],cwd=root,env=env,stdout=log,stderr=log);processes.append(process)
                    def find():
                        assert process.poll() is None
                        result=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],env=env,capture_output=True,text=True)
                        return result.stdout.strip().splitlines()[-1] if result.returncode==0 else None
                    window=wait_until(find);cmd('windowfocus',window);time.sleep(.4);return process,window
                process,window=start()
                def key(value):cmd('keydown','--window',window,value);time.sleep(.08);cmd('keyup','--window',window,value);time.sleep(.1)
                def text(value):cmd('type','--clearmodifiers','--window',window,'--delay',10,'--',value)
                def palette(query):key('ctrl+shift+p');text(query);key('Return')
                def safe(name):
                    text(f'printf "%s:%s" "$$" "$keep" > {name}');key('Return')
                    assert wait_until(lambda:(root/name).exists() and (root/name).read_text())==f'{pid}:dotfiles'
                def capture(tag):
                    started=time.time_ns();key('F12');image=wait_until(lambda:next((p for p in root.glob('screenshot*.png') if p.stat().st_mtime_ns>=started),None))
                    target=ROOT/'.local/test-results'/f'dotfiles-{tag}.png';target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(image,target);key('ctrl+u');return target
                def geometry():
                    fd=os.open(f'/proc/{pid}/fd/0',os.O_RDONLY|os.O_NOCTTY)
                    try:return fcntl.ioctl(fd,termios.TIOCGWINSZ,bytes(8))
                    finally:os.close(fd)
                safe('initial');baseline=geometry();image=capture('managed')
                assert pixel(image,200,600)==(0x12,0x34,0x56)
                assert pixel(image,1200,600)==(0xf4,0xf1,0xeb)
                palette('Change appearance');key('Return')
                assert '@workspace\tmain' in (state/'appearance.state').read_text(), 'Apply did not pin the selected default'
                palette('Shortcut settings');text('Next terminal');key('Return');key('ctrl+alt+j');key('Return');key('Escape')
                wait_until(lambda:(ui/'shortcuts.conf').exists());saved=(ui/'shortcuts.conf').read_text()
                assert saved=='workbench-shortcuts-v1\nterminal.next=Ctrl+Alt+J\n',saved
                palette('Change appearance');text('Workbench Night');key('shift+Tab');key('space')
                wait_until(lambda:(ui/'appearance.state').exists());saved=(ui/'appearance.state').read_text()
                assert saved.startswith('workbench-ui-appearance-v1\nmask\t4\n') and '@workspace' not in saved and '@terminal' not in saved
                safe('after-ui');assert geometry()==baseline and config.read_text()==original and link.is_symlink()
                # A saved workspace theme wins over changed managed defaults on reopen.
                close_window(window,env);assert process.wait(timeout=5)==0
                theme.write_text('background = #654321\nforeground = #eeeeee\n')
                config.write_text(original.replace('forward = F8','forward = F10'))
                process,window=start();safe('reopened');image=capture('reopened')
                assert pixel(image,200,600)==(0x15,0x19,0x1f)
                assert pixel(image,1200,600)==(0x15,0x19,0x1f),pixel(image,1200,600)
                assert geometry()==baseline
                # Cancel and an inaccessible save destination must leave the managed file/shell usable.
                palette('Change appearance');text('Workbench Dawn');key('Escape');safe('cancel')
                saved_path=state/'appearance.state';before=saved_path.read_bytes();saved_path.unlink();saved_path.mkdir()
                palette('Change appearance');text('Workbench Dawn');key('shift+Tab');key('space');key('Escape');safe('failed-save')
                assert saved_path.is_dir();saved_path.rmdir();saved_path.write_bytes(before)
                # Reset to the managed default without editing the managed file.
                palette('Change appearance');cmd('mousemove','--window',window,280,667);cmd('click',1);time.sleep(.2);key('Tab');key('Tab');key('space')
                image=capture('inherited');assert pixel(image,200,600)==(0x65,0x43,0x21)
                close_window(window,env);assert process.wait(timeout=5)==0
                # Invalid managed config remains read-only; defaults plus valid UI overrides open a usable viewer.
                config.write_text('workbench-config-v1\n[shortcuts]\ncopy = Ctrl+C\n');bad=config.read_bytes()
                process,window=start();safe('malformed-config');assert config.read_bytes()==bad
                assert 'Dotfiles:' in (root/'test.log').read_text()
                os.kill(pid,0);assert (root/'term.sock').stat().st_ino==inode and geometry()==baseline
                # An explicit advanced fixed-chrome policy remains usable without a checkbox.
                close_window(window,env);assert process.wait(timeout=5)==0
                config.write_text('workbench-config-v1\n[appearance]\napplication-theme = Workbench Dawn\nterminal-theme = theme\nfollow-workspace = false\n')
                process,window=start();palette('Change appearance');image=capture('fixed-picker');assert pixel(image,210,180)[0]>200
                text('Dracula');key('Return')
                image=capture('fixed-chrome');assert pixel(image,200,600)==(0x28,0x2a,0x36) and pixel(image,1200,600)==(0xf4,0xf1,0xeb)
                safe('fixed-chrome-state')
                # Workspace themes remain available when there are no terminals.
                key('ctrl+shift+Down');palette('Change appearance');text('Workbench Dawn');key('Return')
                assert '@workspace\tempty' in (state/'appearance.state').read_text()
                key('ctrl+shift+Up');safe('after-empty-workspace');assert geometry()==baseline
                close_window(window,env);assert process.wait(timeout=5)==0
                print('PASS: managed/symlink colors and shortcuts, sparse UI saves, workspace persistence and managed reset, malformed config/failed-save isolation and same shell/PID/socket/grid')
            except Exception:
                log.flush();print((root/'test.log').read_text()[-4500:],file=sys.stderr);raise
            finally:
                for p in reversed(processes):
                    if p.poll() is None:p.terminate();p.wait(timeout=5)
if __name__=='__main__':main()
