/* Reuse the original bounded Ghostty parser and attributed offline catalog. */
#include "theme_bridge.h"
#include "../ghostling/theme_catalog.h"
int workbench_theme_count(void) {return THEME_CATALOG_COUNT;}
const char *workbench_theme_name(int index) {return index>=0 && index<THEME_CATALOG_COUNT ? theme_catalog_name(index) : "";}
static WorkbenchThemeColor rgb(ThemeRgb color) {return (WorkbenchThemeColor){color.r,color.g,color.b};}
bool workbench_theme_get(int index,WorkbenchTheme *result) {
    static Theme themes[THEME_CATALOG_COUNT];static int state=0;
    if(!state) {int ignored[THEME_CATALOG_COUNT];char error[256];state=theme_catalog_load(themes,ignored,error,sizeof(error))?1:-1;}
    if(state<0 || index<0 || index>=THEME_CATALOG_COUNT || !result)return false;
    const Theme *theme=&themes[index];
    result->background=rgb(theme->background);result->foreground=rgb(theme->foreground);result->cursor=rgb(theme->cursor);
    result->selection_background=rgb(theme->selection_background);result->selection_foreground=rgb(theme->selection_foreground);
    for(int i=0;i<256;++i)result->palette[i]=rgb(theme->palette[i]);
    result->light=theme_light(theme);return true;
}
