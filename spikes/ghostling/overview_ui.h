/* Client-only spatial overview and searchable list. */
typedef struct { int workspace, pane; } OverviewItem;

static Rectangle overview_button(void) { return (Rectangle){482, 4, 80, 24}; }
static Rectangle overview_bounds(WorkspaceClient *client)
{
    float width = GetScreenWidth() - 48;
    if (client->overview_list && width > 720) width = 720;
    return (Rectangle){(GetScreenWidth() - width) / 2, 48, width, GetScreenHeight() - (client->overview_list ? 96 : 72)};
}
static int overview_visible_rows(WorkspaceClient *client)
{
    int count = ((int)overview_bounds(client).height - 156) / 42;
    return count > 0 ? count : 1;
}
static Rectangle overview_content(WorkspaceClient *client)
{
    Rectangle bounds = overview_bounds(client);
    return (Rectangle){bounds.x+16,bounds.y+116,bounds.width-32,fmaxf(1,bounds.height-156)};
}
static void overview_clip(WorkspaceClient *client, Rectangle area)
{
    Rectangle clip = GetCollisionRec(overview_content(client),area);
    BeginScissorMode((int)clip.x,(int)clip.y,(int)fmaxf(0,clip.width),(int)fmaxf(0,clip.height));
}
static Rectangle overview_row(WorkspaceClient *client, float index)
{
    Rectangle bounds = overview_bounds(client);
    return (Rectangle){bounds.x + 16, bounds.y + 116 + index * 42, bounds.width - 32, 38};
}

static bool overview_matches(const char *name, const char *query)
{
    if (!query[0]) return true;
    for (; *name; name++) {
        const unsigned char *a = (const unsigned char *)name, *b = (const unsigned char *)query;
        while (*a && *b && tolower(*a) == tolower(*b)) { a++; b++; }
        if (!*b) return true;
    }
    return false;
}

static bool overview_agent_matches(WorkspaceClient *client, WorkbenchPane *pane)
{
    return (!client->overview_waiting || (!pane->closed && pane->transport.attention == 2))
        && (!client->overview_agents || (!pane->closed && remote_agent_name(pane->transport.agent)[0]));
}
static Rectangle overview_agents_button(WorkspaceClient *client)
{
    Rectangle bounds = overview_bounds(client);
    return (Rectangle){bounds.x + bounds.width - 184, bounds.y + 14, 96, 24};
}

static Rectangle overview_waiting_button(WorkspaceClient *client)
{
    Rectangle bounds = overview_bounds(client);
    return (Rectangle){bounds.x + bounds.width - 288, bounds.y + 14, 96, 24};
}

static int overview_items(WorkspaceClient *client, OverviewItem *items)
{
    int count = 0;
    for (int w = 0; w < client->count; w++)
        for (int p = 0; p < client->workspaces[w].count; p++)
            if (overview_agent_matches(client, client->workspaces[w].panes[p])
                && (overview_matches(client->workspaces[w].name, client->overview_query)
                || overview_matches(client->workspaces[w].panes[p]->name, client->overview_query)
                || (!client->workspaces[w].panes[p]->closed && remote_agent_name(client->workspaces[w].panes[p]->transport.agent)[0]
                    && overview_matches(remote_agent_name(client->workspaces[w].panes[p]->transport.agent), client->overview_query))))
                items[count++] = (OverviewItem){w, p};
    client->overview_group_count = 0;
    for (int w = 0; w < client->count; w++) {
        bool included = client->overview_dragging;
        for (int i = 0; i < count; i++) included |= items[i].workspace == w;
        if (!client->workspaces[w].count && !client->overview_agents && !client->overview_waiting
            && overview_matches(client->workspaces[w].name, client->overview_query)) included = true;
        if (included) client->overview_groups[client->overview_group_count++] = w;
    }
    return count;
}

static Rectangle overview_view_button(WorkspaceClient *client)
{
    Rectangle bounds = overview_bounds(client);
    return (Rectangle){bounds.x + bounds.width - 392, bounds.y + 14, 96, 24};
}
#include "overview_spatial.h"
static void overview_apply_jump(WorkspaceClient *client, int workspace_index, int pane_index);
static bool overview_requested(void);
static void overview_draw(WorkspaceClient *client);
static void overview_jump(WorkspaceClient *client, OverviewItem item);
static Rectangle overview_theme_button(WorkspaceClient *client);
#include "overview_motion.h"
#include "overview_drag.h"

