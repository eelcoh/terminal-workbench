/* Read-only portable preferences. Parsed once at startup, never in pane loops. */
#ifndef WORKBENCH_PORTABLE_CONFIG_H
#define WORKBENCH_PORTABLE_CONFIG_H
#include "theme_catalog.h"
#include "shortcut_state.h"
#include "font_preferences.h"
typedef struct {
    char application[1024],terminal[1024];
    bool follow,has_follow,seen_application,seen_terminal;
    ShortcutState shortcuts;
    bool seen[COMMAND_COUNT];
    FontPreferences font;bool has_size,has_family;
    char workspace_directory[1024];double terminal_width;bool has_directory,has_width;
    bool navigation_pinned,reduced_motion,has_navigation_pinned,has_reduced_motion;
    bool integrated_title_bar,has_integrated_title_bar;
} PortableLayer;
typedef struct {
    Theme application,terminal;
    char terminal_source[1024],workspace_directory[1024];
    double terminal_width;
    bool navigation_pinned,reduced_motion,integrated_title_bar;
    FontPreferences font;
    bool has_application,has_terminal,has_follow,follow;
    ShortcutState shortcuts;
} PortableConfig;
/* App definitions ([app.ID], [linux.app.ID], [macos.app.ID]); see docs/apps.md. */
#define PORTABLE_APP_MAX 32
#define PORTABLE_APP_SECTIONS (PORTABLE_APP_MAX*3)
/* install: your own recipe; setup: run after Band installs the app, or by hand. */
typedef struct { char id[41],name[64],command[1024],directory[1024],install[1024],setup[1024]; } PortableApp;
/* About 520 KB: callers that want apps allocate it; others pass NULL and only validate. */
typedef struct {
    int count;PortableApp apps[PORTABLE_APP_MAX];
    PortableApp sections[PORTABLE_APP_SECTIONS];
} PortableApps;
/* SSH hosts ([host.ID], [linux.host.ID], [macos.host.ID]); see docs/hosts.md.
 * IDs are letters, digits and dashes (at most 34) so pane app IDs "_host-ID"
 * and "_host-ID_2" fit the runtime's 40-character app IDs unambiguously. */
#define PORTABLE_HOST_MAX 32
#define PORTABLE_HOST_SECTIONS (PORTABLE_HOST_MAX*3)
typedef struct { char id[35],name[64],destination[256],directory[1024],theme[128]; } PortableHost;
typedef struct {
    int count;PortableHost hosts[PORTABLE_HOST_MAX];
    PortableHost sections[PORTABLE_HOST_SECTIONS];
} PortableHosts;
static bool portable_host_id(const char *text)
{
    size_t length=strlen(text);if (!length || length>34) return false;
    for (const char *p=text;*p;p++) if (!isalnum((unsigned char)*p) && *p!='-') return false;
    return true;
}
/* An ssh destination: an alias, host or user@host; never an option. */
static bool portable_destination(const char *text)
{
    size_t length=strlen(text);if (!length || length>255 || *text=='-') return false;
    for (const char *p=text;*p;p++) if (!isalnum((unsigned char)*p) && !strchr("._-@:[]%",*p)) return false;
    return true;
}
static bool portable_app_id(const char *text)
{
    /* A leading underscore is reserved for Band's own panes (hosts). */
    size_t length=strlen(text);if (!length || length>40 || *text=='_') return false;
    for (const char *p=text;*p;p++) if (!isalnum((unsigned char)*p) && *p!='-' && *p!='_') return false;
    return true;
}
static bool portable_plain(const char *value)
{
    for (const unsigned char *p=(const unsigned char *)value;*p;p++) if (*p<32 || *p==127) return false;
    return true;
}
static char *portable_trim(char *text)
{
    while (*text && isspace((unsigned char)*text)) text++;
    size_t length=strlen(text);
    while (length && isspace((unsigned char)text[length-1])) text[--length]=0;
    return text;
}
static bool portable_theme(const char *config,const char *value,Theme *out,char *error,size_t capacity)
{
    Theme themes[THEME_CATALOG_COUNT];int ignored[THEME_CATALOG_COUNT];
    if (!theme_catalog_load(themes,ignored,error,capacity)) return false;
    for (int i=0;i<THEME_CATALOG_COUNT;i++) if (!strcmp(value,theme_catalog_name(i))) { *out=themes[i];return true; }
    char path[2048];
    if (value[0]=='/') {
        if (snprintf(path,sizeof(path),"%s",value)>=(int)sizeof(path)) return false;
    } else {
        const char *slash=strrchr(config,'/');int prefix=slash ? (int)(slash-config+1) : 0;
        if (snprintf(path,sizeof(path),"%.*s%s",prefix,config,value)>=(int)sizeof(path)) return false;
    }
    FILE *file=fopen(path,"r");
    if (!file) { snprintf(error,capacity,"Cannot read configured theme: %s",strerror(errno));return false; }
    int count=0;bool valid=theme_read(file,&themes[0],out,&count,error,capacity);fclose(file);return valid;
}
/* Font-only clients validate fields without opening appearance assets they do not use.
 * When false, theme values are neither resolved nor advertised as loaded colors. */
