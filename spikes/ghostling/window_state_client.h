/* Window presentation only; never claims or restarts a terminal. */
static void window_view_restore_layout(WorkspaceClient *client)
{
    const WindowState *view=&client->saved_view;
    for (int w=0;w<client->count;w++) {
        WorkbenchWorkspace *workspace=&client->workspaces[w];
        if (!strcmp(workspace->id,view->active)) client->active=w;
        if (!strcmp(workspace->id,view->sidebar)) client->sidebar_first=w;
        for (int p=0;p<workspace->count;p++)
            for (int i=0;i<view->count;i++) for (int t=0;t<view->workspaces[i].terminals;t++) {
                const WindowTerminalState *width=&view->workspaces[i].widths[t];
                if (!strcmp(width->id,workspace->panes[p]->id))
                    workspace->panes[p]->width=(PaneWidth){.preset=width->preset,.full=width->full};
            }
        for (int i=0;i<view->count;i++) if (!strcmp(workspace->id,view->workspaces[i].id)) {
            const WindowWorkspaceState *saved=&view->workspaces[i];
            for (int p=0;p<workspace->count;p++) {
                if (!strcmp(workspace->panes[p]->id,saved->focused)) workspace->focused=p;
                if (!strcmp(workspace->panes[p]->id,saved->first)) workspace->first=p;
            }
            workspace->scroll=saved->terminals ? saved->scroll*(workspace_estate()+WORKBENCH_GAP) : workspace_offset(workspace,workspace->first);
            workspace->estate=workspace_estate();
            workspace->manual_scroll=saved->terminals>0;workspace_sync_view(workspace);
            client->overview_columns[w]=(int)floorf(saved->horizontal+.5f);
            client->overview_horizontal[w]=(OverviewScroll){.position=saved->horizontal,.target=saved->horizontal,
                .command=client->overview_columns[w]};
        }
    }
}
static void window_view_restore_overview(WorkspaceClient *client)
{
    const WindowState *view=&client->saved_view;
    client->view_restore_overview=false;
    client->overview_list=view->list;
    client->overview_agents=view->agents;
    client->overview_waiting=view->waiting;
    strcpy(client->overview_query,view->query);
    client->overview_first=(int)floorf(view->vertical+.5f);
    client->overview_scroll=(OverviewScroll){.position=view->vertical,.target=view->vertical,.command=client->overview_first};
    for (int w=0;w<client->count;w++) for (int i=0;i<view->count;i++)
        if (!strcmp(client->workspaces[w].id,view->workspaces[i].id)) {
            float horizontal=view->workspaces[i].horizontal;
            client->overview_columns[w]=(int)floorf(horizontal+.5f);
            client->overview_horizontal[w]=(OverviewScroll){.position=horizontal,.target=horizontal,.command=client->overview_columns[w]};
        }
    OverviewItem items[WORKBENCH_MAX_WORKSPACES*WORKBENCH_PANES];
    int count=overview_items(client,items);
    client->overview_selected=0;
    for (int i=0;i<count;i++)
        if (!strcmp(client->workspaces[items[i].workspace].panes[items[i].pane]->id,view->selected)) client->overview_selected=i;
    overview_scroll_update(client,items,count,true);
}
static void window_view_save(WorkspaceClient *client, bool force)
{
    if (!client->window_state_path) return;
    double now=GetTime();
    if (!force && now-client->view_checked<1) return;
    client->view_checked=now;
    WindowState view={0};
    view.width=GetScreenWidth(); view.height=GetScreenHeight();
    if (view.width<800 || view.height<300) return; /* Ignore minimized dimensions. */
    view.divider=.5f; view.count=client->count;
    view.chrome_pinned=client->chrome_pinned;view.chrome_agents=client->chrome_agents;view.chrome_waiting=client->chrome_waiting;
    strcpy(view.active,client->workspaces[client->active].id);
    if (client->sidebar_first>=0 && client->sidebar_first<client->count)
        strcpy(view.sidebar,client->workspaces[client->sidebar_first].id);
    view.context=client->view_has_overview;
    view.overview=client->overview_open && !client->overview_closing;
    view.list=client->overview_list; view.agents=client->overview_agents; view.waiting=client->overview_waiting;
    view.vertical=client->overview_scroll.target; strcpy(view.query,client->overview_query);
    if (client->view_restore_overview) {
        /* A working-view launch retains its not-yet-opened overview state. */
        view.list=client->saved_view.list; view.agents=client->saved_view.agents; view.waiting=client->saved_view.waiting;
        view.vertical=client->saved_view.vertical; strcpy(view.query,client->saved_view.query);
        strcpy(view.selected,client->saved_view.selected);
    } else {
        OverviewItem items[WORKBENCH_MAX_WORKSPACES*WORKBENCH_PANES];
        int count=overview_items(client,items);
        if (client->overview_selected>=0 && client->overview_selected<count)
            strcpy(view.selected,client->workspaces[items[client->overview_selected].workspace].panes[items[client->overview_selected].pane]->id);
    }
    for (int w=0;w<client->count;w++) {
        WorkbenchWorkspace *workspace=&client->workspaces[w];
        WindowWorkspaceState *saved=&view.workspaces[w]; strcpy(saved->id,workspace->id);
        if (workspace->count) {
            strcpy(saved->focused,workspace->panes[workspace->focused]->id);
            strcpy(saved->first,workspace->panes[workspace->first]->id);
        }
        saved->horizontal=client->overview_horizontal[w].target;
        saved->scroll=workspace->scroll/((workspace->estate>0 ? workspace->estate : workspace_estate())+WORKBENCH_GAP);
        saved->terminals=workspace->count;
        for (int p=0;p<workspace->count;p++) {
            WindowTerminalState *width=&saved->widths[p];strcpy(width->id,workspace->panes[p]->id);
            width->preset=workspace->panes[p]->width.preset;width->full=workspace->panes[p]->width.full;
        }
    }
    if (client->view_loaded && !memcmp(&view,&client->saved_view,sizeof(view))) return;
    if (window_state_write(&view,client->window_state_path)) { client->saved_view=view; client->view_loaded=true; }
    else fprintf(stderr,"Could not save window view: %s\n",client->window_state_path);
}
