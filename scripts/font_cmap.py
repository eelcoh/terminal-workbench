"""Read Unicode mappings in the two pinned TrueType fonts, with stdlib only."""
import struct

def codepoints(data):
    def u16(offset): return struct.unpack_from('>H',data,offset)[0]
    def u32(offset): return struct.unpack_from('>I',data,offset)[0]
    tables={data[o:o+4]:(u32(o+8),u32(o+12)) for o in range(12,12+16*u16(4),16)}
    base,_=tables[b'cmap']
    result=set()
    for offset in range(base+4,base+4+8*u16(base+2),8):
        platform,encoding=u16(offset),u16(offset+2)
        if platform!=0 and not (platform==3 and encoding in (1,10)): continue
        table=base+u32(offset+4)
        form=u16(table)
        if form==12:
            for group in range(table+16,table+16+12*u32(table+12),12):
                first,last,glyph=u32(group),u32(group+4),u32(group+8)
                if last>0x10ffff or last<first: raise ValueError('invalid Unicode group')
                result.update(range(first+(glyph==0),last+1))
        elif form==4:
            count=u16(table+6)//2
            ends=table+14; starts=ends+2*count+2; deltas=starts+2*count; ranges=deltas+2*count
            for i in range(count):
                delta=u16(deltas+2*i); relative=u16(ranges+2*i)
                for cp in range(u16(starts+2*i),u16(ends+2*i)+1):
                    if cp==0xffff: continue
                    glyph=u16(ranges+2*i+relative+2*(cp-u16(starts+2*i))) if relative else cp
                    mapped=(glyph+delta)&0xffff if glyph else 0
                    if mapped: result.add(cp)
    return sorted(cp for cp in result if cp>=32 and not 0xd800<=cp<=0xdfff)

def header(primary,fallback):
    main=codepoints(primary)
    extra=sorted(set(codepoints(fallback))-set(main))
    lines=['// Generated from the pinned bundled font cmap tables.','#pragma once']
    for name,values in [('terminal_primary_codepoints',main),('terminal_fallback_codepoints',extra)]:
        lines+=['static int '+name+'[] = {']
        lines+=['    '+','.join(str(cp) for cp in values[i:i+16])+',' for i in range(0,len(values),16)]
        lines+=['};']
    return '\n'.join(lines)+'\n'
