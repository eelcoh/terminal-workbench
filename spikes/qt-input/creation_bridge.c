#include "creation_bridge.h"
#include "core/portable_config.h"
bool workbench_creation_defaults(const char *path,bool apple,char *directory,size_t length,double *width,char *error,size_t capacity)
{
    PortableConfig config;
    if(!portable_read_with_theme_files(path,apple,false,&config,error,capacity))return false;
    if(snprintf(directory,length,"%s",config.workspace_directory)>=(int)length){snprintf(error,capacity,"Workspace directory is too long");return false;}
    *width=config.terminal_width;return true;
}
