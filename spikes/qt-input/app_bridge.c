#include "app_bridge.h"
#include "core/portable_config.h"
int workbench_app_definitions(const char *path,bool apple,WorkbenchApp *out,int capacity,char *error,size_t length)
{
    PortableConfig config;PortableApps *apps=calloc(1,sizeof(*apps));
    if(!apps){snprintf(error,length,"Out of memory reading apps");return -1;}
    int count=-1;
    if(portable_read_full(path,apple,false,&config,apps,NULL,error,length)) {
        count=apps->count<capacity?apps->count:capacity;
        for(int i=0;i<count;i++) {
            const PortableApp *app=&apps->apps[i];
            snprintf(out[i].id,sizeof(out[i].id),"%s",app->id);snprintf(out[i].name,sizeof(out[i].name),"%s",app->name);
            snprintf(out[i].command,sizeof(out[i].command),"%s",app->command);snprintf(out[i].directory,sizeof(out[i].directory),"%s",app->directory);
            snprintf(out[i].install,sizeof(out[i].install),"%s",app->install);snprintf(out[i].setup,sizeof(out[i].setup),"%s",app->setup);
        }
    }
    free(apps);return count;
}
int workbench_host_definitions(const char *path,bool apple,WorkbenchHost *out,int capacity,char *error,size_t length)
{
    PortableConfig config;PortableHosts *hosts=calloc(1,sizeof(*hosts));
    if(!hosts){snprintf(error,length,"Out of memory reading hosts");return -1;}
    int count=-1;
    if(portable_read_full(path,apple,false,&config,NULL,hosts,error,length)) {
        count=hosts->count<capacity?hosts->count:capacity;
        for(int i=0;i<count;i++) {
            const PortableHost *host=&hosts->hosts[i];
            snprintf(out[i].id,sizeof(out[i].id),"%s",host->id);snprintf(out[i].name,sizeof(out[i].name),"%s",host->name);
            snprintf(out[i].destination,sizeof(out[i].destination),"%s",host->destination);snprintf(out[i].directory,sizeof(out[i].directory),"%s",host->directory);
            snprintf(out[i].theme,sizeof(out[i].theme),"%s",host->theme);
        }
    }
    free(hosts);return count;
}
