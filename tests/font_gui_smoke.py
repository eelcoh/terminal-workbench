"""Glyph coverage and real terminal screenshot regression for Claude symbols."""
from pathlib import Path
import os,select,subprocess,sys,tempfile,time,shutil,struct,zlib
from session_smoke import wait_until,attach,send
from gui_smoke import close_window
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from font_cmap import codepoints
SYMBOLS='▐▛█▝▜←❯⏵◐✻📦'

def pixels(path):
    data=path.read_bytes();offset=8;compressed=bytearray()
    while offset<len(data):
        length=struct.unpack('!I',data[offset:offset+4])[0]
        kind=data[offset+4:offset+8];body=data[offset+8:offset+8+length]
        if kind==b'IHDR':
            width,height,depth,color,_,_,interlace=struct.unpack('!IIBBBBB',body)
            assert depth==8 and color in (2,6) and not interlace
        if kind==b'IDAT':compressed.extend(body)
        offset+=length+12
    raw=zlib.decompress(compressed);channels=4 if color==6 else 3
    prior=bytearray(width*channels);offset=0;image=[]
    for y in range(height):
        kind=raw[offset];offset+=1
        row=bytearray(raw[offset:offset+len(prior)]);offset+=len(prior)
        for x in range(len(row)):
            left=row[x-channels] if x>=channels else 0;above=prior[x]
            corner=prior[x-channels] if x>=channels else 0
            if kind==0:value=0
            elif kind==1:value=left
            elif kind==2:value=above
            elif kind==3:value=(left+above)//2
            else:
                assert kind==4
                p=left+above-corner;d=[abs(p-v) for v in (left,above,corner)]
                value=(left,above,corner)[d.index(min(d))]
            row[x]=(row[x]+value)&255
        image.append([tuple(row[x:x+3]) for x in range(0,len(row),channels)])
        prior=row
    return image

def main():
    primary=codepoints((ROOT/'.local/ghostling-viewer/fonts/JetBrainsMono-Regular.ttf').read_bytes())
    fallback=codepoints((ROOT/'fonts/noto-symbols/NotoSansSymbols2-Regular.ttf').read_bytes())
    assert primary==sorted(set(primary)) and fallback==sorted(set(fallback))
    assert all(ord(cp) in primary or ord(cp) in fallback for cp in SYMBOLS)
    assert ord('◐') not in primary and ord('◐') in fallback
    runtime,binary=map(lambda value:str(Path(value).resolve()),sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix='wb-font-') as directory:
        root=Path(directory);path=root/'session.sock';processes=[];viewer=None;connection=None
        with (root/'log').open('w+') as log:
            try:
                rd,wr=os.pipe()
                xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-noreset','-screen','0','1280x900x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log)
                processes.append(xvfb);os.close(wr)
                assert select.select([rd],[],[],5)[0]
                with os.fdopen(rd) as pipe:display=pipe.readline().strip()
                env=dict(os.environ,DISPLAY=':'+display,LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_SOCKET=str(path))
                for key in ('WAYLAND_DISPLAY','WORKBENCH_WORKSPACES'):env.pop(key,None)
                server=subprocess.Popen([runtime,str(path),'/bin/sh'],cwd=root,stdout=log,stderr=log);processes.append(server)
                wait_until(path.exists);connection,_,_=attach(path)
                specimen='\x1b[2J\x1b[H'+('???\r\n'+''.join(cp*3+'\r\n' for cp in SYMBOLS))+'\r\n ▐▛███▛█   Claude Code\r\n▝▜██████▀  Unicode terminal sample\r\n ❯  ⏵⏵  ◐  ✻  📦\r\n'
                (root/'specimen').write_bytes(specimen.encode())
                send(connection,16,b'cat specimen; sleep 120\n')
                wait_until(lambda:'Unicode terminal sample' in subprocess.check_output([runtime,'read',str(path)],text=True))
                connection.close();connection=None;time.sleep(.2)
                viewer=subprocess.Popen([binary],cwd=root,env=env,stdout=log,stderr=log);processes.append(viewer)
                def cmd(*args):return subprocess.run(['xdotool',*args],env=env,capture_output=True,text=True,timeout=5)
                window=wait_until(lambda:cmd('search','--onlyvisible','--pid',str(viewer.pid)).stdout.strip(),timeout=15)
                cmd('windowfocus',window).check_returncode();time.sleep(.5)
                cmd('keydown','F12').check_returncode();time.sleep(.06);cmd('keyup','F12').check_returncode()
                shot=wait_until(lambda:next(iter(root.glob('screenshot*.png')),None))
                image=pixels(shot)
                # The terminal's existing 7x16 grid remains unchanged. Compare
                # a three-glyph footprint with the literal '?' baseline.
                def crop(row):return tuple(tuple(image[y][4:25]) for y in range(36+row*16,52+row*16))
                reference=crop(0)
                for row,cp in enumerate(SYMBOLS,1):
                    glyph=crop(row)
                    assert glyph!=reference,f'{cp} rendered as missing-glyph question marks'
                    assert len(set(pixel for line in glyph for pixel in line))>1,f'{cp} has no visible ink'
                dest=ROOT/'.local/test-results/fonts.png';dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(shot,dest)
                close_window(window,env);assert viewer.wait(timeout=5)==0
                print('PASS: bundled Unicode coverage and visible primary/fallback Claude glyphs; screenshot',dest)
            finally:
                if connection:connection.close()
                subprocess.run([runtime,'stop',str(path)],capture_output=True,timeout=12)
                for process in reversed(processes):
                    if process.poll() is None:process.terminate()
                    process.wait(timeout=5)
                if sys.exc_info()[0]:log.seek(0);print(log.read(),file=sys.stderr)

if __name__=='__main__':main()
