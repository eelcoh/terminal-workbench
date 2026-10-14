#include <assert.h>
#include "terminal_context.h"
int main(void) {
    TerminalContext context={0};
    uint8_t valid[]={1,1,0,4,'/','t','m','p',0,1,'r',0,1,'b',0,0};
    assert(terminal_context_decode(&context,valid,sizeof(valid)));
    assert(!strcmp(context.cwd,"/tmp") && !strcmp(context.branch,"b"));
    for(size_t size=0;size<sizeof(valid);size++) assert(!terminal_context_decode(&context,valid,size));
    uint8_t invalid[sizeof(valid)];memcpy(invalid,valid,sizeof(valid));
    invalid[1]=5;assert(!terminal_context_decode(&context,invalid,sizeof(invalid)));
    invalid[1]=1;invalid[4]=0;assert(!terminal_context_decode(&context,invalid,sizeof(invalid)));
    invalid[4]=0xff;assert(!terminal_context_decode(&context,invalid,sizeof(invalid)));
    invalid[4]=0xc0;invalid[5]=0x80;assert(!terminal_context_decode(&context,invalid,sizeof(invalid)));
    assert(!strcmp(context.cwd,"/tmp")); /* Invalid packets are transactional. */
    char label[4097];
    terminal_context_display("/a\nb",label,sizeof(label));assert(!strcmp(label,"/a b"));
    terminal_context_label(&context,"Shell 1",false,label,sizeof(label));assert(!strcmp(label,"r · b"));
    terminal_context_label(&context,"Shell 1",true,label,sizeof(label));assert(!strcmp(label,"Shell 1"));
    strcpy(context.worktree,"fix");context.detached=true;
    terminal_context_label(&context,"Shell 1",false,label,sizeof(label));assert(!strcmp(label,"r · detached b · fix"));
    context.repository[0]=0;strcpy(context.cwd,"/tmp/λ\nfolder");
    terminal_context_label(&context,"Shell 1",false,label,sizeof(label));assert(!strcmp(label,"/tmp/λ folder"));
    context.available=false;
    terminal_context_label(&context,"Legacy",false,label,sizeof(label));assert(!strcmp(label,"Legacy"));
    assert(terminal_context_default_name("Shell 1") && terminal_context_default_name("Terminal 0"));
    assert(!terminal_context_default_name("Shell λ") && !terminal_context_default_name("Shell 1 project"));
    return 0;
}
