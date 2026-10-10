/* Geometry, navigation and cards for the spatial overview. */
static int overview_group(WorkspaceClient *client, OverviewItem *items, int index)
{
    for (int group = 0; group < client->overview_group_count; group++)
        if (client->overview_groups[group] == items[index].workspace) return group;
    return -1;
}
static int overview_column(OverviewItem *items, int index)
{
    int column = 0;
    for (int i = index - 1; i >= 0 && items[i].workspace == items[index].workspace; i--) column++;
    return column;
}
static int overview_columns_visible(WorkspaceClient *client)
{
    int count = ((int)overview_bounds(client).width - 32 + 12) / 272;
    return count > 0 ? count : 1;
}
static int overview_groups_visible(WorkspaceClient *client)
{
    int count = ((int)overview_bounds(client).height - 144) / 230;
    return count > 0 ? count : 1;
}
static float overview_group_height(WorkspaceClient *client)
{
    return (overview_bounds(client).height - 144) / overview_groups_visible(client);
}
static Rectangle overview_card(WorkspaceClient *client, OverviewItem *items, int index)
{
    Rectangle bounds = overview_bounds(client);
    int columns = overview_columns_visible(client);
    float width = (bounds.width - 32 - (columns - 1) * 12) / columns;
    int column = overview_column(items, index) - client->overview_columns[items[index].workspace];
    return (Rectangle){bounds.x + 16 + column * (width + 12),
        bounds.y + 142 + (overview_group(client, items, index) - client->overview_first) * overview_group_height(client),
        width, overview_group_height(client) - 38};
}
static bool overview_card_visible(WorkspaceClient *client, OverviewItem *items, int index)
{
    int row = overview_group(client, items, index) - client->overview_first;
    int col = overview_column(items, index) - client->overview_columns[items[index].workspace];
    return row >= 0 && row < overview_groups_visible(client) && col >= 0 && col < overview_columns_visible(client);
}
static void overview_reveal(WorkspaceClient *client, OverviewItem *items, int count)
{
    if (!count) { client->overview_first = 0; return; }
    int index = client->overview_selected;
    if (index >= count) index = client->overview_selected = count - 1;
    int row = client->overview_list ? index : overview_group(client, items, index);
    int visible = client->overview_list ? overview_visible_rows(client) : overview_groups_visible(client);
    if (row < client->overview_first) client->overview_first = row;
    if (row >= client->overview_first + visible) client->overview_first = row - visible + 1;
    if (!client->overview_list) {
        int column = overview_column(items, index), *first = &client->overview_columns[items[index].workspace];
        int columns = overview_columns_visible(client);
        if (column < *first) *first = column;
        if (column >= *first + columns) *first = column - columns + 1;
    }
}
static void overview_spatial_move(WorkspaceClient *client, OverviewItem *items, int count, int horizontal, int vertical)
{
    if (!count) return;
    int index = client->overview_selected;
    if (horizontal) {
        int next = index + horizontal;
        if (next >= 0 && next < count && items[next].workspace == items[index].workspace) index = next;
    }
    if (vertical) {
        int target = overview_group(client, items, index) + vertical;
        int column = overview_column(items, index), best = -1;
        while (target >= 0 && target < client->overview_group_count && best < 0) {
            for (int i = 0; i < count; i++)
                if (overview_group(client, items, i) == target) {
                    best = i;
                    if (overview_column(items, i) >= column) break;
                }
            target += vertical > 0 ? 1 : -1;
        }
        if (best >= 0) index = best;
    }
    client->overview_selected = index;
}
static void overview_spatial_clamp(WorkspaceClient *client, OverviewItem *items, int count)
{
    int groups = client->overview_group_count;
    int limit = groups - overview_groups_visible(client);
    if (limit < 0) limit = 0;
    if (client->overview_first < 0) client->overview_first = 0;
    if (client->overview_first > limit) client->overview_first = limit;
    for (int w = 0; w < client->count; w++) {
        int matches = 0;
        for (int i = 0; i < count; i++) matches += items[i].workspace == w;
        int max = matches - overview_columns_visible(client);
        if (max < 0) max = 0;
        if (client->overview_columns[w] < 0) client->overview_columns[w] = 0;
        if (client->overview_columns[w] > max) client->overview_columns[w] = max;
    }
}
static Rectangle overview_pan_button(WorkspaceClient *client, int group, bool next)
{
    Rectangle bounds = overview_bounds(client);
    return (Rectangle){bounds.x + bounds.width - (next ? 40 : 72),
        bounds.y + 116 + (group - client->overview_first) * overview_group_height(client), 24, 22};
}
static void overview_spatial_draw(WorkspaceClient *client, OverviewItem *items, int count)
{
    Rectangle bounds = overview_bounds(client);
    for (int group = client->overview_first; group < client->overview_group_count && group < client->overview_first + overview_groups_visible(client); group++) {
        int w = client->overview_groups[group];
        WorkbenchWorkspace *workspace = &client->workspaces[w];
        float y = bounds.y + 116 + (group - client->overview_first) * overview_group_height(client);
        BeginScissorMode((int)bounds.x + 16, (int)y, (int)bounds.width - 100, 24);
        DrawTextEx(client->font, workspace->name, (Vector2){bounds.x + 16, y + 4}, 14, 0, ui.text);
        EndScissorMode();
        int matches = 0;
        for (int i = 0; i < count; i++) matches += items[i].workspace == w;
        if (matches > overview_columns_visible(client)) {
            draw_header_button(client->font, overview_pan_button(client, group, false), "<", client->overview_columns[w] > 0);
            draw_header_button(client->font, overview_pan_button(client, group, true), ">", client->overview_columns[w] + overview_columns_visible(client) < matches);
        }
        if (!matches) {
            Rectangle empty = {bounds.x + 16, y + 26, bounds.width - 32, overview_group_height(client) - 38};
            DrawRectangleRec(empty, ui.background);
            DrawRectangleLinesEx(empty, 1, ui.border);
            DrawTextEx(client->font, workspace->count ? "Drop into this workspace" : "Empty workspace / drop a terminal here",
                (Vector2){empty.x + 16, empty.y + 16}, 13, 0, ui.muted);
        }
    }
    for (int i = 0; i < count; i++) {
        if (!overview_card_visible(client, items, i)) continue;
        WorkbenchPane *pane = client->workspaces[items[i].workspace].panes[items[i].pane];
        Rectangle card = overview_card(client, items, i);
        bool selected = i == client->overview_selected;
        bool hovered = CheckCollisionPointRec(GetMousePosition(), card);
        ui_panel(card,ui.background,false);
        /* Texture rendering must happen outside the card's screen scissor. */
        pane_preview_draw(client, pane, (Rectangle){card.x + 4, card.y + 26, card.width - 8, card.height - 52});
        DrawRectangleRec((Rectangle){card.x, card.y, card.width, 24}, ui.raised);
        BeginScissorMode((int)card.x + 8, (int)card.y, (int)card.width - 24, 24);
        DrawTextEx(client->font, pane->name, (Vector2){card.x + 8, card.y + 5}, 13, 0, ui.text);
        EndScissorMode();
        if (pane->unread) DrawCircle((int)(card.x + card.width - 12), (int)(card.y + 12), 3, ui.waiting);
        const char *agent = pane->closed ? "" : remote_agent_name(pane->transport.agent);
        const char *status = pane->closed ? "Disconnected" : pane->transport.replaying ? "Connecting" : agent[0] ? remote_attention_name(pane->transport.attention) : "Connected";
        char detail[100];
        snprintf(detail, sizeof(detail), "%s%s%s", agent, agent[0] ? " / " : "", status);
        BeginScissorMode((int)card.x + 8, (int)(card.y + card.height - 23), (int)card.width - 16, 23);
        DrawTextEx(client->font, detail, (Vector2){card.x + 8, card.y + card.height - 18}, 12, 0,
            agent[0] ? remote_attention_color(pane->transport.attention) : ui.muted);
        EndScissorMode();
        DrawRectangleRoundedLinesEx(card,.06f,8,selected ? 2 : 1,selected ? ui.accent : hovered ? ui.muted : ui.border);
    }
}
