"""Linear-time font embedding; CMake's per-byte concatenation is too expensive."""
from pathlib import Path
import sys
data=Path(sys.argv[1]).read_bytes()
lines=['// Generated from the bundled font.','static const unsigned char '+sys.argv[3]+'[] = {']
lines+=['    '+','.join('0x%02x'%byte for byte in data[i:i+24])+',' for i in range(0,len(data),24)]
lines+=['};']
Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
