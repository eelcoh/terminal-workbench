/* Scoped palette resolution and client preference persistence. */
static const char *theme_workspace_id(WorkspaceClient *client)
{
    return client->count ? client->workspaces[client->active].id : "";
}
static WorkbenchPane *theme_focused(WorkspaceClient *client)
{
    WorkbenchWorkspace *w=&client->workspaces[client->active];
    return w->count ? w->panes[w->focused] : NULL;
}
static bool theme_identity_live(WorkspaceClient *client, const char *id, bool terminal)
{
    for (int w=0;w<client->count;w++) {
        if (!terminal && !strcmp(client->workspaces[w].id,id)) return true;
        if (terminal) for (int p=0;p<client->workspaces[w].count;p++)
            if (!strcmp(client->workspaces[w].panes[p]->id,id)) return true;
    }
    return false;
}
static void theme_sync(WorkspaceClient *client)
{
    const char *active=theme_workspace_id(client);
    if (!client->appearance_dirty && !strcmp(client->appearance_active,active)) return;
    if (client->appearance_dirty) {
        AppearanceState *state=&client->appearance;
        for (int kind=0;kind<2;kind++) {
            AppearanceBinding *bindings=kind ? state->terminals : state->workspaces;
            int *count=kind ? &state->terminal_count : &state->workspace_count;
            for (int i=0;i<*count;) {
                if (theme_identity_live(client,bindings[i].id,kind!=0)) i++;
                else appearance_set(bindings,count,kind ? APPEARANCE_TERMINALS : APPEARANCE_WORKSPACES,bindings[i].id,NULL);
            }
        }
        bool changed=false;
        for (int w=0;w<client->count;w++) for (int p=0;p<client->workspaces[w].count;p++) {
            WorkbenchPane *pane=client->workspaces[w].panes[p];
            const Theme *theme=appearance_terminal(state,client->workspaces[w].id,pane->id);
            if (memcmp(&pane->theme,theme,sizeof(Theme))) {
                pane->theme=*theme; ui_apply_terminal(pane->terminal,&pane->theme); changed=true;
            }
        }
        if (changed) client->overview_epoch++;
        client->appearance_dirty=false;
    }
    ui_theme_set(*appearance_chrome(&client->appearance,active));
    strcpy(client->appearance_active,active);
}
static const Theme *theme_target(WorkspaceClient *client)
{
    AppearanceState *state=&client->appearance;
    if (client->theme_scope==1) return appearance_workspace(state,theme_workspace_id(client));
    if (client->theme_scope==2) { WorkbenchPane *pane=theme_focused(client); return pane ? appearance_terminal(state,theme_workspace_id(client),pane->id) : &state->defaults; }
    return client->theme_scope==3 ? &state->defaults : &state->application;
}
static void theme_apply(WorkspaceClient *client, Theme theme)
{
    AppearanceState *state=&client->appearance; bool valid=true;
    if (client->theme_scope==0) { state->application=theme; state->follow=false; }
    if (client->theme_scope==1) valid=appearance_set(state->workspaces,&state->workspace_count,APPEARANCE_WORKSPACES,theme_workspace_id(client),&theme);
    if (client->theme_scope==2) { WorkbenchPane *pane=theme_focused(client); valid=pane && appearance_set(state->terminals,&state->terminal_count,APPEARANCE_TERMINALS,pane->id,&theme); }
    if (client->theme_scope==3) state->defaults=theme;
    if (!valid) snprintf(client->theme_error,sizeof(client->theme_error),"Could not set appearance for this target");
    client->appearance_dirty=true; theme_sync(client);
}
static bool theme_load_path(const char *path, Theme *theme, int *ignored, char *error, size_t capacity)
{
    FILE *file=fopen(path,"r");
    if (!file) { snprintf(error,capacity,"Could not open theme: %s",strerror(errno)); return false; }
    Theme base=theme_preset(false);
    bool success=theme_read(file,&base,theme,ignored,error,capacity);
    fclose(file); return success;
}
static bool theme_initialize(WorkspaceClient *client)
{
    if (!theme_catalog_load(client->theme_catalog,client->theme_catalog_ignored,client->theme_error,sizeof(client->theme_error))) {
        fprintf(stderr,"Theme catalog: %s\n",client->theme_error); return false;
    }
    client->appearance=appearance_default(client->theme_catalog[0]);
    const char *state=getenv("WORKBENCH_STATE_DIR"); char legacy[1024]={0};
    if (state && (snprintf(client->theme_path,sizeof(client->theme_path),"%s/appearance.state",state)>=(int)sizeof(client->theme_path)
        || snprintf(legacy,sizeof(legacy),"%s/appearance.conf",state)>=(int)sizeof(legacy))) return false;
    if (client->theme_path[0] && access(client->theme_path,F_OK)==0) {
        FILE *file=fopen(client->theme_path,"r");
        if (!file) { fprintf(stderr,"Could not read appearance preferences\n"); return false; }
        bool valid=appearance_read(file,&client->appearance,client->theme_error,sizeof(client->theme_error)); fclose(file);
        if (!valid) { fprintf(stderr,"%s\n",client->theme_error); return false; }
    } else if (legacy[0] && access(legacy,F_OK)==0) {
        Theme theme;
        if (!theme_load_path(legacy,&theme,&client->theme_ignored,client->theme_error,sizeof(client->theme_error))) {
            fprintf(stderr,"Theme: %s\n",client->theme_error); return false;
        }
        /* Preserve the old global palette as both application and default colors. */
        client->appearance=appearance_default(theme);
    }
    const char *path=getenv("WORKBENCH_THEME");
    if (path) {
        if (!theme_load_path(path,&client->theme_imported,&client->theme_ignored,client->theme_error,sizeof(client->theme_error))) {
            fprintf(stderr,"Theme: %s\n",client->theme_error); return false;
        }
        client->theme_has_import=theme_find(client,&client->theme_imported)==THEME_CATALOG_COUNT;
        client->appearance.defaults=client->theme_imported;
    }
    client->appearance_dirty=true;
    ui_theme_set(*appearance_chrome(&client->appearance,theme_workspace_id(client))); return true;
}
static bool theme_save(WorkspaceClient *client)
{
    if (!client->theme_path[0]) return true; /* Diagnostic clients preview in memory. */
    char temporary[1100]; snprintf(temporary,sizeof(temporary),"%s.XXXXXX",client->theme_path);
    int fd=mkstemp(temporary);
    if (fd<0) { snprintf(client->theme_error,sizeof(client->theme_error),"Could not save appearance: %s",strerror(errno)); return false; }
    FILE *file=fdopen(fd,"w"); bool valid=file!=NULL;
    if (file) {
        valid=appearance_write(file,&client->appearance);
        if (fflush(file) || fsync(fd)) valid=false;
        if (fclose(file)) valid=false;
    } else close(fd);
    if (valid && rename(temporary,client->theme_path)) valid=false;
    if (!valid) { unlink(temporary); snprintf(client->theme_error,sizeof(client->theme_error),"Could not save appearance; retry or cancel"); }
    return valid;
}