static void overview_open(WorkspaceClient *client)
{
    overview_animation_target(&client->overview_animation, 1, GetTime(), client->overview_animation_enabled && !client->overview_list ? 0.18 : 0);
    client->overview_open = true;
    client->overview_closing = false;
    client->overview_drag_armed = client->overview_dragging = false;
    client->overview_jump_workspace = client->overview_jump_pane = -1;
    client->overview_epoch++;
    client->overview_query[0] = '\0';
    client->overview_selected = client->overview_first = 0;
    client->overview_scroll = (OverviewScroll){0};
    for (int w=0;w<client->count;w++)
        client->overview_horizontal[w] = (OverviewScroll){.position=client->overview_columns[w],.target=client->overview_columns[w],.command=client->overview_columns[w]};
    client->overview_all = false;
    client->divider_dragging = false;
    for (int w = 0; w < client->count; w++)
        for (int p = 0; p < client->workspaces[w].count; p++) {
            WorkbenchPane *pane = client->workspaces[w].panes[p];
            pane->selection.dragging = pane->scrollbar_dragging = false;
        }
    OverviewItem items[WORKBENCH_MAX_WORKSPACES * WORKBENCH_PANES];
    int count = overview_items(client, items);
    for (int i = 0; i < count; i++)
        if (items[i].workspace == client->active && items[i].pane == client->workspaces[client->active].focused)
            client->overview_selected = i;
    overview_reveal(client, items, count);
    overview_scroll_update(client,items,count,true);
}

static bool overview_requested(void)
{
    return (clipboard_chord_down() && IsKeyPressed(KEY_O)
#if defined(__APPLE__)
        && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
#endif
        ) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
            && CheckCollisionPointRec(GetMousePosition(), overview_button()));
}

static void overview_apply_jump(WorkspaceClient *client, int workspace_index, int pane_index)
{
    client->active = workspace_index;
    WorkbenchWorkspace *workspace = &client->workspaces[workspace_index];
    workspace->focused = pane_index;
    if (pane_index < workspace->first) workspace->first = pane_index;
    if (pane_index >= workspace->first + 2) workspace->first = pane_index - 1;
    int visible = (GetScreenHeight() - 268) / 44;
    if (visible < 1) visible = 1;
    if (client->active < client->sidebar_first) client->sidebar_first = client->active;
    if (client->active >= client->sidebar_first + visible) client->sidebar_first = client->active - visible + 1;
    client->overview_open = false;
}

static void overview_jump(WorkspaceClient *client, OverviewItem item)
{
    overview_close(client, item.workspace, item.pane);
}

