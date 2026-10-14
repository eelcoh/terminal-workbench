/* Optional runtime context. No filesystem or process discovery in the viewer. */
#ifndef WORKBENCH_TERMINAL_CONTEXT_H
#define WORKBENCH_TERMINAL_CONTEXT_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
typedef struct {
    char cwd[4097], repository[513], branch[1025], worktree[513];
    bool available, detached, remote;
} TerminalContext;
static bool terminal_context_default_name(const char *name)
{
    const char *p = !strncmp(name,"Shell ",6) ? name+6 : !strncmp(name,"Terminal ",9) ? name+9 : NULL;
    if (!p || !*p) return false;
    for (; *p; p++) if (*p<'0' || *p>'9') return false;
    return true;
}
static bool terminal_context_utf8(const uint8_t *p, size_t size)
{
    for (size_t i=0;i<size;) {
        uint32_t c=p[i++], min=0; int extra=0;
        if (!c) return false;
        if (c<128) continue;
        if (c>=0xc2 && c<=0xdf) { c&=31;extra=1;min=128; }
        else if (c>=0xe0 && c<=0xef) { c&=15;extra=2;min=2048; }
        else if (c>=0xf0 && c<=0xf4) { c&=7;extra=3;min=65536; }
        else return false;
        if (i+(size_t)extra>size) return false;
        while(extra--) { if ((p[i]&0xc0)!=0x80) return false; c=(c<<6)|(p[i++]&63); }
        if(c<min || c>0x10ffff || (c>=0xd800 && c<=0xdfff)) return false;
    }
    return true;
}
static bool terminal_context_decode(TerminalContext *context,const uint8_t *data,size_t size)
{
    if(size<10 || data[0]!=1 || (data[1]&~7)) return false;
    TerminalContext next={0};
    char *fields[]={next.cwd,next.repository,next.branch,next.worktree};
    size_t capacities[]={sizeof(next.cwd),sizeof(next.repository),sizeof(next.branch),sizeof(next.worktree)};
    size_t offset=2;
    for(int i=0;i<4;i++) {
        if(offset+2>size) return false;
        size_t length=((size_t)data[offset]<<8)|data[offset+1];offset+=2;
        if(length>=capacities[i] || length>size-offset || !terminal_context_utf8(data+offset,length)) return false;
        memcpy(fields[i],data+offset,length);offset+=length;
    }
    if(offset!=size) return false;
    next.available=(data[1]&1)!=0;next.detached=(data[1]&2)!=0;next.remote=(data[1]&4)!=0;
    if(next.available != (next.cwd[0]!=0) || (next.remote && next.available)) return false;
    *context=next;return true;
}
static void terminal_context_display(const char *text,char *label,size_t size)
{
    snprintf(label,size,"%s",text);
    for(char *p=label;*p;p++) if((unsigned char)*p<32 || *p==127) *p=' ';
}
static void terminal_context_label(const TerminalContext *context,const char *name,bool custom,char *label,size_t size)
{
    if(custom || !context->available) snprintf(label,size,"%s",name);
    else if(context->repository[0]) snprintf(label,size,"%s · %s%s%s%s",context->repository,
        context->detached ? "detached " : "",context->branch,
        context->worktree[0] ? " · " : "",context->worktree);
    else snprintf(label,size,"%s",context->cwd);
    /* Paths may contain controls. Preserve raw API data, sanitize visible text. */
    for(char *p=label;*p;p++) if((unsigned char)*p<32 || *p==127) *p=' ';
}
#endif