static bool portable_read_full(const char *path,bool apple,bool load_theme_files,PortableConfig *out,PortableApps *apps,PortableHosts *hosts,char *error,size_t capacity)
{
    if (apps) apps->count=0;
    if (hosts) hosts->count=0;
    PortableConfig next={.shortcuts=shortcut_defaults(apple),.font=font_preferences_default(),.terminal_width=0.5};
    if (!path) { *out=next;return true; }
    FILE *file=fopen(path,"r");
    if (!file) { if (errno==ENOENT) { *out=next;return true; } snprintf(error,capacity,"Cannot read dotfiles: %s",strerror(errno));return false; }
    PortableLayer layers[3]={0};char line[2048];int section=-1,kind=0,number=1;
    struct { char id[41];int layer;bool name,command,directory,install,setup; } app_sections[PORTABLE_APP_SECTIONS];int app_count=0,app=-1;
    struct { char id[35];int layer;bool name,destination,directory,theme; } host_sections[PORTABLE_HOST_SECTIONS];int host_count=0,host=-1;
    // Files written before the rename to Band keep their workbench header.
    bool valid=fgets(line,sizeof(line),file) && (!strcmp(line,"band-config-v1\n") || !strcmp(line,"workbench-config-v1\n"));
    while (valid && fgets(line,sizeof(line),file)) {
        number++;if (number>512 || !strchr(line,'\n')) {valid=false;break;}
        char *text=portable_trim(line);if (!*text || *text=='#') continue;
        if (*text=='[') {
            app=-1;host=-1;size_t length=strlen(text);
            const char *host_prefixes[]={"[host.","[linux.host.","[macos.host."};int host_layer=-1;
            for (int i=0;i<3;i++) if (!strncmp(text,host_prefixes[i],strlen(host_prefixes[i]))) host_layer=i;
            if (host_layer>=0) {
                char *id=text+strlen(host_prefixes[host_layer]);
                if (text[length-1]!=']' || host_count>=PORTABLE_HOST_SECTIONS) {valid=false;break;}
                text[length-1]=0;if (!portable_host_id(id)) {valid=false;break;}
                for (int i=0;i<host_count;i++) if (host_sections[i].layer==host_layer && !strcmp(host_sections[i].id,id)) valid=false;
                if (!valid) break;
                host=host_count++;memset(&host_sections[host],0,sizeof(host_sections[host]));
                strcpy(host_sections[host].id,id);host_sections[host].layer=host_layer;
                if (hosts) {memset(&hosts->sections[host],0,sizeof(hosts->sections[host]));strcpy(hosts->sections[host].id,id);}
                section=host_layer;kind=6;continue;
            }
            const char *prefixes[]={"[app.","[linux.app.","[macos.app."};int layer=-1;
            for (int i=0;i<3;i++) if (!strncmp(text,prefixes[i],strlen(prefixes[i]))) layer=i;
            if (layer>=0) {
                char *id=text+strlen(prefixes[layer]);
                if (text[length-1]!=']' || app_count>=PORTABLE_APP_SECTIONS) {valid=false;break;}
                text[length-1]=0;if (!portable_app_id(id)) {valid=false;break;}
                for (int i=0;i<app_count;i++) if (app_sections[i].layer==layer && !strcmp(app_sections[i].id,id)) valid=false;
                if (!valid) break;
                app=app_count++;memset(&app_sections[app],0,sizeof(app_sections[app]));
                strcpy(app_sections[app].id,id);app_sections[app].layer=layer;
                if (apps) {memset(&apps->sections[app],0,sizeof(apps->sections[app]));strcpy(apps->sections[app].id,id);}
                section=layer;kind=5;continue;
            }
            static const char *sections[]={"[appearance]","[shortcuts]","[font]","[workspace]","[view]","[linux.appearance]","[linux.shortcuts]","[linux.font]","[linux.workspace]","[linux.view]","[macos.appearance]","[macos.shortcuts]","[macos.font]","[macos.workspace]","[macos.view]"};
            int found=-1;for (int i=0;i<15;i++) if (!strcmp(text,sections[i])) found=i;
            if (found<0) {valid=false;break;}section=found/5;kind=found%5;continue;
        }
        char *equal=strchr(text,'=');if (section<0 || !equal) {valid=false;break;}*equal++=0;
        char *key=portable_trim(text),*value=portable_trim(equal);PortableLayer *layer=&layers[section];
        if (!*value) {valid=false;break;}
        if (kind==6) {
            bool *seen=NULL;char *target=NULL;size_t size=0;PortableHost *values=hosts ? &hosts->sections[host] : NULL;bool ok=true;
            if (!strcmp(key,"name")) {seen=&host_sections[host].name;size=sizeof(values->name);if (values) target=values->name;}
            else if (!strcmp(key,"destination")) {seen=&host_sections[host].destination;size=sizeof(values->destination);if (values) target=values->destination;ok=portable_destination(value);}
            else if (!strcmp(key,"directory")) {seen=&host_sections[host].directory;size=sizeof(values->directory);if (values) target=values->directory;}
            else if (!strcmp(key,"theme")) {seen=&host_sections[host].theme;size=sizeof(values->theme);if (values) target=values->theme;}
            if (!seen || *seen || !ok || strlen(value)>=size || !portable_plain(value)) {valid=false;break;}
            *seen=true;if (target) strcpy(target,value);
        } else if (kind==5) {
            bool *seen=NULL;char *target=NULL;size_t size=0;PortableApp *values=apps ? &apps->sections[app] : NULL;
            if (!strcmp(key,"name")) {seen=&app_sections[app].name;size=sizeof(values->name);if (values) target=values->name;}
            else if (!strcmp(key,"command")) {seen=&app_sections[app].command;size=sizeof(values->command);if (values) target=values->command;}
            else if (!strcmp(key,"directory")) {seen=&app_sections[app].directory;size=sizeof(values->directory);if (values) target=values->directory;}
            else if (!strcmp(key,"install")) {seen=&app_sections[app].install;size=sizeof(values->install);if (values) target=values->install;}
            else if (!strcmp(key,"setup")) {seen=&app_sections[app].setup;size=sizeof(values->setup);if (values) target=values->setup;}
            if (!seen || *seen || strlen(value)>=size || !portable_plain(value)) {valid=false;break;}
            *seen=true;if (target) strcpy(target,value);
        } else if (kind==4) {
            bool *seen=NULL,*setting=NULL;
            if (!strcmp(key,"navigation-pinned")) {seen=&layer->has_navigation_pinned;setting=&layer->navigation_pinned;}
            else if (!strcmp(key,"reduced-motion")) {seen=&layer->has_reduced_motion;setting=&layer->reduced_motion;}
            else if (!strcmp(key,"integrated-title-bar")) {seen=&layer->has_integrated_title_bar;setting=&layer->integrated_title_bar;}
            if (!seen || *seen || (strcmp(value,"true") && strcmp(value,"false"))) {valid=false;break;}
            *seen=true;*setting=!strcmp(value,"true");
        } else if (kind==3) {
            if (!strcmp(key,"directory")) {
                if (layer->has_directory || strlen(value)>=sizeof(layer->workspace_directory)) {valid=false;break;}
                for (const unsigned char *p=(const unsigned char *)value;*p;p++) if (*p<32 || *p==127) {valid=false;break;}
                if (!valid) break;
                strcpy(layer->workspace_directory,value);layer->has_directory=true;
            } else if (!strcmp(key,"terminal-width")) {
                if (layer->has_width) {valid=false;break;}
                if (!strcmp(value,"1/3")) layer->terminal_width=1.0/3.0;
                else if (!strcmp(value,"1/2")) layer->terminal_width=0.5;
                else if (!strcmp(value,"2/3")) layer->terminal_width=2.0/3.0;
                else if (!strcmp(value,"1/1") || !strcmp(value,"3/3")) layer->terminal_width=1.0;
                else {valid=false;break;}
                layer->has_width=true;
            } else {valid=false;break;}
        } else if (kind==2) {
            if (!strcmp(key,"size")) {
                char *end=NULL;long size=strtol(value,&end,10);
                if (layer->has_size || end==value || *end || size<TERMINAL_FONT_MIN || size>TERMINAL_FONT_MAX) {valid=false;break;}
                layer->font.size=(int)size;layer->has_size=true;
            } else if (!strcmp(key,"family")) {
                int family=font_preferences_family(value);
                if (layer->has_family || family<0) {valid=false;break;}
                layer->font.family=family;layer->has_family=true;
            } else {valid=false;break;}
        } else if (kind==1) {
            int action=-1;for (int i=0;i<COMMAND_COUNT;i++) if (!strcmp(key,command_ids[i])) action=i;
            if (action<0 || layer->seen[action] || !shortcut_parse(value,&layer->shortcuts.bindings[action])) {valid=false;break;}
            layer->seen[action]=true;
        } else if (!strcmp(key,"application-theme") || !strcmp(key,"terminal-theme")) {
            bool application=!strcmp(key,"application-theme");bool *seen=application ? &layer->seen_application : &layer->seen_terminal;
            if (*seen || strlen(value)>=1024) {valid=false;break;}*seen=true;
            strcpy(application ? layer->application : layer->terminal,value);
        } else if (!strcmp(key,"follow-workspace")) {
            if (layer->has_follow || (strcmp(value,"true") && strcmp(value,"false"))) {valid=false;break;}
            layer->has_follow=true;layer->follow=!strcmp(value,"true");
        } else {valid=false;break;}
    }
    if (ferror(file)) valid=false;
    fclose(file);
    if (!valid) {snprintf(error,capacity,"Invalid dotfile near line %d; preferences kept",number);return false;}
    /* Every app has a command on both platforms: a platform section either
     * overrides an [app.ID] or defines a platform-only app in full. */
    int distinct=0;
    for (int i=0;i<app_count;i++) {
        int base=-1,first=i;
        for (int j=0;j<app_count;j++) if (!strcmp(app_sections[j].id,app_sections[i].id)) {
            if (app_sections[j].layer==0) base=j;
            if (j<first) first=j;
        }
        if (first==i) distinct++;
        if (!app_sections[i].command && (app_sections[i].layer==0 || base<0)) {snprintf(error,capacity,"App %s needs a command; preferences kept",app_sections[i].id);return false;}
    }
    if (distinct>PORTABLE_APP_MAX) {snprintf(error,capacity,"At most %d apps can be defined; preferences kept",PORTABLE_APP_MAX);return false;}
    /* Hosts follow the same rule with a destination. */
    int distinct_hosts=0;
    for (int i=0;i<host_count;i++) {
        int base=-1,first=i;
        for (int j=0;j<host_count;j++) if (!strcmp(host_sections[j].id,host_sections[i].id)) {
            if (host_sections[j].layer==0) base=j;
            if (j<first) first=j;
        }
        if (first==i) distinct_hosts++;
        if (!host_sections[i].destination && (host_sections[i].layer==0 || base<0)) {snprintf(error,capacity,"Host %s needs a destination; preferences kept",host_sections[i].id);return false;}
    }
    if (distinct_hosts>PORTABLE_HOST_MAX) {snprintf(error,capacity,"At most %d hosts can be defined; preferences kept",PORTABLE_HOST_MAX);return false;}
    if (hosts) {
        int layer=apple ? 2 : 1;
        for (int i=0;i<host_count;i++) {
            int base=-1,platform=-1,first=i;
            for (int j=0;j<host_count;j++) if (!strcmp(host_sections[j].id,host_sections[i].id)) {
                if (host_sections[j].layer==0) base=j;
                if (host_sections[j].layer==layer) platform=j;
                if (j<first) first=j;
            }
            if (first!=i || (base<0 && platform<0)) continue;
            PortableHost *result=&hosts->hosts[hosts->count++];*result=hosts->sections[base>=0 ? base : platform];
            if (platform>=0 && base>=0) {
                const PortableHost *over=&hosts->sections[platform];
                if (host_sections[platform].name) strcpy(result->name,over->name);
                if (host_sections[platform].destination) strcpy(result->destination,over->destination);
                if (host_sections[platform].directory) strcpy(result->directory,over->directory);
                if (host_sections[platform].theme) strcpy(result->theme,over->theme);
            }
            if (!result->name[0]) memcpy(result->name,result->id,strlen(result->id)+1);
        }
    }
    if (apps) {
        int layer=apple ? 2 : 1;
        for (int i=0;i<app_count;i++) {
            int base=-1,platform=-1,first=i;
            for (int j=0;j<app_count;j++) if (!strcmp(app_sections[j].id,app_sections[i].id)) {
                if (app_sections[j].layer==0) base=j;
                if (app_sections[j].layer==layer) platform=j;
                if (j<first) first=j;
            }
            if (first!=i || (base<0 && platform<0)) continue;
            PortableApp *result=&apps->apps[apps->count++];*result=apps->sections[base>=0 ? base : platform];
            if (platform>=0 && base>=0) {
                const PortableApp *over=&apps->sections[platform];
                if (app_sections[platform].name) strcpy(result->name,over->name);
                if (app_sections[platform].command) strcpy(result->command,over->command);
                if (app_sections[platform].directory) strcpy(result->directory,over->directory);
                if (app_sections[platform].install) strcpy(result->install,over->install);
                if (app_sections[platform].setup) strcpy(result->setup,over->setup);
            }
            if (!result->name[0]) memcpy(result->name,result->id,strlen(result->id)+1); /* IDs fit names */
        }
    }
    PortableLayer *platform=&layers[apple ? 2 : 1];
    next.navigation_pinned=platform->has_navigation_pinned ? platform->navigation_pinned : layers[0].navigation_pinned;
    next.reduced_motion=platform->has_reduced_motion ? platform->reduced_motion : layers[0].reduced_motion;
    next.integrated_title_bar=platform->has_integrated_title_bar ? platform->integrated_title_bar : layers[0].integrated_title_bar;
    snprintf(next.workspace_directory,sizeof(next.workspace_directory),"%s",platform->has_directory ? platform->workspace_directory : layers[0].workspace_directory);
    if (layers[0].has_width) next.terminal_width=layers[0].terminal_width;
    if (platform->has_width) next.terminal_width=platform->terminal_width;
    const char *application=platform->seen_application ? platform->application : layers[0].application;
    const char *terminal=platform->seen_terminal ? platform->terminal : layers[0].terminal;
    snprintf(next.terminal_source,sizeof(next.terminal_source),"%s",terminal);
    next.has_application=load_theme_files && *application;next.has_terminal=load_theme_files && *terminal;
    if ((next.has_application && !portable_theme(path,application,&next.application,error,capacity))
        || (next.has_terminal && !portable_theme(path,terminal,&next.terminal,error,capacity))) return false;
    if (layers[0].has_size) next.font.size=layers[0].font.size;
    if (layers[0].has_family) next.font.family=layers[0].font.family;
    if (platform->has_size) next.font.size=platform->font.size;
    if (platform->has_family) next.font.family=platform->font.family;
    next.has_follow=platform->has_follow || layers[0].has_follow;
    next.follow=platform->has_follow ? platform->follow : layers[0].follow;
    for (int i=0;i<COMMAND_COUNT;i++) {
        if (layers[0].seen[i]) next.shortcuts.bindings[i]=layers[0].shortcuts.bindings[i];
        if (platform->seen[i]) next.shortcuts.bindings[i]=platform->shortcuts.bindings[i];
    }
    for (int i=COMMAND_FONT_SETTINGS;i<COMMAND_COUNT;i++) if (!layers[0].seen[i] && !platform->seen[i])
        for (int j=0;j<COMMAND_COUNT;j++) if ((layers[0].seen[j] || platform->seen[j]) && next.shortcuts.bindings[i].key
            && next.shortcuts.bindings[i].key==next.shortcuts.bindings[j].key && next.shortcuts.bindings[i].mods==next.shortcuts.bindings[j].mods)
            next.shortcuts.bindings[i]=(ShortcutChord){0};
    if (!shortcut_valid(&next.shortcuts,error,capacity)) return false;
    *out=next;error[0]=0;return true;
}
static bool portable_read_with_theme_files(const char *path,bool apple,bool load_theme_files,PortableConfig *out,char *error,size_t capacity)
{
    return portable_read_full(path,apple,load_theme_files,out,NULL,NULL,error,capacity);
}
/* The working viewer retains full theme validation and its transaction boundary. */
static inline bool portable_read(const char *path,bool apple,PortableConfig *out,char *error,size_t capacity)
{
    return portable_read_with_theme_files(path,apple,true,out,error,capacity);
}
#endif
