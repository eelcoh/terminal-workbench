/* Reuse versioned field validation without resolving unused appearance files. */
#include "font_bridge.h"
#include "../ghostling/portable_config.h"
bool workbench_font_defaults(const char *path,bool apple,int *family,int *size,char *error,size_t capacity)
{
    PortableConfig config;
    if(!portable_read_with_theme_files(path,apple,false,&config,error,capacity))return false;
    *family=config.font.family;*size=config.font.size;return true;
}
