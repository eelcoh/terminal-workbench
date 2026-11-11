#ifndef WORKBENCH_FONT_PREFERENCES_H
#define WORKBENCH_FONT_PREFERENCES_H
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#define TERMINAL_FONT_MIN 10
#define TERMINAL_FONT_MAX 32
#define TERMINAL_FONT_FAMILIES 2
/* Any other family is an installed font, kept by name; the client checks it is
   installed and monospaced, and falls back to JetBrains Mono when it is not. */
#define TERMINAL_FONT_INSTALLED 2
#define TERMINAL_FONT_NAME_MAX 96
typedef struct { int size,family; char installed[TERMINAL_FONT_NAME_MAX]; } FontPreferences;
static inline FontPreferences font_preferences_default(void) {return (FontPreferences){.size=16,.family=0};}
static inline bool font_preferences_valid(FontPreferences p)
{return p.size>=TERMINAL_FONT_MIN && p.size<=TERMINAL_FONT_MAX && p.family>=0
    && (p.family<TERMINAL_FONT_FAMILIES || (p.family==TERMINAL_FONT_INSTALLED && p.installed[0]));}
/* A plausible installed family name: printable, no '#', shorter than the limit. */
static inline bool font_preferences_installed_name(const char *name)
{
    size_t length=strlen(name);
    if (!length || length>=TERMINAL_FONT_NAME_MAX) return false;
    for (size_t i=0;i<length;i++) if ((unsigned char)name[i]<0x20 || name[i]==0x7f || name[i]=='#') return false;
    return true;
}
static inline const char *font_preferences_name(int family)
{return family==1 ? "Fira Code" : "JetBrains Mono";}
static inline const char *font_preferences_label(const FontPreferences *p)
{return p->family==TERMINAL_FONT_INSTALLED ? p->installed : font_preferences_name(p->family);}
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
