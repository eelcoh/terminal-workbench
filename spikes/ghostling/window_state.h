/* Versioned client-only view state. Stable identities survive catalog reorder. */
#ifndef WORKBENCH_WINDOW_STATE_H
#define WORKBENCH_WINDOW_STATE_H
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#define WINDOW_STATE_WORKSPACES 8
typedef struct {
    char id[41], focused[41], first[41];
    float horizontal;
} WindowWorkspaceState;
typedef struct {
    int width, height, count;
    float divider, vertical;
    bool overview, list, agents, waiting, context;
    char active[41], sidebar[41], selected[41], query[64];
    WindowWorkspaceState workspaces[WINDOW_STATE_WORKSPACES];
} WindowState;
static bool window_state_id(const char *id)
{
    if (!*id || strlen(id)>40) return false;
    for (;*id;id++) if (!((*id>='a' && *id<='z') || (*id>='A' && *id<='Z')
        || (*id>='0' && *id<='9') || *id=='-' || *id=='_')) return false;
    return true;
}
static const char *window_state_token(const char *id) { return *id ? id : "."; }
static bool window_state_decode_id(char *id)
{
    if (!strcmp(id,".")) { *id=0; return true; }
    return window_state_id(id);
}
static int window_state_hex(char c)
{
    if (c>='0' && c<='9') return c-'0';
    if (c>='a' && c<='f') return c-'a'+10;
    return -1;
}
static bool window_state_read(WindowState *state, const char *path)
{
    FILE *file=fopen(path,"r");
    if (!file) return false;
    WindowState next={0}; char line[512], query[128]; int end=0, overview,list,agents,waiting,context;
    bool valid=false;
    if (!fgets(line,sizeof(line),file) || strcmp(line,"workbench-window-v1\n")) goto done;
    if (!fgets(line,sizeof(line),file) || sscanf(line,"V %d %d %f %40s %40s %d %d %d %d %f %40s %127s %d%n",
        &next.width,&next.height,&next.divider,next.active,next.sidebar,&overview,&list,&agents,&waiting,
        &next.vertical,next.selected,query,&context,&end)!=13 || strcmp(line+end,"\n")) goto done;
    if (next.width<800 || next.width>16384 || next.height<300 || next.height>16384
        || !isfinite(next.divider) || next.divider<.1f || next.divider>.9f
        || !isfinite(next.vertical) || next.vertical<0 || next.vertical>64
        || context<0 || context>1 || (overview && !context)
        || overview<0 || overview>1 || list<0 || list>1 || agents<0 || agents>1 || waiting<0 || waiting>1
        || !window_state_decode_id(next.active) || !window_state_decode_id(next.sidebar)
        || !window_state_decode_id(next.selected)) goto done;
    next.context=context; next.overview=overview; next.list=list; next.agents=agents; next.waiting=waiting;
    if (strcmp(query,"-")) {
        size_t length=strlen(query);
        if (length%2 || length>126) goto done;
        for (size_t i=0;i<length;i+=2) {
            int a=window_state_hex(query[i]), b=window_state_hex(query[i+1]);
            if (a<0 || b<0 || !(a*16+b)) goto done;
            next.query[i/2]=(char)(a*16+b);
        }
    }
    while (fgets(line,sizeof(line),file)) {
        if (next.count==WINDOW_STATE_WORKSPACES) goto done;
        WindowWorkspaceState *workspace=&next.workspaces[next.count];
        if (sscanf(line,"W %40s %40s %40s %f%n",workspace->id,workspace->focused,workspace->first,
            &workspace->horizontal,&end)!=4 || strcmp(line+end,"\n") || !window_state_id(workspace->id)
            || !window_state_decode_id(workspace->focused) || !window_state_decode_id(workspace->first)
            || !isfinite(workspace->horizontal) || workspace->horizontal<0 || workspace->horizontal>8) goto done;
        for (int i=0;i<next.count;i++) if (!strcmp(next.workspaces[i].id,workspace->id)) goto done;
        next.count++;
    }
    if (ferror(file)) goto done;
    *state=next; valid=true;
done:
    fclose(file); return valid;
}
static bool window_state_write(const WindowState *state, const char *path)
{
    char temporary[1200];
    if (snprintf(temporary,sizeof(temporary),"%s.tmp.XXXXXX",path)>=(int)sizeof(temporary)) return false;
    int fd=mkstemp(temporary);
    if (fd<0) return false;
    FILE *file=fdopen(fd,"w");
    if (!file) { close(fd); unlink(temporary); return false; }
    char query[128]={0};
    for (size_t i=0;i<strlen(state->query);i++) snprintf(query+i*2,3,"%02x",(unsigned char)state->query[i]);
    bool valid=fprintf(file,"workbench-window-v1\nV %d %d %.9g %s %s %d %d %d %d %.9g %s %s %d\n",
        state->width,state->height,state->divider,window_state_token(state->active),window_state_token(state->sidebar),
        state->overview,state->list,state->agents,state->waiting,state->vertical,window_state_token(state->selected),
        *query ? query : "-",state->context)>0;
    for (int i=0;i<state->count;i++) {
        const WindowWorkspaceState *workspace=&state->workspaces[i];
        if (fprintf(file,"W %s %s %s %.9g\n",workspace->id,window_state_token(workspace->focused),
            window_state_token(workspace->first),workspace->horizontal)<0) valid=false;
    }
    if (fflush(file) || fsync(fd)) valid=false;
    if (fclose(file)) valid=false;
    if (valid && rename(temporary,path)==0) return true;
    unlink(temporary); return false;
}
#endif
