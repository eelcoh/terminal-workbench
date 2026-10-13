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
/* Dialog targets are stable IDs; catalog refresh must never redirect an edit. */
static WorkbenchWorkspace *theme_dialog_workspace(WorkspaceClient *client)
{
    for (int w=0;w<client->count;w++) {
        WorkbenchWorkspace *workspace=&client->workspaces[w];
        if (client->theme_scope==1 && !strcmp(workspace->id,client->theme_workspace)) return workspace;
        if (client->theme_scope==2) for (int p=0;p<workspace->count;p++)
            if (!strcmp(workspace->panes[p]->id,client->theme_terminal)) return workspace;
    }
    return NULL;
}
static bool theme_follow_policy(WorkspaceClient *client)
{ return client->config.has_follow ? client->config.follow : true; }
static const Theme *theme_target(WorkspaceClient *client)
{
    WorkbenchWorkspace *workspace=theme_dialog_workspace(client);
    if (!workspace) return &client->appearance.defaults;
    return client->theme_scope==2 ? appearance_terminal(&client->appearance,workspace->id,client->theme_terminal)
        : appearance_workspace(&client->appearance,workspace->id);
}
static void theme_apply(WorkspaceClient *client,Theme theme)
{
    WorkbenchWorkspace *workspace=theme_dialog_workspace(client);
    if (!workspace) {strcpy(client->theme_error,"This target was closed. Cancel to return.");return;}
    AppearanceState *state=&client->appearance;
    bool valid=client->theme_scope==2 ? appearance_set(state->terminals,&state->terminal_count,APPEARANCE_TERMINALS,client->theme_terminal,&theme)
        : appearance_set(state->workspaces,&state->workspace_count,APPEARANCE_WORKSPACES,client->theme_workspace,&theme);
    if (client->theme_scope==1) state->follow=theme_follow_policy(client);
    if (!valid) snprintf(client->theme_error,sizeof(client->theme_error),"Could not set appearance for this target");
    client->appearance_dirty=true;theme_sync(client);
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
    if (getenv("WORKBENCH_CONFIG")) {
        /* Session files retain scoped identities; globals start from portable defaults. */
        client->appearance.application=client->theme_catalog[0];client->appearance.defaults=client->theme_catalog[0];client->appearance.follow=false;
        if (client->config.has_application) client->appearance.application=client->config.application;
        if (client->config.has_terminal) client->appearance.defaults=client->config.terminal;
        if (client->config.has_follow) client->appearance.follow=client->config.follow;
        const char *ui=getenv("WORKBENCH_UI_APPEARANCE");
        if (ui && access(ui,F_OK)==0) {
            AppearanceState saved;unsigned mask=0;FILE *file=fopen(ui,"r");
            bool valid=file && appearance_ui_read(file,&saved,&mask,client->theme_error,sizeof(client->theme_error));
            if (file) fclose(file);
            if (valid && !saved.workspace_count && !saved.terminal_count) {
                if (mask&1) client->appearance.application=saved.application;
                if (mask&2) client->appearance.defaults=saved.defaults;
                if (mask&4) client->appearance.follow=saved.follow;
            } else fprintf(stderr,"Invalid UI appearance overrides; dotfile defaults retained (%s)\n",ui);
        } else if (ui) {
            char legacy_ui[1200];snprintf(legacy_ui,sizeof(legacy_ui),"%.*sappearance.conf",strrchr(ui,'/') ? (int)(strrchr(ui,'/')-ui+1) : 0,ui);
            if (access(legacy_ui,F_OK)==0) {
                Theme palette;int ignored=0;
                if (theme_load_path(legacy_ui,&palette,&ignored,client->theme_error,sizeof(client->theme_error)))
                    client->appearance.application=client->appearance.defaults=palette;
                else fprintf(stderr,"Invalid migrated UI appearance; dotfile defaults retained\n");
            }
        }
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

/* Prepare all changed records before replacing either destination. */
static bool theme_prepare_save(WorkspaceClient *client,const char *path,const AppearanceState *state,unsigned mask,char *temporary,size_t capacity)
{
    if (!path || !*path) {temporary[0]=0;return true;}
    if (snprintf(temporary,capacity,"%s.XXXXXX",path)>=(int)capacity) {strcpy(client->theme_error,"Appearance path is too long");return false;}
    int fd=mkstemp(temporary);
    if (fd<0) {snprintf(client->theme_error,sizeof(client->theme_error),"Could not save appearance: %s",strerror(errno));return false;}
    FILE *file=fdopen(fd,"w");bool valid=file!=NULL;
    if (file) {
        valid=mask ? appearance_ui_write(file,state,mask) : appearance_write(file,state);
        if (fflush(file) || fsync(fd)) valid=false;
        if (fclose(file)) valid=false;
    } else close(fd);
    if (!valid) {unlink(temporary);strcpy(client->theme_error,"Could not prepare appearance; retry or cancel");}
    return valid;
}
static bool theme_save(WorkspaceClient *client)
{
    const char *ui=getenv("WORKBENCH_UI_APPEARANCE");
    unsigned changed=0,mask=0;
    if (memcmp(&client->appearance.application,&client->appearance_before.application,sizeof(Theme))) changed|=1;
    if (memcmp(&client->appearance.defaults,&client->appearance_before.defaults,sizeof(Theme))) changed|=2;
    if (client->appearance.follow!=client->appearance_before.follow) changed|=4;
    bool scoped=client->appearance.workspace_count!=client->appearance_before.workspace_count
        || client->appearance.terminal_count!=client->appearance_before.terminal_count
        || memcmp(client->appearance.workspaces,client->appearance_before.workspaces,sizeof(client->appearance.workspaces))
        || memcmp(client->appearance.terminals,client->appearance_before.terminals,sizeof(client->appearance.terminals));
    AppearanceState saved=appearance_default(client->appearance.application);
    saved.defaults=client->appearance.defaults;saved.follow=client->appearance.follow;
    bool existed=ui && access(ui,F_OK)==0;
    if (ui && changed && existed) {
        FILE *file=fopen(ui,"r");
        bool valid=file && appearance_ui_read(file,&saved,&mask,client->theme_error,sizeof(client->theme_error));
        if (file) fclose(file);
        if (!valid) return false;
    }
    AppearanceState previous=saved;unsigned previous_mask=mask;
    if (changed&1) saved.application=client->appearance.application;
    if (changed&2) saved.defaults=client->appearance.defaults;
    if (changed&4) saved.follow=client->appearance.follow;
    mask|=changed;
    char global[1100]={0},local[1100]={0},backup[1100]={0};
    bool global_save=ui && changed,local_save=(!ui || scoped) && client->theme_path[0];
    if (global_save && !theme_prepare_save(client,ui,&saved,mask,global,sizeof(global))) return false;
    if (local_save && !theme_prepare_save(client,client->theme_path,&client->appearance,0,local,sizeof(local))) {
        if (global[0]) unlink(global);
        return false;
    }
    if (global_save && local_save && existed && !theme_prepare_save(client,ui,&previous,previous_mask,backup,sizeof(backup))) {
        unlink(global);unlink(local);return false;
    }
    bool committed=false;
    if (global[0]) {
        if (rename(global,ui)) goto failed;
        committed=true;global[0]=0;
    }
    if (local[0] && rename(local,client->theme_path)) {
        /* A late local replacement failure restores the pre-Apply global record. */
        if (committed) {if (existed) {if (!rename(backup,ui)) backup[0]=0;} else unlink(ui);}
        goto failed;
    }
    if (backup[0]) unlink(backup);
    return true;
failed:
    if (global[0]) unlink(global);
    if (local[0]) unlink(local);
    if (backup[0]) unlink(backup);
    strcpy(client->theme_error,"Could not save appearance; retry or cancel");return false;
}
