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
} PortableLayer;
typedef struct {
    Theme application,terminal;
    char terminal_source[1024],workspace_directory[1024];
    double terminal_width;
    bool navigation_pinned,reduced_motion;
    FontPreferences font;
    bool has_application,has_terminal,has_follow,follow;
    ShortcutState shortcuts;
} PortableConfig;
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
static bool portable_read_with_theme_files(const char *path,bool apple,bool load_theme_files,PortableConfig *out,char *error,size_t capacity)
{
    PortableConfig next={.shortcuts=shortcut_defaults(apple),.font=font_preferences_default(),.terminal_width=0.5};
    if (!path) { *out=next;return true; }
    FILE *file=fopen(path,"r");
    if (!file) { if (errno==ENOENT) { *out=next;return true; } snprintf(error,capacity,"Cannot read dotfiles: %s",strerror(errno));return false; }
    PortableLayer layers[3]={0};char line[2048];int section=-1,kind=0,number=1;
    // Files written before the rename to Band keep their workbench header.
    bool valid=fgets(line,sizeof(line),file) && (!strcmp(line,"band-config-v1\n") || !strcmp(line,"workbench-config-v1\n"));
    while (valid && fgets(line,sizeof(line),file)) {
        number++;if (number>512 || !strchr(line,'\n')) {valid=false;break;}
        char *text=portable_trim(line);if (!*text || *text=='#') continue;
        if (*text=='[') {
            static const char *sections[]={"[appearance]","[shortcuts]","[font]","[workspace]","[view]","[linux.appearance]","[linux.shortcuts]","[linux.font]","[linux.workspace]","[linux.view]","[macos.appearance]","[macos.shortcuts]","[macos.font]","[macos.workspace]","[macos.view]"};
            int found=-1;for (int i=0;i<15;i++) if (!strcmp(text,sections[i])) found=i;
            if (found<0) {valid=false;break;}section=found/5;kind=found%5;continue;
        }
        char *equal=strchr(text,'=');if (section<0 || !equal) {valid=false;break;}*equal++=0;
        char *key=portable_trim(text),*value=portable_trim(equal);PortableLayer *layer=&layers[section];
        if (!*value) {valid=false;break;}
        if (kind==4) {
            bool *seen=NULL,*setting=NULL;
            if (!strcmp(key,"navigation-pinned")) {seen=&layer->has_navigation_pinned;setting=&layer->navigation_pinned;}
            else if (!strcmp(key,"reduced-motion")) {seen=&layer->has_reduced_motion;setting=&layer->reduced_motion;}
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
    PortableLayer *platform=&layers[apple ? 2 : 1];
    next.navigation_pinned=platform->has_navigation_pinned ? platform->navigation_pinned : layers[0].navigation_pinned;
    next.reduced_motion=platform->has_reduced_motion ? platform->reduced_motion : layers[0].reduced_motion;
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
/* The working viewer retains full theme validation and its transaction boundary. */
static inline bool portable_read(const char *path,bool apple,PortableConfig *out,char *error,size_t capacity)
{
    return portable_read_with_theme_files(path,apple,true,out,error,capacity);
}
#endif
