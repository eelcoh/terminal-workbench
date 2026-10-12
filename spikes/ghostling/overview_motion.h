/* Overview transitions only own presentation. Pending navigation commits at
 * completion; no resize, terminal input or focus handoff occurs in between. */
static void overview_motion_finish(WorkspaceClient *client)
{
    if (client->overview_jump_workspace >= 0)
        overview_apply_jump(client, client->overview_jump_workspace, client->overview_jump_pane);
    client->overview_open = false;
    client->overview_closing = false;
    client->overview_jump_workspace = client->overview_jump_pane = -1;
    overview_animation_target(&client->overview_animation, 0, GetTime(), 0);
}
static void overview_motion_tick(WorkspaceClient *client)
{
    if (IsWindowResized() && client->overview_open && !client->overview_list) {
        if (client->overview_closing) overview_motion_finish(client);
        else overview_animation_target(&client->overview_animation, 1, GetTime(), 0);
        return;
    }
    if (!client->overview_closing) return;
    workbench_input_peek();
    bool interrupted = false;
    /* A new terminal key completes the transition before normal input handling.
     * Leave queues intact. The exit gesture was consumed in its own modal frame. */
    if (!overview_requested() && !workbench_input_has_key(KEY_ESCAPE)) {
        interrupted = workbench_input_has_text();
        for (int key = 32; key <= KEY_KB_MENU; key++)
            if (!(key >= KEY_LEFT_SHIFT && key <= KEY_RIGHT_SUPER) && key != KEY_F12 && workbench_input_has_key(key)) { interrupted = true; break; }
    }
    if (interrupted || !overview_animation_busy(&client->overview_animation, GetTime()))
        overview_motion_finish(client);
}
static void overview_close(WorkspaceClient *client, int workspace_index, int pane_index)
{
    if (client->overview_list || !client->overview_animation_enabled) {
        if (workspace_index >= 0) overview_apply_jump(client, workspace_index, pane_index);
        else client->overview_open = false;
        overview_animation_target(&client->overview_animation, 0, GetTime(), 0);
        return;
    }
    int workspace = workspace_index >= 0 ? workspace_index : client->active;
    int pane = pane_index >= 0 ? pane_index : client->workspaces[workspace].focused;
    client->overview_exit_has_card = false;
    OverviewItem items[WORKBENCH_MAX_WORKSPACES * WORKBENCH_PANES];
    int count = overview_items(client, items);
    for (int i = 0; i < count; i++)
        if (items[i].workspace == workspace && items[i].pane == pane && overview_card_visible(client, items, i)) {
            client->overview_exit_card = overview_card(client, items, i);
            client->overview_exit_has_card = true;
        }
    client->overview_jump_workspace = workspace_index;
    client->overview_jump_pane = pane_index;
    client->overview_closing = true;
    overview_animation_target(&client->overview_animation, 0, GetTime(), 0.18);
}
static void overview_previews_refresh(WorkspaceClient *client)
{
    OverviewItem items[WORKBENCH_MAX_WORKSPACES * WORKBENCH_PANES];
    int count = overview_items(client, items);
    for (int i = 0; i < count; i++)
        if (overview_card_visible(client, items, i)) {
            Rectangle card = overview_card(client, items, i);
            if (card.height - 52 >= 8)
                pane_preview_refresh(client, client->workspaces[items[i].workspace].panes[items[i].pane]);
        }
}
static Rectangle overview_motion_rect(Rectangle from, Rectangle to, float value)
{
    return (Rectangle){from.x + (to.x - from.x) * value, from.y + (to.y - from.y) * value,
        from.width + (to.width - from.width) * value, from.height + (to.height - from.height) * value};
}
static void overview_present(WorkspaceClient *client)
{
    double now = GetTime();
    if (client->overview_list || !overview_animation_busy(&client->overview_animation, now)) {
        overview_draw(client); return;
    }
    int width = GetScreenWidth(), height = GetScreenHeight();
    if (!client->overview_stage.id || client->overview_stage.texture.width != width || client->overview_stage.texture.height != height) {
        if (client->overview_stage.id) UnloadRenderTexture(client->overview_stage);
        client->overview_stage = LoadRenderTexture(width, height);
        if (!client->overview_stage.id) {
            overview_animation_target(&client->overview_animation, client->overview_closing ? 0 : 1, now, 0);
            overview_draw(client); return;
        }
        SetTextureFilter(client->overview_stage.texture, TEXTURE_FILTER_BILINEAR);
    }
    /* Previews were refreshed before BeginDrawing; drawing this offscreen stage
     * never nests a preview framebuffer inside another framebuffer. */
    BeginTextureMode(client->overview_stage);
    ClearBackground(BLANK);
    overview_draw(client);
    EndTextureMode();
    float value = overview_animation_value(&client->overview_animation, now);
    DrawTextureRec(client->overview_stage.texture, (Rectangle){0, 0, width, -height}, (Vector2){0, 0}, Fade(WHITE, value));
    if (client->overview_closing && client->overview_exit_has_card) {
        int w = client->overview_jump_workspace >= 0 ? client->overview_jump_workspace : client->active;
        WorkbenchWorkspace *workspace = &client->workspaces[w];
        int p = client->overview_jump_pane >= 0 ? client->overview_jump_pane : workspace->focused;
        WorkbenchWorkspace destination=*workspace;
        workspace_focus(&destination,p);
        Rectangle normal=workspace_pane_bounds(&destination,p);
        Rectangle card = client->overview_exit_card;
        Rectangle source = {card.x, height - card.y - card.height, card.width, -card.height};
        DrawTexturePro(client->overview_stage.texture, source, overview_motion_rect(card, normal, 1 - value), (Vector2){0, 0}, 0, Fade(WHITE, 1 - value));
    } else if (!client->overview_closing) {
        OverviewItem items[WORKBENCH_MAX_WORKSPACES * WORKBENCH_PANES];
        int count = overview_items(client, items);
        WorkbenchWorkspace *workspace = &client->workspaces[client->active];
        for (int i = 0; i < count; i++) {
            int p = items[i].pane;
            if (items[i].workspace != client->active || !workspace_visible(workspace,p)
                || !overview_card_visible(client, items, i)) continue;
            WorkbenchPane *pane = workspace->panes[p];
            if (!pane->canvas.id) continue;
            Rectangle source = {0, 0, pane->canvas.texture.width, -pane->canvas.texture.height};
            DrawTexturePro(pane->canvas.texture, source, overview_motion_rect(pane->bounds, overview_card(client, items, i), value), (Vector2){0, 0}, 0, Fade(WHITE, 1 - value));
        }
    }
}
