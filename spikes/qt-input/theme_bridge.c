/* Reuse the original bounded Ghostty parser and attributed offline catalog. */
#include "theme_bridge.h"
#include "../ghostling/portable_config.h"
bool workbench_theme_default_source(const char *path,bool apple,char *source,size_t length,char *error,size_t capacity) {
    PortableConfig config;
    if(!portable_read_with_theme_files(path,apple,false,&config,error,capacity))return false;
    if(snprintf(source,length,"%s",config.terminal_source)>=(int)length)return false;
    return true;
}
#define IMPORT_CAPACITY 512
/* GUI-thread registration, immutable slots; no parsing or allocation during paint. */
static WorkbenchTheme imports[IMPORT_CAPACITY];
static char import_names[IMPORT_CAPACITY][128];
static int import_count=0;
int workbench_theme_builtin_count(void) {return THEME_CATALOG_COUNT;}
int workbench_theme_count(void) {return THEME_CATALOG_COUNT+import_count;}
const char *workbench_theme_name(int index) {
    return index>=0 && index<THEME_CATALOG_COUNT ? theme_catalog_name(index)
        : index>=THEME_CATALOG_COUNT && index<workbench_theme_count() ? import_names[index-THEME_CATALOG_COUNT] : "";
}
static WorkbenchThemeColor rgb(ThemeRgb color) {return (WorkbenchThemeColor){color.r,color.g,color.b};}
static void convert(const Theme *theme,WorkbenchTheme *result) {
    result->background=rgb(theme->background);result->foreground=rgb(theme->foreground);result->cursor=rgb(theme->cursor);
    result->selection_background=rgb(theme->selection_background);result->selection_foreground=rgb(theme->selection_foreground);
    for(int i=0;i<256;++i)result->palette[i]=rgb(theme->palette[i]);
    result->light=theme_light(theme);
}
bool workbench_theme_get(int index,WorkbenchTheme *result) {
    if(!result || index<0 || index>=workbench_theme_count())return false;
    if(index>=THEME_CATALOG_COUNT) {*result=imports[index-THEME_CATALOG_COUNT];return true;}
    static Theme themes[THEME_CATALOG_COUNT];static int state=0;
    if(!state) {int ignored[THEME_CATALOG_COUNT];char error[256];state=theme_catalog_load(themes,ignored,error,sizeof(error))?1:-1;}
    if(state<0)return false;
    convert(&themes[index],result);return true;
}
bool workbench_theme_parse(const char *text,WorkbenchTheme *result,int *ignored,char *error,size_t capacity) {
    if(!text || !result || !ignored || !error || !capacity)return false;
    const Theme base=theme_preset(false);Theme parsed;
    if(!theme_read_text(text,&base,&parsed,ignored,error,capacity))return false;
    convert(&parsed,result);return true;
}
int workbench_theme_register(const char *name,const WorkbenchTheme *theme) {
    if(!name || !*name || strlen(name)>=sizeof(import_names[0]) || !theme || import_count>=IMPORT_CAPACITY)return -1;
    imports[import_count]=*theme;snprintf(import_names[import_count],sizeof(import_names[0]),"%s",name);
    return THEME_CATALOG_COUNT+import_count++;
}
