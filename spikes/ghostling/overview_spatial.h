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
    float column = overview_column(items, index) - client->overview_horizontal[items[index].workspace].position;
    return (Rectangle){bounds.x + 16 + column * (width + 12),
        bounds.y + 142 + (overview_group(client, items, index) - client->overview_scroll.position) * overview_group_height(client),
        width, overview_group_height(client) - 38};
}
static bool overview_card_visible(WorkspaceClient *client, OverviewItem *items, int index)
{
    return CheckCollisionRecs(overview_card(client,items,index),overview_content(client));
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
    client->overview_scroll.target = (float)client->overview_first;
    client->overview_scroll.command = client->overview_first;
    if (!client->overview_list) {
        int column = overview_column(items, index), *first = &client->overview_columns[items[index].workspace];
        int columns = overview_columns_visible(client);
        if (column < *first) *first = column;
        if (column >= *first + columns) *first = column - columns + 1;
        client->overview_horizontal[items[index].workspace].target = (float)*first;
        client->overview_horizontal[items[index].workspace].command = *first;
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
static void overview_scroll_update(WorkspaceClient *client, OverviewItem *items, int count, bool immediate)
{
    float maximum = client->overview_list ? count-overview_visible_rows(client) : client->overview_group_count-overview_groups_visible(client);
    double now = GetTime();
    overview_scroll_sync(&client->overview_scroll,&client->overview_first,maximum,now,immediate || !client->overview_animation_enabled);
    int matches[WORKBENCH_MAX_WORKSPACES] = {0};
    for (int i=0;i<count;i++) matches[items[i].workspace]++;
    for (int w=0;w<client->count;w++)
        overview_scroll_sync(&client->overview_horizontal[w],&client->overview_columns[w],matches[w]-overview_columns_visible(client),now,immediate || !client->overview_animation_enabled);
}
static void overview_scroll_input(WorkspaceClient *client, OverviewItem *items, int count)
{
    Vector2 pointer=GetMousePosition(), wheel=GetMouseWheelMoveV();
    if (!CheckCollisionPointRec(pointer,overview_content(client))) return;
    bool shift=IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool horizontal=!client->overview_list && (shift || fabsf(wheel.x)>fabsf(wheel.y));
    if (horizontal) {
        for (int g=0;g<client->overview_group_count;g++) {
            float y=overview_bounds(client).y+116+(g-client->overview_scroll.position)*overview_group_height(client);
            if (pointer.y < y || pointer.y >= y+overview_group_height(client)) continue;
            int w=client->overview_groups[g], matches=0;
            for (int i=0;i<count;i++) matches += items[i].workspace==w;
            float stride=(overview_bounds(client).width-32+12)/overview_columns_visible(client);
            overview_scroll_delta(&client->overview_horizontal[w],&client->overview_columns[w],-(wheel.x!=0 ? wheel.x : wheel.y),stride,matches-overview_columns_visible(client));
            break;
        }
    } else {
        float stride=client->overview_list ? 42 : overview_group_height(client);
        float maximum=client->overview_list ? count-overview_visible_rows(client) : client->overview_group_count-overview_groups_visible(client);
        overview_scroll_delta(&client->overview_scroll,&client->overview_first,-wheel.y,stride,maximum);
    }
}
static Rectangle overview_pan_button(WorkspaceClient *client, int group, bool next)
{
    Rectangle bounds = overview_bounds(client);
    return (Rectangle){bounds.x + bounds.width - (next ? 40 : 72),
        bounds.y + 116 + (group - client->overview_scroll.position) * overview_group_height(client), 24, 22};
}
static void overview_spatial_draw(WorkspaceClient *client, OverviewItem *items, int count)
{
    Rectangle bounds = overview_bounds(client);
    for (int group = 0; group < client->overview_group_count; group++) {
        int w = client->overview_groups[group];
        WorkbenchWorkspace *workspace = &client->workspaces[w];
        float y = bounds.y + 116 + (group - client->overview_scroll.position) * overview_group_height(client);
        Rectangle strip = {bounds.x+16,y,bounds.width-32,overview_group_height(client)};
        if (!CheckCollisionRecs(strip,overview_content(client))) continue;
        overview_clip(client,(Rectangle){bounds.x+16,y,bounds.width-100,24});
        DrawTextEx(client->ui_font, workspace->name, (Vector2){bounds.x + 16, y + 4},UI_TEXT_BODY, 0, ui.text);
        EndScissorMode();
        int matches = 0;
        for (int i = 0; i < count; i++) matches += items[i].workspace == w;
        overview_clip(client,overview_content(client));
        if (matches > overview_columns_visible(client)) {
            draw_header_button(client->ui_font, overview_pan_button(client, group, false), "<", client->overview_horizontal[w].target > 0);
            draw_header_button(client->ui_font, overview_pan_button(client, group, true), ">", client->overview_horizontal[w].target + overview_columns_visible(client) < matches);
        }
        if (!matches) {
            Rectangle empty = {bounds.x + 16, y + 26, bounds.width - 32, overview_group_height(client) - 38};
            DrawTextEx(client->ui_font, workspace->count ? "Drop into this workspace" : "Empty workspace / drop a terminal here",
                (Vector2){empty.x + 16, empty.y + 16},UI_TEXT_BODY, 0, ui.muted);
        }
        EndScissorMode();
    }
    for (int i = 0; i < count; i++) {
        if (!overview_card_visible(client, items, i)) continue;
        WorkbenchPane *pane = client->workspaces[items[i].workspace].panes[items[i].pane];
        Rectangle card = overview_card(client, items, i);
        bool selected = i == client->overview_selected;
        bool hovered = CheckCollisionPointRec(GetMousePosition(), card);
        overview_clip(client,overview_content(client));
        ui_panel(card,ui.background,false);
        bool context_subtitle=pane->transport.context.available && (pane->transport.context.repository[0] || pane->custom_name);
        float header=context_subtitle ? 42 : 24;
        /* Previews were refreshed before drawing; clip only their presentation. */
        pane_preview_draw(client, pane, (Rectangle){card.x + 4, card.y + header + 2, card.width - 8, card.height - header - 28});
        DrawRectangleRec((Rectangle){card.x, card.y, card.width, header}, ui.raised);
        EndScissorMode();
        overview_clip(client,(Rectangle){card.x+8,card.y,card.width-24,header});
        DrawTextEx(client->ui_font, pane->overview_label[0] ? pane->overview_label : pane->name, (Vector2){card.x + 8, card.y + 5},UI_TEXT_BODY, 0, ui.text);
        EndScissorMode();
        if(context_subtitle) {
            overview_clip(client,(Rectangle){card.x+8,card.y+23,card.width-24,18});
            DrawTextEx(client->ui_font,pane->overview_directory,(Vector2){card.x+8,card.y+25},UI_TEXT_CAPTION,0,ui.muted);
            EndScissorMode();
        }
        overview_clip(client,overview_content(client));
        if (pane->unread) DrawCircle((int)(card.x + card.width - 12), (int)(card.y + 12), 3, ui.waiting);
        const char *agent = pane->closed ? "" : remote_agent_name(pane->transport.agent);
        const char *status = pane->closed ? (pane->transport.attached_elsewhere ? "Open elsewhere" : "Disconnected") : pane->transport.replaying ? "Connecting" : agent[0] ? remote_attention_name(pane->transport.attention) : "Connected";
        char detail[100];
        snprintf(detail, sizeof(detail), "%s%s%s", agent, agent[0] ? " / " : "", status);
        EndScissorMode();
        overview_clip(client,(Rectangle){card.x+8,card.y+card.height-23,card.width-16,23});
        DrawTextEx(client->ui_font, detail, (Vector2){card.x + 8, card.y + card.height - 18},UI_TEXT_CAPTION, 0,
            agent[0] ? remote_attention_color(pane->transport.attention) : ui.muted);
        EndScissorMode();
        overview_clip(client,overview_content(client));
        DrawRectangleRoundedLinesEx(card,.06f,8,selected ? 2 : 1,selected ? ui.accent : hovered ? ui.muted : ui.border);
        EndScissorMode();
    }
}
