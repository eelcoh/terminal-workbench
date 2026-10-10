/* Client presentation preferences. No runtime/session metadata is changed. */
#ifndef WORKBENCH_APPEARANCE_STATE_H
#define WORKBENCH_APPEARANCE_STATE_H
#include "theme_core.h"
#define APPEARANCE_WORKSPACES 8
#define APPEARANCE_TERMINALS 64
typedef struct { char id[41]; Theme theme; } AppearanceBinding;
typedef struct {
    Theme application, defaults;
    bool follow;
    AppearanceBinding workspaces[APPEARANCE_WORKSPACES], terminals[APPEARANCE_TERMINALS];
    int workspace_count, terminal_count;
} AppearanceState;
static AppearanceState appearance_default(Theme theme)
{
    AppearanceState state={0}; state.application=state.defaults=theme; return state;
}
static const Theme *appearance_binding(const AppearanceBinding *bindings, int count, const char *id)
{
    for (int i=0;i<count;i++) if (!strcmp(bindings[i].id,id)) return &bindings[i].theme;
    return NULL;
}
static bool appearance_identity(const char *id)
{
    size_t length=strlen(id);
    if (!length || length>40) return false;
    for (size_t i=0;i<length;i++) if ((unsigned char)id[i]<32 || id[i]==127) return false;
    return true;
}
static bool appearance_set(AppearanceBinding *bindings, int *count, int maximum, const char *id, const Theme *theme)
{
    if (!appearance_identity(id)) return false;
    int index=0;
    while (index<*count && strcmp(bindings[index].id,id)) index++;
    if (!theme) {
        if (index<*count) { memmove(&bindings[index],&bindings[index+1],(size_t)(*count-index-1)*sizeof(*bindings)); (*count)--; }
        return true;
    }
    if (index==*count) { if (*count==maximum) return false; (*count)++; strcpy(bindings[index].id,id); }
    bindings[index].theme=*theme; return true;
}
static const Theme *appearance_workspace(const AppearanceState *state, const char *workspace)
{
    const Theme *theme=appearance_binding(state->workspaces,state->workspace_count,workspace);
    return theme ? theme : &state->defaults;
}
static const Theme *appearance_terminal(const AppearanceState *state, const char *workspace, const char *terminal)
{
    const Theme *theme=appearance_binding(state->terminals,state->terminal_count,terminal);
    return theme ? theme : appearance_workspace(state,workspace);
}
static const Theme *appearance_chrome(const AppearanceState *state, const char *workspace)
{
    return state->follow ? appearance_workspace(state,workspace) : &state->application;
}
static bool appearance_write_theme(FILE *file, const Theme *t)
{
    if (fprintf(file,"background = #%02x%02x%02x\nforeground = #%02x%02x%02x\ncursor-color = #%02x%02x%02x\nselection-background = #%02x%02x%02x\nselection-foreground = #%02x%02x%02x\n",t->background.r,t->background.g,t->background.b,t->foreground.r,t->foreground.g,t->foreground.b,t->cursor.r,t->cursor.g,t->cursor.b,t->selection_background.r,t->selection_background.g,t->selection_background.b,t->selection_foreground.r,t->selection_foreground.g,t->selection_foreground.b)<0) return false;
    for (int i=0;i<256;i++) if (fprintf(file,"palette = %d=#%02x%02x%02x\n",i,t->palette[i].r,t->palette[i].g,t->palette[i].b)<0) return false;
    return true;
}
static bool appearance_write(FILE *file, const AppearanceState *state)
{
    if (fprintf(file,"workbench-appearance-v1\nfollow\t%d\n@application\n",state->follow ? 1 : 0)<0 || !appearance_write_theme(file,&state->application)
        || fputs("@default\n",file)==EOF || !appearance_write_theme(file,&state->defaults)) return false;
    for (int i=0;i<state->workspace_count;i++)
        if (fprintf(file,"@workspace\t%s\n",state->workspaces[i].id)<0 || !appearance_write_theme(file,&state->workspaces[i].theme)) return false;
    for (int i=0;i<state->terminal_count;i++)
        if (fprintf(file,"@terminal\t%s\n",state->terminals[i].id)<0 || !appearance_write_theme(file,&state->terminals[i].theme)) return false;
    return !ferror(file);
}
static bool appearance_section(AppearanceState *state, const char *header, const char *text, bool *application, bool *defaults, char *error, size_t capacity)
{
    Theme base=theme_preset(false),theme; int ignored=0;
    if (!theme_read_text(text,&base,&theme,&ignored,error,capacity)) return false;
    if (!strcmp(header,"@application")) { if (*application) return false; *application=true; state->application=theme; return true; }
    if (!strcmp(header,"@default")) { if (*defaults) return false; *defaults=true; state->defaults=theme; return true; }
    bool workspace=!strncmp(header,"@workspace\t",11), terminal=!strncmp(header,"@terminal\t",10);
    if (!workspace && !terminal) return false;
    const char *id=header+(workspace ? 11 : 10);
    AppearanceBinding *bindings=workspace ? state->workspaces : state->terminals;
    int *count=workspace ? &state->workspace_count : &state->terminal_count;
    if (appearance_binding(bindings,*count,id)) return false;
    return appearance_set(bindings,count,workspace ? APPEARANCE_WORKSPACES : APPEARANCE_TERMINALS,id,&theme);
}
static bool appearance_read(FILE *file, AppearanceState *result, char *error, size_t capacity)
{
    AppearanceState next=appearance_default(theme_preset(false));
    char line[1024],header[64]={0},block[16384]={0}; size_t used=0;
    bool application=false,defaults=false;
    if (!fgets(line,sizeof(line),file) || strcmp(line,"workbench-appearance-v1\n")) goto invalid;
    if (!fgets(line,sizeof(line),file)) goto invalid;
    if (!strcmp(line,"follow\t1\n")) next.follow=true;
    else if (strcmp(line,"follow\t0\n")) goto invalid;
    while (fgets(line,sizeof(line),file)) {
        if (!strchr(line,'\n')) goto invalid;
        if (line[0]=='@') {
            if (header[0] && !appearance_section(&next,header,block,&application,&defaults,error,capacity)) goto invalid;
            line[strcspn(line,"\r\n")]='\0';
            if (strlen(line)>=sizeof(header)) goto invalid;
            strcpy(header,line); used=0; block[0]='\0';
        } else {
            size_t length=strlen(line);
            if (!header[0] || used+length>=sizeof(block)) goto invalid;
            memcpy(block+used,line,length+1); used+=length;
        }
    }
    if (ferror(file) || !header[0] || !appearance_section(&next,header,block,&application,&defaults,error,capacity) || !application || !defaults) goto invalid;
    *result=next; error[0]='\0'; return true;
invalid:
    snprintf(error,capacity,"Invalid saved appearance; existing preferences preserved"); return false;
}
#endif
