#include "view_bridge.h"
#include "../ghostling/portable_config.h"
bool workbench_view_defaults(const char *path,bool apple,bool *pinned,bool *motion,char *error,size_t capacity)
{
    PortableConfig config;
    if(!portable_read_with_theme_files(path,apple,false,&config,error,capacity))return false;
    *pinned=config.navigation_pinned;*motion=config.reduced_motion;return true;
}
