#include "shortcut_bridge.h"
#include "../ghostling/portable_config.h"
int workbench_shortcuts_read(const char *path,bool apple,WorkbenchShortcut *out,size_t count,char *error,size_t capacity)
{
    PortableConfig config;
    if(count<COMMAND_COUNT || !portable_read_with_theme_files(path,apple,false,&config,error,capacity))return -1;
    for(int i=0;i<COMMAND_COUNT;++i)out[i]=(WorkbenchShortcut){command_ids[i],config.shortcuts.bindings[i].key,config.shortcuts.bindings[i].mods};
    return COMMAND_COUNT;
}
