#include "shortcut_bridge.h"
#include "../ghostling/portable_config.h"
int workbench_shortcuts_read(const char *path,bool apple,WorkbenchShortcut *out,size_t count,char *error,size_t capacity)
{
    PortableConfig config;
    if(count<COMMAND_COUNT || !portable_read_with_theme_files(path,apple,false,&config,error,capacity))return -1;
    for(int i=0;i<COMMAND_COUNT;++i)out[i]=(WorkbenchShortcut){command_ids[i],config.shortcuts.bindings[i].key,config.shortcuts.bindings[i].mods};
    return COMMAND_COUNT;
}

static ShortcutState bridge_state(const WorkbenchShortcut *chords) {
    ShortcutState state={0};for(int i=0;i<COMMAND_COUNT;++i)state.bindings[i]=(ShortcutChord){chords[i].key,chords[i].mods};return state;
}
int workbench_shortcuts_layer(const char *path,bool apple,WorkbenchShortcut *chords,size_t count,char *error,size_t capacity) {
    if(count!=COMMAND_COUNT)return -1;
    ShortcutState base=bridge_state(chords),next;
    if(!shortcut_read_layer(path,apple,&base,&next,error,capacity))return -1;
    for(int i=0;i<COMMAND_COUNT;++i){chords[i].key=next.bindings[i].key;chords[i].mods=next.bindings[i].mods;}
    return COMMAND_COUNT;
}
bool workbench_shortcuts_valid(const WorkbenchShortcut *chords,size_t count,char *error,size_t capacity) {
    if(count!=COMMAND_COUNT)return false;
    ShortcutState next=bridge_state(chords);return shortcut_valid(&next,error,capacity);
}
bool workbench_shortcuts_save(const char *path,const WorkbenchShortcut *chords,const WorkbenchShortcut *base,size_t count,char *error,size_t capacity) {
    if(count!=COMMAND_COUNT)return false;
    ShortcutState next=bridge_state(chords),managed=bridge_state(base);return shortcut_write_layer(path,&next,&managed,error,capacity);
}
