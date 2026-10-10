/* A continuous pane strip. Only intersecting tiles render, once per navigation;
 * intermediate panes retain their runtime geometry and never receive input. */
static Rectangle navigation_area(void)
{
    return (Rectangle){WORKBENCH_SIDEBAR+WORKBENCH_GAP,42,
        GetScreenWidth()-WORKBENCH_SIDEBAR-WORKBENCH_GAP*2,GetScreenHeight()-52};
}
static void navigation_finish(WorkspaceClient *client)
{
    overview_animation_target(&client->navigation_animation,client->navigation_animation.target,GetTime(),0);
}
static bool navigation_moving(WorkspaceClient *client)
{
    return client->navigation_valid && overview_animation_busy(&client->navigation_animation,GetTime());
}
static float navigation_stride(WorkspaceClient *client, WorkbenchWorkspace *workspace)
{
    return workspace_pane_bounds(client,workspace->count,workspace->first,workspace->first).width+WORKBENCH_GAP;
}
static bool navigation_refresh(WorkspaceClient *client, WorkbenchWorkspace *workspace, int first, int last)
{
    Rectangle area=navigation_area();
    float stride=navigation_stride(client,workspace);
    for (int p=first;p<last;p++) {
        NavigationTile *tile=&client->navigation_tiles[p];
        /* Cache enough pixels for the right destination's changing visible width.
         * Its content/controls use its normal slot geometry; only the clip grows.
         * Intermediate terminal screens keep their retained PTY geometry. */
        float render_width=p==workspace->first+1 ? workspace->panes[p]->bounds.width : stride-WORKBENCH_GAP;
        float width=render_width>stride-WORKBENCH_GAP ? render_width : stride-WORKBENCH_GAP;
        if (tile->canvas.id && (tile->canvas.texture.width!=(int)width || tile->canvas.texture.height!=(int)area.height)) {
            UnloadRenderTexture(tile->canvas); *tile=(NavigationTile){0};
        }
        if (!tile->canvas.id) tile->canvas=LoadRenderTexture((int)width,(int)area.height);
        if (!tile->canvas.id) { navigation_finish(client); return false; }
        if (tile->epoch!=client->navigation_epoch) {
            pane_render_canvas(client,workspace->panes[p],tile->canvas,(Rectangle){0,0,render_width,area.height});
            tile->epoch=client->navigation_epoch;
        }
    }
    return true;
}
static void navigation_present(WorkspaceClient *client, WorkbenchWorkspace *workspace, int first)
{
    Rectangle area=navigation_area();
    bool moving=navigation_moving(client);
    float position=overview_animation_value(&client->navigation_animation,GetTime());
    float stride=navigation_stride(client,workspace);
    int last=first+2;
    if (moving) {
        pane_navigation_visible(position,workspace->count,stride,area.width,&first,&last);
        moving=navigation_refresh(client,workspace,first,last);
    }
    if (!moving) {
        first=workspace->first; last=first+2;
        for (int p=first;p<last && p<workspace->count;p++)
            pane_render_canvas(client,workspace->panes[p],workspace->panes[p]->canvas,workspace->panes[p]->bounds);
    }
    BeginScissorMode((int)area.x,(int)area.y,(int)area.width,(int)area.height);
    if (last>workspace->count) last=workspace->count;
    /* Lower-index tiles paint last so an expanding right destination covers
     * subsequent strip tiles instead of showing them through its final slot. */
    for (int p=last-1;p>=first;p--) {
        RenderTexture2D canvas=moving ? client->navigation_tiles[p].canvas : workspace->panes[p]->canvas;
        if (!canvas.id) continue;
        Rectangle bounds=moving ? (Rectangle){area.x+(p-position)*stride,area.y,canvas.texture.width,canvas.texture.height} : workspace->panes[p]->bounds;
        if (moving && p==workspace->first+1)
            bounds.width=pane_navigation_right_width(position,workspace->first,stride-WORKBENCH_GAP,workspace->panes[p]->bounds.width);
        DrawTextureRec(canvas.texture,(Rectangle){0,0,bounds.width,-canvas.texture.height},(Vector2){bounds.x,bounds.y},WHITE);
        DrawRectangleLinesEx(bounds,1,p==workspace->focused ? ui.accent : ui.border);
    }
    EndScissorMode();
}
static void navigation_prepare(WorkspaceClient *client)
{
    WorkbenchWorkspace *workspace=&client->workspaces[client->active];
    Rectangle area=navigation_area();
    bool same=client->navigation_valid && !strcmp(client->navigation_workspace,workspace->id)
        && client->navigation_width==(int)area.width && client->navigation_height==(int)area.height;
    bool blocked=client->theme_open || client->dialog_open || client->overview_open || client->management_busy
        || client->divider_dragging || client->navigation_reset;
    if (!same || blocked || !client->overview_animation_enabled) {
        client->navigation_animation=(OverviewAnimation){.from=workspace->first,.target=workspace->first};
        client->navigation_epoch++;
    } else if (workspace->first!=client->navigation_first) {
        float from=overview_animation_value(&client->navigation_animation,GetTime());
        PaneNavigationMotion motion=pane_navigation_motion(from,workspace->first,navigation_stride(client,workspace));
        client->navigation_animation=(OverviewAnimation){.from=from,.target=workspace->first,.started=GetTime(),.duration=motion.duration};
        client->navigation_epoch++;
    }
    client->navigation_valid=true; client->navigation_reset=false;
    strcpy(client->navigation_workspace,workspace->id);
    client->navigation_first=workspace->first; client->navigation_focused=workspace->focused;
    client->navigation_width=(int)area.width; client->navigation_height=(int)area.height;
}
