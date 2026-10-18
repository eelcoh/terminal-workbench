/* Reuse the versioned dotfile transaction, including validation of other sections. */
#include "font_bridge.h"
#include "../ghostling/portable_config.h"
bool workbench_font_defaults(const char *path,bool apple,int *family,int *size,char *error,size_t capacity)
{
    PortableConfig config;
    if(!portable_read(path,apple,&config,error,capacity))return false;
    *family=config.font.family;*size=config.font.size;return true;
}
