#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <sys/stat.h>
#include "shortcut_state.h"
int main(void)
{
    ShortcutState linux_state=shortcut_defaults(false),mac=shortcut_defaults(true);char error[256];
    assert(shortcut_valid(&linux_state,error,sizeof(error)) && shortcut_valid(&mac,error,sizeof(error)));
    assert(linux_state.bindings[COMMAND_TERMINAL_NEXT].key==262 && linux_state.bindings[COMMAND_TERMINAL_NEXT].mods==(SHORTCUT_SUPER|SHORTCUT_ALT));
    assert(mac.bindings[COMMAND_WORKSPACE_PREVIOUS].key==265 && mac.bindings[COMMAND_WORKSPACE_PREVIOUS].mods==(SHORTCUT_SUPER|SHORTCUT_ALT));
    assert(linux_state.bindings[COMMAND_WIDTH_CYCLE].key=='R' && linux_state.bindings[COMMAND_WIDTH_CYCLE].mods==(SHORTCUT_SUPER|SHORTCUT_ALT));
    assert(linux_state.bindings[COMMAND_WIDTH_FULL].key=='F' && linux_state.bindings[COMMAND_WIDTH_FULL].mods==(SHORTCUT_SUPER|SHORTCUT_ALT));
    assert(mac.bindings[COMMAND_WIDTH_CYCLE].key=='R' && mac.bindings[COMMAND_WIDTH_CYCLE].mods==mac.bindings[COMMAND_TERMINAL_NEXT].mods);
    assert(mac.bindings[COMMAND_WIDTH_FULL].key=='F' && mac.bindings[COMMAND_WIDTH_FULL].mods==mac.bindings[COMMAND_TERMINAL_NEXT].mods);
    for (int i=0;i<COMMAND_COUNT;i++) if (mac.bindings[i].key)
        assert(mac.bindings[i].mods==(i==COMMAND_COPY || i==COMMAND_PASTE ? SHORTCUT_SUPER : SHORTCUT_SUPER|SHORTCUT_ALT));
    assert(mac.bindings[COMMAND_PALETTE].key=='P' && mac.bindings[COMMAND_OVERVIEW].key=='O');
    assert(mac.bindings[COMMAND_TERMINAL_CREATE].key=='T' && mac.bindings[COMMAND_WORKSPACE_CREATE].key=='N');
    assert(linux_state.bindings[COMMAND_HELP].key==47 && linux_state.bindings[COMMAND_HELP].mods==(SHORTCUT_SUPER|SHORTCUT_ALT));
    assert(mac.bindings[COMMAND_HELP].key==47);
    assert(linux_state.bindings[COMMAND_AGENT_INPUT_NEXT].key=='J' && linux_state.bindings[COMMAND_AGENT_INPUT_NEXT].mods==(SHORTCUT_SUPER|SHORTCUT_ALT));
    assert(mac.bindings[COMMAND_AGENT_INPUT_PREVIOUS].key=='K' && mac.bindings[COMMAND_AGENT_INPUT_PREVIOUS].mods==(SHORTCUT_SUPER|SHORTCUT_ALT));
    ShortcutChord chord={0};assert(shortcut_parse("Command+Option+Left",&chord) && chord.mods==12);
    char label[96];shortcut_label(chord,true,label,sizeof(label));assert(!strcmp(label,"Command+Option+Left"));
    for (int i=0;i<COMMAND_COUNT;i++) { shortcut_label(linux_state.bindings[i],false,label,sizeof(label));ShortcutChord roundtrip;assert(shortcut_parse(label,&roundtrip));assert(roundtrip.key==linux_state.bindings[i].key && roundtrip.mods==linux_state.bindings[i].mods); }
    const char *bad[]={"Ctrl+C","Ctrl+D","Ctrl+Z","P","Shift+Left","Escape","Ctrl+F12","Ctrl+Ctrl+P","Ctrl+P+Shift","Ctrl+","Alt+unknown","Ctrl+Alt+Shift+Super+Shift+P"};
    for (size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) assert(!shortcut_parse(bad[i],&chord));
    assert(shortcut_parse("Ctrl+Shift+/",&chord) && chord.key==47 && chord.mods==3);
    assert(shortcut_parse("Command+Option+Slash",&chord) && chord.key==47 && chord.mods==12);
    shortcut_label(chord,true,label,sizeof(label));assert(!strcmp(label,"Command+Option+/"));
    assert(!shortcut_parse("Ctrl+Shift+?",&chord) && !shortcut_parse("/",&chord));
    assert(shortcut_parse("Command+Option+Equal",&chord) && chord.key==61 && chord.mods==12);
    assert(shortcut_parse("Ctrl+Shift+-",&chord) && chord.key==45 && chord.mods==3);
    assert(linux_state.bindings[COMMAND_FONT_INCREASE].key==61 && mac.bindings[COMMAND_FONT_RESET].key==48);
    assert(linux_state.bindings[COMMAND_SEARCH].key=='S' && mac.bindings[COMMAND_SEARCH].mods==(SHORTCUT_SUPER|SHORTCUT_ALT));
    assert(shortcut_parse("F8",&chord));assert(shortcut_parse("none",&chord));
    char path[1024];snprintf(path,sizeof(path),"%s/shortcuts.conf",getenv("WB_TEST_DIRECTORY"));
    assert(shortcut_write(path,&linux_state,error,sizeof(error)));struct stat statbuf;assert(!stat(path,&statbuf) && (statbuf.st_mode&0777)==0600);
    ShortcutState loaded={0};assert(shortcut_read(path,false,&loaded,error,sizeof(error)));assert(!memcmp(&loaded,&linux_state,sizeof(loaded)));
    ShortcutState collision=linux_state;collision.bindings[COMMAND_WORKSPACE_NEXT]=collision.bindings[COMMAND_TERMINAL_NEXT];
    assert(!shortcut_write(path,&collision,error,sizeof(error)));assert(strstr(error,"Next terminal"));
    assert(shortcut_read(path,false,&loaded,error,sizeof(error)) && !memcmp(&loaded,&linux_state,sizeof(loaded)));
    const char *invalid[]={"workbench-shortcuts-v99\n", "band-shortcuts-v1\ncopy=Ctrl+C\n", "band-shortcuts-v1\nunknown=F8\n", "band-shortcuts-v1\ncopy=F8\ncopy=F9\n", "band-shortcuts-v1\nterminal.next=Super+Alt+P\ncommands=Super+Alt+P\n", "band-shortcuts-v1\ncopy=F8"};
    for (size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) { FILE *file=fopen(path,"w");assert(file);fputs(invalid[i],file);fclose(file);assert(!shortcut_read(path,false,&loaded,error,sizeof(error)));assert(!memcmp(&loaded,&linux_state,sizeof(loaded))); }
    FILE *file=fopen(path,"w");assert(file);fputs("band-shortcuts-v1\n# partial overrides\ncommands=none\ncopy=Ctrl+Shift+P\n",file);fclose(file);
    assert(shortcut_read(path,false,&loaded,error,sizeof(error)));assert(!loaded.bindings[COMMAND_PALETTE].key && loaded.bindings[COMMAND_COPY].key=='P');
    file=fopen(path,"w");assert(file);fputs("workbench-shortcuts-v1\ncommands=none\n",file);fclose(file);
    assert(shortcut_read(path,false,&loaded,error,sizeof(error)));assert(!loaded.bindings[COMMAND_PALETTE].key);
    file=fopen(path,"w");assert(file);fputs("band-shortcuts-v1\ncopy=Super+Alt+R\n",file);fclose(file);
    assert(shortcut_read(path,false,&loaded,error,sizeof(error)));
    assert(loaded.bindings[COMMAND_COPY].key=='R' && !loaded.bindings[COMMAND_WIDTH_CYCLE].key);
    assert(loaded.bindings[COMMAND_WIDTH_FULL].key=='F');
    file=fopen(path,"w");assert(file);fputs("band-shortcuts-v1\ncopy=Super+Alt+/\n",file);fclose(file);
    assert(shortcut_read(path,false,&loaded,error,sizeof(error)) && !loaded.bindings[COMMAND_HELP].key);
    file=fopen(path,"w");assert(file);fputs("band-shortcuts-v1\ncommands=Command+Shift+P\noverview=Command+Shift+O\nterminal.new=Command+T\n",file);fclose(file);
    assert(shortcut_read(path,true,&loaded,error,sizeof(error)) && loaded.bindings[COMMAND_PALETTE].mods==(SHORTCUT_SUPER|SHORTCUT_SHIFT));
    assert(loaded.bindings[COMMAND_OVERVIEW].mods==(SHORTCUT_SUPER|SHORTCUT_SHIFT) && loaded.bindings[COMMAND_TERMINAL_CREATE].mods==SHORTCUT_SUPER);
    file=fopen(path,"w");assert(file);fputs("band-shortcuts-v1\ncopy=Command+Option+P\n",file);fclose(file);
    assert(shortcut_read(path,true,&loaded,error,sizeof(error)) && !loaded.bindings[COMMAND_PALETTE].key);
    assert(loaded.bindings[COMMAND_COPY].mods==(SHORTCUT_SUPER|SHORTCUT_ALT));
    assert(!shortcut_read(getenv("WB_TEST_DIRECTORY"),false,&loaded,error,sizeof(error)));
    file=fopen(path,"w");assert(file);fputs("band-shortcuts-v1\nterminal.new=Super+Alt+S\n",file);fclose(file);
    assert(shortcut_read(path,false,&loaded,error,sizeof(error)) && loaded.bindings[COMMAND_TERMINAL_CREATE].key=='S' && !loaded.bindings[COMMAND_SEARCH].key);
    file=fopen(path,"w");assert(file);fputs("band-shortcuts-v1\nterminal.new=Command+Option+S\n",file);fclose(file);
    assert(shortcut_read(path,true,&loaded,error,sizeof(error)) && loaded.bindings[COMMAND_TERMINAL_CREATE].key=='S' && !loaded.bindings[COMMAND_SEARCH].key);
    file=fopen(path,"w");assert(file);fputs("band-shortcuts-v1\ncommands=Super+Alt+J\n",file);fclose(file);
    assert(shortcut_read(path,false,&loaded,error,sizeof(error)) && loaded.bindings[COMMAND_PALETTE].key=='J' && !loaded.bindings[COMMAND_AGENT_INPUT_NEXT].key);
    /* A full saved old Linux map retains every key, and loading never rewrites it. */
    ShortcutState old_linux=linux_state;
    for(int i=0;i<COMMAND_COUNT;i++) if(old_linux.bindings[i].key) old_linux.bindings[i].mods=SHORTCUT_CTRL|SHORTCUT_SHIFT;
    assert(shortcut_write(path,&old_linux,error,sizeof(error)));
    struct stat saved_stat;assert(!stat(path,&saved_stat));
    assert(shortcut_read(path,false,&loaded,error,sizeof(error)) && !memcmp(&loaded,&old_linux,sizeof(loaded)));
    assert(!stat(path,&statbuf) && saved_stat.st_ino==statbuf.st_ino && saved_stat.st_size==statbuf.st_size);
    /* Explicit old defaults, custom keys and unbindings remain authoritative. */
    file=fopen(path,"w");assert(file);fputs("band-shortcuts-v1\ncommands=Ctrl+Shift+P\nterminal.next=Ctrl+Shift+J\noverview=none\n",file);fclose(file);
    assert(shortcut_read(path,false,&loaded,error,sizeof(error)));
    assert(loaded.bindings[COMMAND_PALETTE].mods==3 && loaded.bindings[COMMAND_TERMINAL_NEXT].key=='J');
    assert(!loaded.bindings[COMMAND_OVERVIEW].key && loaded.bindings[COMMAND_WORKSPACE_NEXT].mods==12);
    /* New Linux defaults yield to explicit old-file customizations. */
    file=fopen(path,"w");assert(file);fputs("band-shortcuts-v1\nforward=Super+Alt+Left\ncopy=Super+Alt+P\n",file);fclose(file);
    assert(shortcut_read(path,false,&loaded,error,sizeof(error)));
    assert(!loaded.bindings[COMMAND_TERMINAL_PREVIOUS].key && !loaded.bindings[COMMAND_PALETTE].key);
    assert(loaded.bindings[COMMAND_FORWARD].mods==12 && loaded.bindings[COMMAND_COPY].key=='P');
    assert(linux_state.bindings[COMMAND_COPY].mods==3 && linux_state.bindings[COMMAND_PASTE].mods==3);
    for(int i=0;i<COMMAND_COUNT;i++) if(i!=COMMAND_COPY && i!=COMMAND_PASTE)
        assert(linux_state.bindings[i].key==mac.bindings[i].key && linux_state.bindings[i].mods==mac.bindings[i].mods);
    unlink(path);assert(shortcut_read(path,true,&loaded,error,sizeof(error)) && !memcmp(&loaded,&mac,sizeof(mac)));
    puts("PASS: platform navigation defaults, shortcut roundtrip, reserved controls, unknown/duplicate/version/truncated rejection, atomic private save, collisions, partial overrides, unbinding and transactional fallback");return 0;
}
