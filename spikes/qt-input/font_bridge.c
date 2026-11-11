/* Reuse versioned field validation without resolving unused appearance files. */
#include "font_bridge.h"
#include "core/portable_config.h"
bool workbench_font_defaults(const char *path,bool apple,char *family,size_t length,int *size,char *error,size_t capacity)
{
    PortableConfig config;
    if(!portable_read_with_theme_files(path,apple,false,&config,error,capacity))return false;
    snprintf(family,length,"%s",font_preferences_label(&config.font));*size=config.font.size;return true;
}
