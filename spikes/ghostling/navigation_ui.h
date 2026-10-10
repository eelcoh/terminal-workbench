/* Bounded working-view motion: two live canvases plus a retained pixel snapshot.
 * Capture only on navigation. Steady frames do not allocate or composite a stage. */
static Rectangle navigation_area(void)
{
    return (Rectangle){WORKBENCH_SIDEBAR + WORKBENCH_GAP, 42,
        GetScreenWidth() - WORKBENCH_SIDEBAR - WORKBENCH_GAP * 2, GetScreenHeight() - 52};
}
static void navigation_finish(WorkspaceClient *client)
{
    overview_animation_target(&client->navigation_animation, 1, GetTime(), 0);
}
static bool navigation_moving(WorkspaceClient *client)
{
    return client->navigation_snapshot.id && overview_animation_busy(&client->navigation_animation, GetTime());
}
static void navigation_present(WorkspaceClient *client, WorkbenchWorkspace *workspace, int first)
{
    Rectangle area = navigation_area();
    bool moving = navigation_moving(client);
    float value = moving ? overview_animation_value(&client->navigation_animation, GetTime()) : 1;
    float incoming = client->navigation_distance * (1 - value);
    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    if (moving) {
        DrawTextureRec(client->navigation_snapshot.texture,
            (Rectangle){0, 0, area.width, -area.height},
            (Vector2){area.x - client->navigation_distance * value, area.y}, WHITE);
    }
    for (int p = first; p < workspace->count && p < first + 2; p++) {
        WorkbenchPane *pane = workspace->panes[p];
        if (!pane->canvas.id) continue;
        Rectangle bounds = pane->bounds; bounds.x += incoming;
        DrawTextureRec(pane->canvas.texture,
            (Rectangle){0, 0, pane->bounds.width, -pane->bounds.height},
            (Vector2){bounds.x, bounds.y}, WHITE);
        DrawRectangleLinesEx(bounds, 1, p == workspace->focused ? ui.accent : ui.border);
    }
    EndScissorMode();
}
static bool navigation_capture(WorkspaceClient *client, WorkbenchWorkspace *workspace)
{
    Rectangle area = navigation_area();
    RenderTexture2D *spare = &client->navigation_spare;
    if (spare->id && (spare->texture.width != (int)area.width || spare->texture.height != (int)area.height)) {
        UnloadRenderTexture(*spare); *spare = (RenderTexture2D){0};
    }
    if (!spare->id) *spare = LoadRenderTexture((int)area.width, (int)area.height);
    if (!spare->id) return false;
    BeginTextureMode(*spare);
    ClearBackground(ui.background);
    /* The stage uses window coordinates, including the same clipping rectangle. */
    Camera2D camera = {.offset={-area.x,-area.y}, .zoom=1};
    BeginMode2D(camera);
    /* Raylib scissor coordinates are framebuffer-local, so clip the full stage
     * separately rather than using navigation_present's window scissor. */
    bool moving = navigation_moving(client);
    float value = moving ? overview_animation_value(&client->navigation_animation, GetTime()) : 1;
    if (moving) DrawTextureRec(client->navigation_snapshot.texture,
        (Rectangle){0,0,area.width,-area.height},
        (Vector2){area.x-client->navigation_distance*value,area.y},WHITE);
    float incoming = client->navigation_distance*(1-value);
    for (int p=client->navigation_first;p<workspace->count && p<client->navigation_first+2;p++) {
        WorkbenchPane *pane=workspace->panes[p];
        if (!pane->canvas.id) continue;
        Rectangle bounds=pane->bounds; bounds.x+=incoming;
        DrawTextureRec(pane->canvas.texture,(Rectangle){0,0,bounds.width,-bounds.height},(Vector2){bounds.x,bounds.y},WHITE);
        DrawRectangleLinesEx(bounds,1,p==client->navigation_focused ? ui.accent : ui.border);
    }
    EndMode2D(); EndTextureMode();
    RenderTexture2D old = client->navigation_snapshot;
    client->navigation_snapshot = *spare; *spare = old;
    return true;
}
static void navigation_prepare(WorkspaceClient *client)
{
    WorkbenchWorkspace *workspace=&client->workspaces[client->active];
    Rectangle area=navigation_area();
    bool same=client->navigation_valid && !strcmp(client->navigation_workspace,workspace->id)
        && client->navigation_width==(int)area.width && client->navigation_height==(int)area.height;
    bool blocked=client->theme_open || client->dialog_open || client->overview_open || client->management_busy
        || client->divider_dragging || client->navigation_reset;
    if (!same || blocked || !client->overview_animation_enabled) navigation_finish(client);
    else if (workspace->first!=client->navigation_first) {
        if (navigation_capture(client,workspace)) {
            PaneNavigationMotion motion=pane_navigation_motion(client->navigation_first,workspace->first,
                workspace->panes[client->navigation_first]->bounds.width+WORKBENCH_GAP,area.width);
            client->navigation_distance=motion.distance;
            client->navigation_animation=(OverviewAnimation){.from=0,.target=1,.started=GetTime(),.duration=motion.duration};
        } else navigation_finish(client);
    }
    client->navigation_valid=true; client->navigation_reset=false;
    strcpy(client->navigation_workspace,workspace->id);
    client->navigation_first=workspace->first; client->navigation_focused=workspace->focused;
    client->navigation_width=(int)area.width; client->navigation_height=(int)area.height;
}
