#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <sys/stat.h>
#include "shortcut_state.h"
int main(void)
{
    ShortcutState linux_state=shortcut_defaults(false),mac=shortcut_defaults(true);char error[256];
    assert(shortcut_valid(&linux_state,error,sizeof(error)) && shortcut_valid(&mac,error,sizeof(error)));
    assert(linux_state.bindings[COMMAND_TERMINAL_NEXT].key==262 && linux_state.bindings[COMMAND_TERMINAL_NEXT].mods==(SHORTCUT_CTRL|SHORTCUT_SHIFT));
    assert(mac.bindings[COMMAND_WORKSPACE_PREVIOUS].key==265 && mac.bindings[COMMAND_WORKSPACE_PREVIOUS].mods==(SHORTCUT_SUPER|SHORTCUT_ALT));
    assert(linux_state.bindings[COMMAND_WIDTH_CYCLE].key=='R' && linux_state.bindings[COMMAND_WIDTH_CYCLE].mods==(SHORTCUT_CTRL|SHORTCUT_SHIFT));
    assert(linux_state.bindings[COMMAND_WIDTH_FULL].key=='F' && linux_state.bindings[COMMAND_WIDTH_FULL].mods==(SHORTCUT_CTRL|SHORTCUT_SHIFT));
    assert(mac.bindings[COMMAND_WIDTH_CYCLE].key=='R' && mac.bindings[COMMAND_WIDTH_CYCLE].mods==mac.bindings[COMMAND_TERMINAL_NEXT].mods);
    assert(mac.bindings[COMMAND_WIDTH_FULL].key=='F' && mac.bindings[COMMAND_WIDTH_FULL].mods==mac.bindings[COMMAND_TERMINAL_NEXT].mods);
    for (int i=0;i<COMMAND_COUNT;i++) if (mac.bindings[i].key)
        assert(mac.bindings[i].mods==(i==COMMAND_COPY || i==COMMAND_PASTE ? SHORTCUT_SUPER : SHORTCUT_SUPER|SHORTCUT_ALT));
    assert(mac.bindings[COMMAND_PALETTE].key=='P' && mac.bindings[COMMAND_OVERVIEW].key=='O');
    assert(mac.bindings[COMMAND_TERMINAL_CREATE].key=='T' && mac.bindings[COMMAND_WORKSPACE_CREATE].key=='N');
    assert(linux_state.bindings[COMMAND_HELP].key==47 && linux_state.bindings[COMMAND_HELP].mods==(SHORTCUT_CTRL|SHORTCUT_SHIFT));
    assert(mac.bindings[COMMAND_HELP].key==47);
    ShortcutChord chord={0};assert(shortcut_parse("Command+Option+Left",&chord) && chord.mods==12);
    char label[96];shortcut_label(chord,true,label,sizeof(label));assert(!strcmp(label,"Command+Option+Left"));
    for (int i=0;i<COMMAND_COUNT;i++) { shortcut_label(linux_state.bindings[i],false,label,sizeof(label));ShortcutChord roundtrip;assert(shortcut_parse(label,&roundtrip));assert(roundtrip.key==linux_state.bindings[i].key && roundtrip.mods==linux_state.bindings[i].mods); }
    const char *bad[]={"Ctrl+C","Ctrl+D","Ctrl+Z","P","Shift+Left","Escape","Ctrl+F12","Ctrl+Ctrl+P","Ctrl+P+Shift","Ctrl+","Alt+unknown","Ctrl+Alt+Shift+Super+Shift+P"};
    for (size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) assert(!shortcut_parse(bad[i],&chord));
    assert(shortcut_parse("Ctrl+Shift+/",&chord) && chord.key==47 && chord.mods==3);
    assert(shortcut_parse("Command+Option+Slash",&chord) && chord.key==47 && chord.mods==12);
    shortcut_label(chord,true,label,sizeof(label));assert(!strcmp(label,"Command+Option+/"));
    assert(!shortcut_parse("Ctrl+Shift+?",&chord) && !shortcut_parse("/",&chord));
    assert(shortcut_parse("F8",&chord));assert(shortcut_parse("none",&chord));
    char path[1024];snprintf(path,sizeof(path),"%s/shortcuts.conf",getenv("WB_TEST_DIRECTORY"));
    assert(shortcut_write(path,&linux_state,error,sizeof(error)));struct stat statbuf;assert(!stat(path,&statbuf) && (statbuf.st_mode&0777)==0600);
    ShortcutState loaded={0};assert(shortcut_read(path,false,&loaded,error,sizeof(error)));assert(!memcmp(&loaded,&linux_state,sizeof(loaded)));
    ShortcutState collision=linux_state;collision.bindings[COMMAND_WORKSPACE_NEXT]=collision.bindings[COMMAND_TERMINAL_NEXT];
    assert(!shortcut_write(path,&collision,error,sizeof(error)));assert(strstr(error,"Next terminal"));
    assert(shortcut_read(path,false,&loaded,error,sizeof(error)) && !memcmp(&loaded,&linux_state,sizeof(loaded)));
    const char *invalid[]={"workbench-shortcuts-v99\n", "workbench-shortcuts-v1\ncopy=Ctrl+C\n", "workbench-shortcuts-v1\nunknown=F8\n", "workbench-shortcuts-v1\ncopy=F8\ncopy=F9\n", "workbench-shortcuts-v1\nterminal.next=Ctrl+Shift+P\n", "workbench-shortcuts-v1\ncopy=F8"};
    for (size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) { FILE *file=fopen(path,"w");assert(file);fputs(invalid[i],file);fclose(file);assert(!shortcut_read(path,false,&loaded,error,sizeof(error)));assert(!memcmp(&loaded,&linux_state,sizeof(loaded))); }
    FILE *file=fopen(path,"w");assert(file);fputs("workbench-shortcuts-v1\n# partial overrides\ncommands=none\ncopy=Ctrl+Shift+P\n",file);fclose(file);
    assert(shortcut_read(path,false,&loaded,error,sizeof(error)));assert(!loaded.bindings[COMMAND_PALETTE].key && loaded.bindings[COMMAND_COPY].key=='P');
    file=fopen(path,"w");assert(file);fputs("workbench-shortcuts-v1\ncopy=Ctrl+Shift+R\n",file);fclose(file);
    assert(shortcut_read(path,false,&loaded,error,sizeof(error)));
    assert(loaded.bindings[COMMAND_COPY].key=='R' && !loaded.bindings[COMMAND_WIDTH_CYCLE].key);
    assert(loaded.bindings[COMMAND_WIDTH_FULL].key=='F');
    file=fopen(path,"w");assert(file);fputs("workbench-shortcuts-v1\ncopy=Ctrl+Shift+/\n",file);fclose(file);
    assert(shortcut_read(path,false,&loaded,error,sizeof(error)) && !loaded.bindings[COMMAND_HELP].key);
    file=fopen(path,"w");assert(file);fputs("workbench-shortcuts-v1\ncommands=Command+Shift+P\noverview=Command+Shift+O\nterminal.new=Command+T\n",file);fclose(file);
    assert(shortcut_read(path,true,&loaded,error,sizeof(error)) && loaded.bindings[COMMAND_PALETTE].mods==(SHORTCUT_SUPER|SHORTCUT_SHIFT));
    assert(loaded.bindings[COMMAND_OVERVIEW].mods==(SHORTCUT_SUPER|SHORTCUT_SHIFT) && loaded.bindings[COMMAND_TERMINAL_CREATE].mods==SHORTCUT_SUPER);
    file=fopen(path,"w");assert(file);fputs("workbench-shortcuts-v1\ncopy=Command+Option+P\n",file);fclose(file);
    assert(shortcut_read(path,true,&loaded,error,sizeof(error)) && !loaded.bindings[COMMAND_PALETTE].key);
    assert(loaded.bindings[COMMAND_COPY].mods==(SHORTCUT_SUPER|SHORTCUT_ALT));
    assert(!shortcut_read(getenv("WB_TEST_DIRECTORY"),false,&loaded,error,sizeof(error)));
    unlink(path);assert(shortcut_read(path,true,&loaded,error,sizeof(error)) && !memcmp(&loaded,&mac,sizeof(mac)));
    puts("PASS: platform navigation defaults, shortcut roundtrip, reserved controls, unknown/duplicate/version/truncated rejection, atomic private save, collisions, partial overrides, unbinding and transactional fallback");return 0;
}
