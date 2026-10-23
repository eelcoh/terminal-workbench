#ifndef WORKBENCH_FONT_PREFERENCES_H
#define WORKBENCH_FONT_PREFERENCES_H
#include <stdbool.h>
#include <strings.h>
#define TERMINAL_FONT_MIN 10
#define TERMINAL_FONT_MAX 32
#define TERMINAL_FONT_FAMILIES 2
typedef struct { int size,family; } FontPreferences;
static inline FontPreferences font_preferences_default(void) {return (FontPreferences){16,0};}
static inline bool font_preferences_valid(FontPreferences p)
{return p.size>=TERMINAL_FONT_MIN && p.size<=TERMINAL_FONT_MAX && p.family>=0 && p.family<TERMINAL_FONT_FAMILIES;}
static inline const char *font_preferences_name(int family)
{return family==1 ? "Fira Code" : "JetBrains Mono";}
static inline int font_preferences_family(const char *name)
{
    for (int i=0;i<TERMINAL_FONT_FAMILIES;i++) if (!strcasecmp(name,font_preferences_name(i))) return i;
    return -1;
}
static inline int font_preferences_step(int size,int direction)
{
    if (direction>0) return size<TERMINAL_FONT_MAX ? size+1 : TERMINAL_FONT_MAX;
    if (direction<0) return size>TERMINAL_FONT_MIN ? size-1 : TERMINAL_FONT_MIN;
    return size;
}
#endif