static bool overview_input(WorkspaceClient *client)
{
    if (client->management_busy && client->dialog_kind == DIALOG_TERMINAL_MOVE) {
        while (GetKeyPressed() != 0) { }
        while (GetCharPressed() != 0) { }
        return false;
    }
    if (client->overview_drag_armed) return overview_drag_input(client);
    bool escape = false, enter = false, backspace = false;
    int direction = 0, horizontal = 0, key;
    while ((key = GetKeyPressed()) != 0) {
        escape |= key == KEY_ESCAPE;
        enter |= key == KEY_ENTER;
        backspace |= key == KEY_BACKSPACE;
        if (key == KEY_UP) direction--;
        if (key == KEY_DOWN) direction++;
        if (key == KEY_LEFT) horizontal--;
        if (key == KEY_RIGHT) horizontal++;
    }
    if (IsKeyPressedRepeat(KEY_UP)) direction--;
    if (IsKeyPressedRepeat(KEY_DOWN)) direction++;
    if (IsKeyPressedRepeat(KEY_LEFT)) horizontal--;
    if (IsKeyPressedRepeat(KEY_RIGHT)) horizontal++;
    bool modifier = text_edit_modifier_down(), changed = false;
    if (modifier && IsKeyPressed(KEY_A)) client->overview_all = true;
    if (modifier && IsKeyPressed(KEY_V)) {
        const char *text = GetClipboardText();
        if (text) {
            size_t length = strnlen(text, sizeof(client->overview_query));
            size_t existing = client->overview_all ? 0 : strlen(client->overview_query);
            bool valid = existing + length < sizeof(client->overview_query);
            for (size_t i = 0; valid && i < length; i++)
                valid = (unsigned char)text[i] >= 32 && text[i] != 127;
            if (valid) {
                memcpy(client->overview_query + existing, text, length + 1);
                changed = true; client->overview_all = false;
            }
        }
    }
    int character;
    while ((character = GetCharPressed()) != 0) {
        if (modifier || character < 32 || character == 127) continue;
        char bytes[4]; int size = utf8_encode((uint32_t)character, bytes);
        size_t length = client->overview_all ? 0 : strlen(client->overview_query);
        if (length + (size_t)size < sizeof(client->overview_query)) {
            memcpy(client->overview_query + length, bytes, size);
            client->overview_query[length + size] = '\0';
            changed = true; client->overview_all = false;
        }
    }
    if (backspace || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        size_t length = strlen(client->overview_query);
        if (client->overview_all) length = 0;
        else if (length) {
            length--;
            while (length && ((unsigned char)client->overview_query[length] & 0xc0) == 0x80) length--;
        }
        client->overview_query[length] = '\0';
        changed = true; client->overview_all = false;
    }
    OverviewItem items[WORKBENCH_MAX_WORKSPACES * WORKBENCH_PANES];
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), overview_agents_button(client))) {
        client->overview_agents = !client->overview_agents; changed = true;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), overview_waiting_button(client))) {
        client->overview_waiting = !client->overview_waiting; changed = true;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), overview_view_button(client))) {
        client->overview_list = !client->overview_list;
        overview_animation_target(&client->overview_animation, 1, GetTime(), 0);
        client->overview_first = 0;
        client->overview_scroll = (OverviewScroll){0};
    }
    int count = overview_items(client, items), rows = overview_visible_rows(client);
    if (changed) { client->overview_selected = client->overview_first = 0; client->overview_scroll = (OverviewScroll){0}; }
    if (client->overview_list) client->overview_selected += direction;
    if (client->overview_selected < 0) client->overview_selected = 0;
    if (client->overview_selected >= count) client->overview_selected = count > 0 ? count - 1 : 0;
    if (!client->overview_list) overview_spatial_move(client, items, count, horizontal, direction);
    if (changed || direction || horizontal) overview_reveal(client, items, count);
    overview_scroll_update(client,items,count,changed);
    Rectangle bounds = overview_bounds(client);
    Vector2 pointer = GetMousePosition();
    overview_scroll_input(client,items,count);
    overview_scroll_update(client,items,count,false);
    if (client->overview_list) {
        int limit = count - rows;
        if (limit < 0) limit = 0;
        if (client->overview_first < 0) client->overview_first = 0;
        if (client->overview_first > limit) client->overview_first = limit;
    } else overview_spatial_clamp(client, items, count);
    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Rectangle dismiss = {bounds.x + bounds.width - 80, bounds.y + 14, 64, 24};
    if (escape || (click && CheckCollisionPointRec(pointer, dismiss))) overview_close(client, -1, -1);
    else if (enter && count) { overview_jump(client, items[client->overview_selected]); return true; }
    else if (click && !client->overview_list) {
        for (int i = 0; i < count; i++) {
            int group = overview_group(client, items, i);
            if (!CheckCollisionPointRec(pointer,overview_content(client))) continue;
            if (!overview_column(items, i)) {
                if (CheckCollisionPointRec(pointer, overview_pan_button(client, group, false))) overview_scroll_step(&client->overview_horizontal[items[i].workspace],&client->overview_columns[items[i].workspace],-1);
                if (CheckCollisionPointRec(pointer, overview_pan_button(client, group, true))) overview_scroll_step(&client->overview_horizontal[items[i].workspace],&client->overview_columns[items[i].workspace],1);
            }
            if (overview_card_visible(client, items, i) && CheckCollisionPointRec(pointer, overview_card(client, items, i))) {
                overview_drag_arm(client, items[i]); return false;
            }
        }
        overview_spatial_clamp(client, items, count);
    }
    else if (click)
        for (int i = (int)client->overview_scroll.position; i < count && i < client->overview_scroll.position + rows + 1; i++)
            if (CheckCollisionPointRec(pointer,overview_content(client)) && CheckCollisionPointRec(pointer, overview_row(client, i - client->overview_scroll.position))) {
                overview_jump(client, items[i]); return true;
            }
    return false;
}

