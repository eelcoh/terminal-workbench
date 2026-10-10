/* Offline gallery; source colors are loaded once, never inside a render loop. */
#include "theme_core.h"
#include "theme_catalog_data.h"
#define THEME_CATALOG_COUNT (2+(int)(sizeof(theme_bundled_names)/sizeof(theme_bundled_names[0])))
static const char *theme_catalog_name(int index)
{
    return index==0 ? "Workbench Night" : index==1 ? "Workbench Dawn" : index<THEME_CATALOG_COUNT ? theme_bundled_names[index-2] : "Custom colors";
}
static bool theme_catalog_load(Theme *themes, int *ignored, char *error, size_t capacity)
{
    themes[0]=theme_preset(false); themes[1]=theme_preset(true); ignored[0]=ignored[1]=0;
    for (int i=2;i<THEME_CATALOG_COUNT;i++)
        if (!theme_read_text(theme_bundled_sources[i-2],&themes[0],&themes[i],&ignored[i],error,capacity)) return false;
    return true;
}
static bool theme_catalog_matches(const char *name, const Theme *theme, const char *query, int mode)
{
    if (mode && theme_light(theme)!=(mode==2)) return false;
    for (const char *p=name;;p++) {
        const char *a=p,*b=query;
        while (*a && *b && tolower((unsigned char)*a)==tolower((unsigned char)*b)) { a++; b++; }
        if (!*b) return true;
        if (!*p) return false;
    }
}
