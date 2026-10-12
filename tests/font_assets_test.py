"""Font cmap contracts and pinned symbol asset integrity, without GUI or packages."""
from pathlib import Path
import hashlib,struct,sys
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from font_cmap import codepoints

def font(cmap):
    return struct.pack('>IHHHH',0x10000,1,0,0,0)+struct.pack('>4sIII',b'cmap',0,28,len(cmap))+cmap

def main():
    # Format 4: direct delta, glyph-index array, missing glyph and sentinel.
    table=struct.pack('>HHHHHHH',4,46,0,6,0,0,0)
    table+=struct.pack('>HHHH',65,68,65535,0)
    table+=struct.pack('>HHH',65,66,65535)
    table+=struct.pack('>HHH',1,2,1)
    table+=struct.pack('>HHH',0,4,0)
    table+=struct.pack('>HHH',3,0,5)
    cmap=struct.pack('>HHHHI',0,1,3,1,12)+table
    assert codepoints(font(cmap))==[65,66,68]
    # Format 12: supplementary characters; glyph zero isn't real coverage.
    table=struct.pack('>HHIII',12,0,40,0,2)+struct.pack('>III',0x1f4e6,0x1f4e6,5)+struct.pack('>III',100,102,0)
    cmap=struct.pack('>HHHHI',0,1,3,10,12)+table
    assert codepoints(font(cmap))==[101,102,0x1f4e6]
    asset=ROOT/'fonts/noto-symbols/NotoSansSymbols2-Regular.ttf'
    digest=hashlib.sha256(asset.read_bytes()).hexdigest()
    assert digest in (asset.parent/'README.md').read_text()
    covered=codepoints(asset.read_bytes())
    assert covered==sorted(set(covered))
    assert all(ord(cp) in covered for cp in '⏵◐✻📦')
    assert 'SIL OPEN FONT LICENSE Version 1.1' in (asset.parent/'OFL.txt').read_text()
    interface=ROOT/'fonts/adwaita-sans/AdwaitaSans-Regular.ttf'
    assert hashlib.sha256(interface.read_bytes()).hexdigest() in (interface.parent/'README.md').read_text()
    assert 'SIL OPEN FONT LICENSE Version 1.1' in (interface.parent/'OFL.txt').read_text()
    ui_covered=codepoints(interface.read_bytes())
    assert all(ord(cp) in ui_covered for cp in 'Workbench café Ελληνικά / →')
    print('PASS: bundled interface integrity/coverage/license; cmap format 4/12, non-BMP/missing glyphs, symbol coverage, font digest and license')

if __name__=='__main__':main()
