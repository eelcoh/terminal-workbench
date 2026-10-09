/* A drag changes catalog ownership only after the management helper succeeds. */
static bool overview_drag_source(WorkspaceClient *client, OverviewItem *source)
{
    for (int w = 0; w < client->count; w++)
        for (int p = 0; p < client->workspaces[w].count; p++)
            if (!strcmp(client->workspaces[w].panes[p]->id, client->overview_drag_id)) {
                *source = (OverviewItem){w, p}; return true;
            }
    return false;
}
static void overview_drag_notice(WorkspaceClient *client, const char *message)
{
    snprintf(client->overview_notice, sizeof(client->overview_notice), "%s", message);
    client->overview_notice_until = GetTime() + 4;
}
static Rectangle overview_drag_strip(WorkspaceClient *client, int group)
{
    Rectangle bounds = overview_bounds(client);
    return (Rectangle){bounds.x + 16, bounds.y + 116 + (group - client->overview_first) * overview_group_height(client),
        bounds.width - 32, overview_group_height(client) - 12};
}
static int overview_drag_target(WorkspaceClient *client, Vector2 pointer)
{
    for (int g = client->overview_first; g < client->overview_group_count && g < client->overview_first + overview_groups_visible(client); g++)
        if (CheckCollisionPointRec(pointer, overview_drag_strip(client, g))) return g;
    return -1;
}
static int overview_drag_position(WorkspaceClient *client, OverviewItem *items, int count, int workspace, float x, float *marker)
{
    Rectangle bounds = overview_bounds(client);
    int position = client->workspaces[workspace].count;
    *marker = bounds.x + 24;
    for (int i = 0; i < count; i++) {
        if (items[i].workspace != workspace || !overview_card_visible(client, items, i)) continue;
        Rectangle card = overview_card(client, items, i);
        if (x < card.x + card.width / 2) { *marker = card.x - 6; return items[i].pane; }
        position = items[i].pane + 1;
        *marker = card.x + card.width + 6;
    }
    return position;
}
static void overview_drag_arm(WorkspaceClient *client, OverviewItem source)
{
    client->overview_drag_armed = true;
    client->overview_dragging = false;
    client->overview_drag_start = GetMousePosition();
    client->overview_drag_scroll = GetTime() + .4;
    snprintf(client->overview_drag_id, sizeof(client->overview_drag_id), "%s", client->workspaces[source.workspace].panes[source.pane]->id);
}
static void overview_move_saved(WorkspaceClient *client)
{
    OverviewItem items[WORKBENCH_MAX_WORKSPACES * WORKBENCH_PANES];
    int count = overview_items(client, items), found = -1;
    for (int attempt = 0; attempt < 2 && found < 0; attempt++) {
        for (int i = 0; i < count; i++)
            if (!strcmp(client->workspaces[items[i].workspace].panes[items[i].pane]->id, client->overview_move_id)) found = i;
        if (found < 0 && client->overview_query[0]) {
            client->overview_query[0] = '\0'; count = overview_items(client, items);
        }
    }
    if (found >= 0) client->overview_selected = found;
    else if (client->overview_selected >= count) client->overview_selected = count ? count - 1 : 0;
    overview_reveal(client, items, count);
    overview_spatial_clamp(client, items, count);
    overview_drag_notice(client, "Terminal moved / session kept running");
}
static bool overview_drag_input(WorkspaceClient *client)
{
    bool escape = false;
    int key;
    while ((key = GetKeyPressed()) != 0) escape |= key == KEY_ESCAPE;
    while (GetCharPressed() != 0) { }
    OverviewItem source;
    if (escape || !IsWindowFocused() || !overview_drag_source(client, &source)) {
        client->overview_drag_armed = client->overview_dragging = false;
        return false;
    }
    Vector2 pointer = GetMousePosition();
    float dx = pointer.x - client->overview_drag_start.x, dy = pointer.y - client->overview_drag_start.y;
    if (!client->overview_dragging && dx * dx + dy * dy >= 64) {
        if (!client->managed) {
            overview_drag_notice(client, "Moving terminals requires the workspace launcher");
            client->overview_drag_armed = false; return false;
        }
        client->overview_dragging = true;
        overview_animation_target(&client->overview_animation, 1, GetTime(), 0);
    }
    OverviewItem items[WORKBENCH_MAX_WORKSPACES * WORKBENCH_PANES];
    int count = overview_items(client, items);
    if (client->overview_dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Rectangle bounds = overview_bounds(client);
        float wheel = GetMouseWheelMove();
        int group = overview_drag_target(client, pointer);
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (wheel && CheckCollisionPointRec(pointer, bounds)) {
            if (shift && group >= 0) client->overview_columns[client->overview_groups[group]] += wheel > 0 ? -1 : 1;
            else client->overview_first += wheel > 0 ? -1 : 1;
        }
        if (GetTime() >= client->overview_drag_scroll && pointer.x >= bounds.x && pointer.x <= bounds.x + bounds.width) {
            float top = bounds.y + 116, bottom = bounds.y + bounds.height - 28;
            if (pointer.y >= top - 18 && pointer.y < top + 20) client->overview_first--;
            else if (pointer.y > bottom - 20 && pointer.y <= bottom + 18) client->overview_first++;
            else if (group >= 0) {
                if (pointer.x < bounds.x + 40) client->overview_columns[client->overview_groups[group]]--;
                else if (pointer.x > bounds.x + bounds.width - 40) client->overview_columns[client->overview_groups[group]]++;
            }
            client->overview_drag_scroll = GetTime() + .35;
        }
        overview_spatial_clamp(client, items, count);
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) return false;
    bool dragging = client->overview_dragging;
    client->overview_drag_armed = client->overview_dragging = false;
    if (!dragging) {
        for (int i = 0; i < count; i++)
            if (items[i].workspace == source.workspace && items[i].pane == source.pane && overview_card_visible(client, items, i)
                && CheckCollisionPointRec(pointer, overview_card(client, items, i))) {
                overview_jump(client, source); return true;
            }
        return false;
    }
    int group = overview_drag_target(client, pointer);
    if (group < 0) { overview_drag_notice(client, "Move cancelled"); return false; }
    int target = client->overview_groups[group];
    if (target != source.workspace && client->workspaces[target].count >= WORKBENCH_PANES) {
        overview_drag_notice(client, "Workspace is full / maximum 8 terminals"); return false;
    }
    float marker;
    int position = overview_drag_position(client, items, count, target, pointer.x, &marker);
    if (target == source.workspace && source.pane < position) position--;
    if (target == source.workspace && source.pane == position) return false;
    management_move_start(client, client->overview_drag_id, client->workspaces[target].id, position);
    return false;
}
static void overview_drag_draw(WorkspaceClient *client, OverviewItem *items, int count)
{
    OverviewItem source;
    if (!client->overview_dragging || !overview_drag_source(client, &source)) return;
    Vector2 pointer = GetMousePosition();
    int group = overview_drag_target(client, pointer);
    if (group >= 0) {
        int target = client->overview_groups[group];
        bool full = target != source.workspace && client->workspaces[target].count >= WORKBENCH_PANES;
        Color color = full ? (Color){218, 108, 108, 255} : (Color){135, 181, 223, 255};
        Rectangle strip = overview_drag_strip(client, group);
        DrawRectangleLinesEx(strip, 2, color);
        if (!full) {
            float marker;
            overview_drag_position(client, items, count, target, pointer.x, &marker);
            DrawRectangle((int)marker - 1, (int)strip.y + 26, 3, (int)strip.height - 26, color);
        }
        DrawTextEx(client->font, full ? "Workspace full" : "Release to move / Esc to cancel",
            (Vector2){strip.x + 240, strip.y + 4}, 13, 0, color);
    }
    Rectangle ghost = {pointer.x + 14, pointer.y + 14, 210, 142};
    if (ghost.x + ghost.width > GetScreenWidth()) ghost.x = pointer.x - ghost.width - 14;
    if (ghost.y + ghost.height > GetScreenHeight()) ghost.y = pointer.y - ghost.height - 14;
    WorkbenchPane *pane = client->workspaces[source.workspace].panes[source.pane];
    DrawRectangleRec(ghost, (Color){24, 28, 34, 245});
    pane_preview_draw(client, pane, (Rectangle){ghost.x + 4, ghost.y + 26, ghost.width - 8, ghost.height - 30});
    BeginScissorMode((int)ghost.x + 8, (int)ghost.y, (int)ghost.width - 16, 24);
    DrawTextEx(client->font, pane->name, (Vector2){ghost.x + 8, ghost.y + 5}, 13, 0, WHITE);
    EndScissorMode();
    DrawRectangleLinesEx(ghost, 2, (Color){135, 181, 223, 255});
}
