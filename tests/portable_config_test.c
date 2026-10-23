#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include "portable_config.h"
static void write_text(const char *path,const char *text)
{ FILE *file=fopen(path,"w");assert(file);assert(fputs(text,file)!=EOF);assert(!fclose(file)); }
int main(void)
{
    char path[1024],ui[1024],theme[1024],error[256];
    snprintf(path,sizeof(path),"%s/config.conf",getenv("WB_TEST_DIRECTORY"));
    snprintf(ui,sizeof(ui),"%s/ui.conf",getenv("WB_TEST_DIRECTORY"));
    snprintf(theme,sizeof(theme),"%s/custom-theme",getenv("WB_TEST_DIRECTORY"));
    write_text(theme,"background = #123456\nforeground = #abcdef\n");
    write_text(path,"band-config-v1\n# comments stay managed\n[macos.shortcuts]\nforward = F9\n[shortcuts]\nforward = F8\n[appearance]\nterminal-theme = custom-theme\napplication-theme = Workbench Dawn\nfollow-workspace = true\n[linux.shortcuts]\nterminal.new = Ctrl+Alt+T\n");
    PortableConfig linux_config={0},mac={0};
    assert(portable_read(path,false,&linux_config,error,sizeof(error)));
    assert(portable_read(path,true,&mac,error,sizeof(error)));
    assert(linux_config.shortcuts.bindings[COMMAND_FORWARD].key==297);
    assert(mac.shortcuts.bindings[COMMAND_FORWARD].key==298);
    assert(linux_config.shortcuts.bindings[COMMAND_TERMINAL_CREATE].mods==5);
    assert(mac.shortcuts.bindings[COMMAND_TERMINAL_CREATE].mods==12);
    assert(linux_config.terminal.background.r==0x12 && linux_config.terminal.background.g==0x34);
    assert(theme_light(&linux_config.application) && linux_config.has_follow && linux_config.follow);
    write_text(path,"band-config-v1\n[macos.appearance]\napplication-theme = Workbench Night\nfollow-workspace = false\n[appearance]\napplication-theme = Workbench Dawn\nfollow-workspace = true\n");
    PortableConfig platform_colors;
    assert(portable_read(path,true,&platform_colors,error,sizeof(error)) && !theme_light(&platform_colors.application) && !platform_colors.follow);
    assert(portable_read(path,false,&platform_colors,error,sizeof(error)) && theme_light(&platform_colors.application) && platform_colors.follow);
    write_text(path,"band-config-v1\n[macos.font]\nsize = 20\n[font]\nfamily = Fira Code\nsize = 18\n[linux.font]\nfamily = JetBrains Mono\n");
    PortableConfig fonts;
    assert(portable_read(path,true,&fonts,error,sizeof(error)) && fonts.font.size==20 && fonts.font.family==1);
    assert(portable_read(path,false,&fonts,error,sizeof(error)) && fonts.font.size==18 && fonts.font.family==0);
    // Dotfiles written before the rename to Band keep working.
    write_text(path,"workbench-config-v1\n[font]\nsize = 19\n");
    assert(portable_read(path,false,&fonts,error,sizeof(error)) && fonts.font.size==19);
    write_text(path,"band-config-v1\n[shortcuts]\ncopy = Super+Alt+=\n");
    assert(portable_read(path,false,&fonts,error,sizeof(error)) && !fonts.shortcuts.bindings[COMMAND_FONT_INCREASE].key
        && fonts.shortcuts.bindings[COMMAND_COPY].key==61);
    write_text(path,"band-config-v1\n[macos.workspace]\nterminal-width = 2/3\n[workspace]\ndirectory = ~/Project folder\nterminal-width = 1/3\n[linux.workspace]\ndirectory = ../Projects\n");
    PortableConfig creation;
    assert(portable_read(path,true,&creation,error,sizeof(error)) && creation.terminal_width==2.0/3.0 && !strcmp(creation.workspace_directory,"~/Project folder"));
    assert(portable_read(path,false,&creation,error,sizeof(error)) && creation.terminal_width==1.0/3.0 && !strcmp(creation.workspace_directory,"../Projects"));
    assert(portable_read(NULL,false,&creation,error,sizeof(error)) && creation.terminal_width==0.5 && !creation.workspace_directory[0]);
    const char *full_widths[]={"1/1","3/3"};
    for(size_t i=0;i<sizeof(full_widths)/sizeof(*full_widths);++i){
        char full[128];snprintf(full,sizeof(full),"band-config-v1\n[workspace]\nterminal-width = %s\n",full_widths[i]);write_text(path,full);
        assert(portable_read(path,false,&creation,error,sizeof(error)) && creation.terminal_width==1.0);
        assert(portable_read(path,true,&creation,error,sizeof(error)) && creation.terminal_width==1.0);
    }
    write_text(path,"band-config-v1\n[macos.view]\nreduced-motion = false\n[view]\nnavigation-pinned = true\nreduced-motion = true\n[linux.view]\nnavigation-pinned = false\n");
    PortableConfig view;
    assert(portable_read(path,true,&view,error,sizeof(error)) && view.navigation_pinned && !view.reduced_motion);
    assert(portable_read(path,false,&view,error,sizeof(error)) && !view.navigation_pinned && view.reduced_motion);
    assert(portable_read(NULL,false,&view,error,sizeof(error)) && !view.navigation_pinned && !view.reduced_motion);
    PortableConfig unchanged=linux_config;
    const char *bad[]={
        "band-config-v1\n[view]\nnavigation-pinned = yes\n",
        "band-config-v1\n[view]\nreduced-motion = 1\n",
        "band-config-v1\n[view]\nreduced-motion = true\nreduced-motion = false\n",
        "band-config-v1\n[macos.view]\nunknown = false\n",
        "band-config-v1\n[workspace]\nterminal-width = 0.4\n",
        "band-config-v1\n[workspace]\nterminal-width = 1/4\n",
        "band-config-v1\n[workspace]\nterminal-width = 1/3\nterminal-width = 2/3\n",
        "band-config-v1\n[workspace]\ndirectory = first\ndirectory = second\n",
        "band-config-v1\n[workspace]\ndirectory = \n",
        "band-config-v1\n[workspace]\ndirectory = control\tcharacter\n",
        "band-config-v1\n[workspace]\ncommand = unexpected\n","workbench-config-v9\n","band-config-v1\n[font]\nsize = 99\n",
        "band-config-v1\n[font]\nsize = 9\n",
        "band-config-v1\n[font]\nsize = 18pt\n",
        "band-config-v1\n[font]\nfamily = absent\n",
        "band-config-v1\n[font]\nfamily = Fira Code\nfamily = JetBrains Mono\n",
        "band-config-v1\n[font]\nsize = 18\nsize = 20\n",
        "band-config-v1\n[shortcuts]\nforward = F8\nforward = F9\n",
        "band-config-v1\n[shortcuts]\ncopy = Ctrl+C\n",
        "band-config-v1\n[shortcuts]\nforward = Super+Alt+P\ncommands = Super+Alt+P\n",
        "band-config-v1\n[appearance]\nfollow-workspace = maybe\n",
        "band-config-v1\n[appearance]\nterminal-theme = absent\n",
        "band-config-v1\n[appearance]\nterminal-theme = Workbench Night"};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        write_text(path,bad[i]);assert(!portable_read(path,false,&linux_config,error,sizeof(error)));
        assert(!memcmp(&linux_config,&unchanged,sizeof(linux_config)) && error[0]);
    }
    /* UI edits inherit a base, and write only differences so unrelated file edits remain live on reopen. */
    ShortcutState changed=unchanged.shortcuts,loaded;changed.bindings[COMMAND_FORWARD]=(ShortcutChord){299,0};
    assert(shortcut_write_layer(ui,&changed,&unchanged.shortcuts,error,sizeof(error)));
    assert(shortcut_read_layer(ui,false,&unchanged.shortcuts,&loaded,error,sizeof(error)));
    assert(!memcmp(&loaded,&changed,sizeof(loaded)));
    ShortcutState new_base=unchanged.shortcuts;new_base.bindings[COMMAND_TERMINAL_CREATE]=(ShortcutChord){'Y',5};
    assert(shortcut_read_layer(ui,false,&new_base,&loaded,error,sizeof(error)));
    assert(loaded.bindings[COMMAND_TERMINAL_CREATE].key=='Y' && loaded.bindings[COMMAND_FORWARD].key==299);
    write_text(ui,"band-shortcuts-v1\nforward=Ctrl+Alt+Y\n");
    ShortcutState before=loaded;assert(!shortcut_read_layer(ui,false,&new_base,&loaded,error,sizeof(error)));
    assert(!memcmp(&loaded,&before,sizeof(loaded)));
    ShortcutState font_base=new_base;font_base.bindings[COMMAND_FONT_INCREASE]=(ShortcutChord){'Z',5};
    write_text(ui,"band-shortcuts-v1\nforward=Ctrl+Alt+Z\n");
    assert(!shortcut_read_layer(ui,false,&font_base,&loaded,error,sizeof(error)));
    assert(!memcmp(&loaded,&before,sizeof(loaded)));
    write_text(ui,"workbench-shortcuts-migrated-v1\ncopy=Super+Alt+R\n");
    assert(shortcut_read_layer(ui,false,&new_base,&loaded,error,sizeof(error)));
    assert(loaded.bindings[COMMAND_COPY].key=='R' && !loaded.bindings[COMMAND_WIDTH_CYCLE].key);
    write_text(ui,"band-shortcuts-v1\nforward=Super+Alt+Left\n");
    assert(shortcut_read_layer(ui,false,&new_base,&loaded,error,sizeof(error)));
    assert(!loaded.bindings[COMMAND_TERMINAL_PREVIOUS].key && loaded.bindings[COMMAND_FORWARD].mods==12);
    assert(loaded.bindings[COMMAND_TERMINAL_CREATE].key=='Y');
    assert(shortcut_write(ui,&new_base,error,sizeof(error)) && shortcut_read(ui,false,&loaded,error,sizeof(error)));
    char label[96];shortcut_label(loaded.bindings[COMMAND_TERMINAL_CREATE],false,label,sizeof(label));assert(!strcmp(label,"Ctrl+Alt+Y"));
    assert(theme_catalog_matches("Dawn",&unchanged.application,"daw",2));
    unlink(path);assert(portable_read(path,true,&mac,error,sizeof(error)) && !mac.has_application);
    puts("PASS: portable platform precedence, custom themes, transaction/error isolation, shortcut delta saves and inherited changes");
    return 0;
}
