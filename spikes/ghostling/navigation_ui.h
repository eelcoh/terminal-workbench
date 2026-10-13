/* Variable-width continuous strip. Cached intermediate screens never resize PTYs. */
static Rectangle navigation_area(void)
{ return (Rectangle){WORKBENCH_SIDEBAR+WORKBENCH_GAP,50,workspace_estate(),GetScreenHeight()-60}; }
static void navigation_finish(WorkspaceClient *client)
{ overview_animation_target(&client->navigation_animation,client->navigation_animation.target,GetTime(),0); }
static bool navigation_moving(WorkspaceClient *client)
{ return client->navigation_valid && overview_animation_busy(&client->navigation_animation,GetTime()); }
static void navigation_pan(WorkspaceClient *client,float delta)
{
    WorkbenchWorkspace *workspace=&client->workspaces[client->active];
    PaneWidth widths[WORKBENCH_PANES];workspace_widths(workspace,widths);
    float estate=workspace_estate(),maximum=fmaxf(0,pane_width_start(widths,workspace->count,estate,WORKBENCH_GAP)-WORKBENCH_GAP-estate);
    navigation_finish(client);
    workspace->scroll=pane_navigation_pan(workspace->scroll,delta,maximum);workspace->manual_scroll=true;
    workspace_sync_view(workspace);
    /* Keep focus until it leaves the viewport, then choose the closest visible pane. */
    if (workspace->count && !workspace_visible(workspace,workspace->focused)) {
        int best=workspace->first;float distance=INFINITY;
        for (int p=0;p<workspace->count;p++) if (workspace_visible(workspace,p)) {
            float next=fabsf((float)(p-workspace->focused));if (next<distance) {distance=next;best=p;}
        }
        workspace->focused=best;
    }
}
static void navigation_present(WorkspaceClient *client, WorkbenchWorkspace *workspace, int first)
{
    (void)first;
    Rectangle area=navigation_area();
    bool moving=navigation_moving(client);
    float position=moving ? overview_animation_value(&client->navigation_animation,GetTime()) : workspace->scroll;
    PaneWidth widths[WORKBENCH_PANES]; workspace_widths(workspace,widths);
    /* Finish framebuffer work before opening the screen scissor. */
    for (int p=0;p<workspace->count;p++) {
        if (!pane_width_visible(widths,p,position,area.width,WORKBENCH_GAP)) continue;
        WorkbenchPane *pane=workspace->panes[p];
        if (!moving) { if (pane->canvas.id) pane_render_canvas(client,pane,pane->canvas,pane->bounds);continue; }
        NavigationTile *tile=&client->navigation_tiles[p];
        int width=(int)pane_width_pixels(widths,p,area.width,WORKBENCH_GAP);
        int pixel_width=pixel_extent(width,client->raster_scale),pixel_height=pixel_extent((int)area.height,client->raster_scale);
        if (tile->canvas.id && (tile->canvas.texture.width!=pixel_width || tile->canvas.texture.height!=pixel_height)) {
            UnloadRenderTexture(tile->canvas);*tile=(NavigationTile){0};
        }
        if (!tile->canvas.id) tile->canvas=LoadRenderTexture(pixel_width,pixel_height);
        if (!tile->canvas.id) { navigation_finish(client);return; }
        if (tile->epoch!=client->navigation_epoch) {
            pane_render_canvas(client,pane,tile->canvas,(Rectangle){0,0,width,area.height});
            tile->epoch=client->navigation_epoch;
        }
    }
    BeginScissorMode((int)area.x,(int)area.y,(int)area.width,(int)area.height);
    for (int p=0;p<workspace->count;p++) {
        if (!pane_width_visible(widths,p,position,area.width,WORKBENCH_GAP)) continue;
        RenderTexture2D canvas=moving ? client->navigation_tiles[p].canvas : workspace->panes[p]->canvas;
        if (!canvas.id) continue;
        Rectangle bounds={pixel_snap(area.x+pane_width_start(widths,p,area.width,WORKBENCH_GAP)-position,client->raster_scale),area.y,
            (int)pane_width_pixels(widths,p,area.width,WORKBENCH_GAP),(int)area.height};
        DrawTexturePro(canvas.texture,(Rectangle){0,0,canvas.texture.width,-canvas.texture.height},bounds,(Vector2){0},0,WHITE);
        /* Mask only four corner pixels, then draw the matching 4px outline.
         * Bounded presentation work; terminal canvases and PTY geometry stay rectangular. */
        for (int y=0;y<4;y++) {
            float inset=4-sqrtf(16-(3.5f-y)*(3.5f-y));
            DrawRectangleRec((Rectangle){bounds.x,bounds.y+y,inset,1},ui.background);
            DrawRectangleRec((Rectangle){bounds.x+bounds.width-inset,bounds.y+y,inset,1},ui.background);
            DrawRectangleRec((Rectangle){bounds.x,bounds.y+bounds.height-y-1,inset,1},ui.background);
            DrawRectangleRec((Rectangle){bounds.x+bounds.width-inset,bounds.y+bounds.height-y-1,inset,1},ui.background);
        }
        /* Raylib's <=1 path is an outward GL line, which the viewport clips.
         * An inset filled ring keeps all four edges inside the pane at any DPI. */
        Rectangle outline={bounds.x+1,bounds.y+1,bounds.width-2,bounds.height-2};
        DrawRectangleRoundedLinesEx(outline,ui_roundness(outline,3),8,1.001f,p==workspace->focused ? ui.accent : ui.border);
    }
    EndScissorMode();
}
static void navigation_prepare(WorkspaceClient *client)
{
    WorkbenchWorkspace *workspace=&client->workspaces[client->active];
    workspace_sync_view(workspace);
    Rectangle area=navigation_area();
    bool same=client->navigation_valid && !strcmp(client->navigation_workspace,workspace->id)
        && client->navigation_width==(int)area.width && client->navigation_height==(int)area.height;
    bool blocked=client->help_open || client->text_open || client->theme_open || client->dialog_open || client->overview_open || client->management_busy
        || client->divider_dragging || client->navigation_reset || workspace->manual_scroll;
    if (!same || blocked || !client->overview_animation_enabled) {
        client->navigation_animation=(OverviewAnimation){.from=workspace->scroll,.target=workspace->scroll};
        client->navigation_epoch++;
    } else if (workspace->scroll!=client->navigation_scroll) {
        float from=overview_animation_value(&client->navigation_animation,GetTime());
        float stride=(area.width+WORKBENCH_GAP)/2;
        PaneNavigationMotion motion=pane_navigation_motion(from/stride,workspace->scroll/stride,stride);
        client->navigation_animation=(OverviewAnimation){.from=from,.target=workspace->scroll,.started=GetTime(),.duration=motion.duration};
        client->navigation_epoch++;
    }
    client->navigation_valid=true;client->navigation_reset=false;
    strcpy(client->navigation_workspace,workspace->id);
    client->navigation_scroll=workspace->scroll;client->navigation_first=workspace->first;client->navigation_focused=workspace->focused;
    client->navigation_width=(int)area.width;client->navigation_height=(int)area.height;
}
