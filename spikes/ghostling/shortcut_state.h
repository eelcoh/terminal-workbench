/* Bounded client preferences; no terminal/session facts live here. */
#ifndef WORKBENCH_SHORTCUT_STATE_H
#define WORKBENCH_SHORTCUT_STATE_H
#include "command_actions.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#define SHORTCUT_CTRL 1u
#define SHORTCUT_SHIFT 2u
#define SHORTCUT_ALT 4u
#define SHORTCUT_SUPER 8u
typedef struct { int key; unsigned mods; } ShortcutChord;
typedef struct { ShortcutChord bindings[COMMAND_COUNT]; } ShortcutState;
static const struct { const char *name; int key; } shortcut_keys[]={
    {"Left",263},{"Right",262},{"Up",265},{"Down",264},{"Space",32},{"Tab",258},
    {"Enter",257},{"Backspace",259},{"Delete",261},{"Home",268},{"End",269},
    {"PageUp",266},{"PageDown",267},{"Insert",260},
    {"F1",290},{"F2",291},{"F3",292},{"F4",293},{"F5",294},{"F6",295},
    {"F7",296},{"F8",297},{"F9",298},{"F10",299},{"F11",300}
};
static int shortcut_key(const char *name)
{
    if (strlen(name)==1) {
        unsigned char ch=(unsigned char)name[0];
        if (ch>='a' && ch<='z') ch-=32;
        if ((ch>='A' && ch<='Z') || (ch>='0' && ch<='9')) return ch;
    }
    for (size_t i=0;i<sizeof(shortcut_keys)/sizeof(shortcut_keys[0]);i++)
        if (!strcasecmp(name,shortcut_keys[i].name)) return shortcut_keys[i].key;
    return 0;
}
static bool shortcut_allowed(ShortcutChord chord)
{
    if (!chord.key) return chord.mods==0;
    bool supported=(chord.key>='A' && chord.key<='Z') || (chord.key>='0' && chord.key<='9');
    for (size_t i=0;i<sizeof(shortcut_keys)/sizeof(shortcut_keys[0]);i++) supported|=chord.key==shortcut_keys[i].key;
    if (!supported || (chord.mods & ~15u)) return false;
    if (!(chord.mods & (SHORTCUT_CTRL|SHORTCUT_ALT|SHORTCUT_SUPER)) && !(chord.key>=290 && chord.key<=300)) return false;
    if (chord.mods==SHORTCUT_CTRL && (chord.key=='C' || chord.key=='D' || chord.key=='Z')) return false;
    return true;
}
static bool shortcut_parse(const char *text, ShortcutChord *out)
{
    if (!strcasecmp(text,"none")) { *out=(ShortcutChord){0}; return true; }
    char buffer[96]; if (!*text || strlen(text)>=sizeof(buffer)) return false;
    strcpy(buffer,text); ShortcutChord chord={0}; char *rest=buffer,*part;
    while ((part=strsep(&rest,"+"))!=NULL) {
        unsigned mod=!strcasecmp(part,"Ctrl") || !strcasecmp(part,"Control") ? SHORTCUT_CTRL :
            !strcasecmp(part,"Shift") ? SHORTCUT_SHIFT : !strcasecmp(part,"Alt") || !strcasecmp(part,"Option") ? SHORTCUT_ALT :
            !strcasecmp(part,"Super") || !strcasecmp(part,"Command") || !strcasecmp(part,"Cmd") ? SHORTCUT_SUPER : 0;
        if (mod) { if (chord.key || (chord.mods & mod)) return false; chord.mods|=mod; }
        else { if (chord.key || rest) return false; chord.key=shortcut_key(part); if (!chord.key) return false; }
    }
    if (!shortcut_allowed(chord)) return false;
    *out=chord; return true;
}
static void shortcut_label(ShortcutChord chord, bool apple, char *out, size_t capacity)
{
    const char *key="none"; char letter[2]={0};
    if ((chord.key>='A' && chord.key<='Z') || (chord.key>='0' && chord.key<='9')) { letter[0]=(char)chord.key; key=letter; }
    for (size_t i=0;i<sizeof(shortcut_keys)/sizeof(shortcut_keys[0]);i++) if (chord.key==shortcut_keys[i].key) key=shortcut_keys[i].name;
    snprintf(out,capacity,"%s%s%s%s%s",chord.mods&SHORTCUT_CTRL ? "Ctrl+" : "",chord.mods&SHORTCUT_SUPER ? (apple ? "Command+" : "Super+") : "",
        chord.mods&SHORTCUT_ALT ? (apple ? "Option+" : "Alt+") : "",chord.mods&SHORTCUT_SHIFT ? "Shift+" : "",key);
}
static ShortcutState shortcut_defaults(bool apple)
{
    ShortcutState state={0};unsigned base=apple ? SHORTCUT_SUPER : SHORTCUT_CTRL|SHORTCUT_SHIFT;
    state.bindings[COMMAND_PALETTE]=(ShortcutChord){'P',base|SHORTCUT_SHIFT};
    state.bindings[COMMAND_OVERVIEW]=(ShortcutChord){'O',base|SHORTCUT_SHIFT};
    state.bindings[COMMAND_COPY]=(ShortcutChord){'C',base}; state.bindings[COMMAND_PASTE]=(ShortcutChord){'V',base};
    state.bindings[COMMAND_TERMINAL_CREATE]=(ShortcutChord){'T',base};
    state.bindings[COMMAND_WORKSPACE_CREATE]=(ShortcutChord){'N',base};
    unsigned navigation=apple ? SHORTCUT_SUPER|SHORTCUT_ALT : SHORTCUT_CTRL|SHORTCUT_SHIFT;
    state.bindings[COMMAND_TERMINAL_PREVIOUS]=(ShortcutChord){263,navigation};state.bindings[COMMAND_TERMINAL_NEXT]=(ShortcutChord){262,navigation};
    state.bindings[COMMAND_WORKSPACE_PREVIOUS]=(ShortcutChord){265,navigation};state.bindings[COMMAND_WORKSPACE_NEXT]=(ShortcutChord){264,navigation};
    return state;
}
static bool shortcut_valid(const ShortcutState *state, char *error, size_t capacity)
{
    for (int i=0;i<COMMAND_COUNT;i++) {
        if (!shortcut_allowed(state->bindings[i])) { snprintf(error,capacity,"Unsupported or reserved binding: %s",command_ids[i]); return false; }
        for (int j=0;j<i;j++) if (state->bindings[i].key && state->bindings[i].key==state->bindings[j].key && state->bindings[i].mods==state->bindings[j].mods) {
            snprintf(error,capacity,"Binding conflict: %s / %s",command_definitions[j].title,command_definitions[i].title);return false;
        }
    }
    return true;
}
static bool shortcut_read(const char *path, bool apple, ShortcutState *out, char *error, size_t capacity)
{
    FILE *file=fopen(path,"r");
    if (!file) { if (errno==ENOENT) { *out=shortcut_defaults(apple); return true; } snprintf(error,capacity,"Cannot read shortcuts: %s",strerror(errno)); return false; }
    ShortcutState next=shortcut_defaults(apple);bool seen[COMMAND_COUNT]={0};char line[256];bool valid=true;
    if (!fgets(line,sizeof(line),file) || strcmp(line,"workbench-shortcuts-v1\n")) valid=false;
    int lines=0;
    while (valid && fgets(line,sizeof(line),file)) {
        if (++lines>64 || !strchr(line,'\n')) { valid=false; break; }
        line[strcspn(line,"\r\n")]=0;
        if (!line[0] || line[0]=='#') continue;
        char *equal=strchr(line,'=');if (!equal) { valid=false;break; } *equal++=0;
        int action=-1;for (int i=0;i<COMMAND_COUNT;i++) if (!strcmp(line,command_ids[i])) action=i;
        if (action<0 || seen[action] || !shortcut_parse(equal,&next.bindings[action])) { valid=false;break; }seen[action]=true;
    }
    if (ferror(file)) valid=false;
    fclose(file);
    if (!valid) { snprintf(error,capacity,"Invalid shortcut file; existing bindings kept"); return false; }
    if (!shortcut_valid(&next,error,capacity)) return false;
    *out=next; return true;
}
static bool shortcut_write(const char *path, const ShortcutState *state, char *error, size_t capacity)
{
    if (!shortcut_valid(state,error,capacity)) return false;
    char temporary[1200];if (snprintf(temporary,sizeof(temporary),"%s.tmp-XXXXXX",path)>=(int)sizeof(temporary)) return false;
    int fd=mkstemp(temporary);if (fd<0) { snprintf(error,capacity,"Cannot save shortcuts: %s",strerror(errno));return false; }
    FILE *file=fdopen(fd,"w");if (!file) { close(fd);unlink(temporary);return false; }
    bool success=fputs("workbench-shortcuts-v1\n",file)!=EOF;
    for (int i=0;i<COMMAND_COUNT;i++) { char label[96];shortcut_label(state->bindings[i],false,label,sizeof(label));if (fprintf(file,"%s=%s\n",command_ids[i],label)<0) success=false; }
    if (fflush(file) || fsync(fd)) success=false;
    if (fclose(file)) success=false;
    if (success && rename(temporary,path)==0) return true;
    snprintf(error,capacity,"Cannot save shortcuts: %s",strerror(errno));unlink(temporary);return false;
}
#endif