static void overview_draw(WorkspaceClient *client)
{
    Rectangle bounds = overview_bounds(client);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 160});
    ui_panel(bounds, ui.surface, true);
    DrawTextEx(client->font, client->overview_list ? "Terminal overview" : "Workspace overview", (Vector2){bounds.x + 24, bounds.y + 20}, 16, 0, ui.text);
    DrawTextEx(client->font, client->overview_list ? "All workspaces / click a terminal to jump" : "Live previews / click to enter / scroll workspaces / swipe sideways or Shift+scroll terminals", (Vector2){bounds.x + 24, bounds.y + 47}, 12, 0, ui.muted);
    draw_header_button(client->font, overview_theme_button(client), "Theme", true);
    draw_header_button(client->font, overview_view_button(client), client->overview_list ? "Spatial" : "List", true);
    draw_header_button(client->font, overview_waiting_button(client), client->overview_waiting ? "All states" : "Needs input", true);
    draw_header_button(client->font, overview_agents_button(client), client->overview_agents ? "All terminals" : "Agents", true);
    draw_header_button(client->font, (Rectangle){bounds.x + bounds.width - 80, bounds.y + 14, 64, 24}, "Done", true);
    Rectangle field = {bounds.x + 24, bounds.y + 70, bounds.width - 48, 30};
    DrawRectangleRounded(field,.22f,8,client->overview_all ? ui.selected : ui.background);
    BeginScissorMode((int)field.x + 8, (int)field.y, (int)field.width - 16, (int)field.height);
    DrawTextEx(client->font, client->overview_query[0] ? client->overview_query : "Filter by workspace or terminal name...",
        (Vector2){field.x + 8, field.y + 8}, 14, 0, client->overview_query[0] ? ui.text : ui.muted);
    if (((int)(GetTime() * 2) % 2 == 0) && !client->overview_all) {
        Vector2 width = MeasureTextEx(client->font, client->overview_query, 14, 0);
        DrawLine((int)(field.x + 8 + width.x), (int)field.y + 7, (int)(field.x + 8 + width.x), (int)field.y + 23, ui.text);
    }
    EndScissorMode();
    OverviewItem items[WORKBENCH_MAX_WORKSPACES * WORKBENCH_PANES];
    int count = overview_items(client, items), rows = overview_visible_rows(client);
    if (!client->overview_list) overview_spatial_draw(client, items, count);
    else for (int i = (int)client->overview_scroll.position; i < count && i < client->overview_scroll.position + rows + 1; i++) {
        WorkbenchWorkspace *workspace = &client->workspaces[items[i].workspace];
        WorkbenchPane *pane = workspace->panes[items[i].pane];
        Rectangle row = overview_row(client, i - client->overview_scroll.position);
        bool selected = i == client->overview_selected;
        overview_clip(client,overview_content(client));
        DrawRectangleRec(row, selected ? ui.selected : ui.raised);
        EndScissorMode();
        overview_clip(client,(Rectangle){row.x+8,row.y,row.width-164,row.height});
        DrawTextEx(client->font, pane->name, (Vector2){row.x + 10, row.y + 5}, 14, 0, ui.text);
        char detail[128];
        const char *agent = pane->closed ? "" : remote_agent_name(pane->transport.agent);
        snprintf(detail, sizeof(detail), "%s%s%s", workspace->name, agent[0] ? " / " : "", agent);
        DrawTextEx(client->font, detail, (Vector2){row.x + 10, row.y + 23}, 12, 0, ui.muted);
        EndScissorMode();
        overview_clip(client,overview_content(client));
        if (pane->unread) DrawCircle((int)(row.x + row.width - 146), (int)(row.y + 19), 3, ui.waiting);
        DrawTextEx(client->font, pane->closed ? (pane->transport.attached_elsewhere ? "Open elsewhere" : "Disconnected") : pane->transport.replaying ? "Connecting" : agent[0] ? remote_attention_name(pane->transport.attention) : "Connected",
            (Vector2){row.x + row.width - 130, row.y + 12}, 12, 0, agent[0] ? remote_attention_color(pane->transport.attention) : ui.muted);
        EndScissorMode();
    }
    if (!client->overview_list) overview_drag_draw(client, items, count);
    if (!count && (client->overview_list || !client->overview_group_count)) DrawTextEx(client->font, client->overview_waiting ? "No agents need input" : client->overview_agents ? "No identified agents" : client->overview_query[0] ? "No matching terminals" : "No terminals yet",
        (Vector2){bounds.x + 24, bounds.y + 124}, 14, 0, ui.muted);
    char footer[256];
    snprintf(footer, sizeof(footer), "%d terminals / %s + Enter / Esc / dot: unseen output", count, client->overview_list ? "arrows" : "Left/Right: terminal / Up/Down: workspace");
    if (client->management_busy && client->dialog_kind == DIALOG_TERMINAL_MOVE)
        snprintf(footer, sizeof(footer), "Moving terminal...");
    else if (client->overview_notice_until > GetTime())
        snprintf(footer, sizeof(footer), "%.*s", (int)sizeof(footer) - 1, client->overview_notice);
    DrawTextEx(client->font, footer, (Vector2){bounds.x + 24, bounds.y + bounds.height - 26}, 12, 0, ui.muted);
}
