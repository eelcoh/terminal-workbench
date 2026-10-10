#include "theme_catalog.h"
#include "appearance_state.h"
/* Experimental client layout. Runtime processes remain independently owned.
 * Terminal setup/rendering builds on the MIT-licensed Ghostling example;
 * see LICENSE.ghostling for its copyright and permission notice. */
#undef GetScreenWidth
#undef GetScreenHeight
#undef GetMousePosition

#include "overview_animation.h"

#define WORKBENCH_MAX_WORKSPACES 8
#define WORKBENCH_PANES 8
#define WORKBENCH_SIDEBAR 184
#define WORKBENCH_GAP 10

typedef struct {
    RemoteSession transport;
    SelectionUI selection;
    GhosttyTerminal terminal;
    GhosttyKeyEncoder keys;
    GhosttyKeyEvent key;
    GhosttyMouseEncoder mouse;
    GhosttyMouseEvent mouse_event;
    GhosttyRenderState render;
    GhosttyRenderStateRowIterator rows;
    GhosttyRenderStateRowCells cells;
    GhosttyKittyGraphicsPlacementIterator images;
    EffectsContext effects;
    Theme theme;
    RenderTexture2D canvas, preview;
    uint64_t preview_revision, preview_epoch;
    uint16_t preview_cols, preview_rows;
    size_t preview_scroll;
    double preview_updated;
    Rectangle bounds;
    char socket_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    char id[41], name[64];
    int fd;
    bool closed, scrollbar_dragging, unread, focused;
} WorkbenchPane;

typedef struct {
    char id[41], name[64];
    WorkbenchPane *panes[WORKBENCH_PANES];
    int focused, first, count;
} WorkbenchWorkspace;

typedef struct {
    WorkbenchWorkspace workspaces[WORKBENCH_MAX_WORKSPACES];
    int count, active;
    float divider;
    bool divider_dragging;
    int cell_width, cell_height;
    Font font;
    const char *catalog_path;
    bool managed, dialog_open, dialog_all, management_busy;
    bool theme_open, theme_has_custom;
    int theme_selected, theme_ignored, theme_filter, theme_first;
    char theme_query[64];
    Theme theme_catalog[THEME_CATALOG_COUNT];
    int theme_catalog_ignored[THEME_CATALOG_COUNT];
    Theme theme_custom, theme_imported;
    bool theme_has_import, appearance_dirty;
    AppearanceState appearance, appearance_before;
    int theme_scope;
    char appearance_active[41];
    char theme_path[1024], theme_error[256];
    int dialog_kind, management_fd;
    pid_t management_pid;
    char edit_name[64], dialog_error[256], management_reply[512];
    size_t management_length;
    int sidebar_first;
    bool overview_open, overview_all, overview_agents, overview_waiting, overview_list;
    int overview_selected, overview_first;
    char overview_query[64];
    int overview_columns[WORKBENCH_MAX_WORKSPACES];
    uint64_t overview_epoch;
    OverviewAnimation overview_animation;
    bool overview_animation_enabled, overview_closing, overview_exit_has_card;
    int overview_jump_workspace, overview_jump_pane;
    Rectangle overview_exit_card;
    RenderTexture2D overview_stage;
    int overview_groups[WORKBENCH_MAX_WORKSPACES], overview_group_count;
    bool overview_drag_armed, overview_dragging;
    char overview_drag_id[41], overview_move_id[41], overview_notice[256];
    Vector2 overview_drag_start;
    double overview_drag_scroll, overview_notice_until;
} WorkspaceClient;

static void pane_bind(WorkbenchPane *pane)
{
    active_terminal_theme = &pane->theme;
    active_remote = &pane->transport;
    active_selection = &pane->selection;
    client_surface = pane->bounds;
    client_surface_active = true;
}

static void pane_unbind(void)
{
    client_surface_active = false;
    active_terminal_theme = NULL;
    active_remote = &single_remote;
    active_selection = &single_selection;
}

static void pane_free(WorkbenchPane *pane)
{
    pane_bind(pane);
    if (pane->fd >= 0) close(pane->fd);
    if (pane->transport.agent_fd >= 0) close(pane->transport.agent_fd);
    if (pane->canvas.id) UnloadRenderTexture(pane->canvas);
    if (pane->preview.id) UnloadRenderTexture(pane->preview);
    if (pane->images) ghostty_kitty_graphics_placement_iterator_free(pane->images);
    if (pane->cells) ghostty_render_state_row_cells_free(pane->cells);
    if (pane->rows) ghostty_render_state_row_iterator_free(pane->rows);
    if (pane->render) ghostty_render_state_free(pane->render);
    if (pane->mouse_event) ghostty_mouse_event_free(pane->mouse_event);
    if (pane->mouse) ghostty_mouse_encoder_free(pane->mouse);
    if (pane->key) ghostty_key_event_free(pane->key);
    if (pane->keys) ghostty_key_encoder_free(pane->keys);
    selection_ui_free(pane->terminal);
    if (pane->terminal) ghostty_terminal_free(pane->terminal);
    pane_unbind();
    free(pane);
}

static bool pane_init(WorkspaceClient *client, WorkbenchPane *pane)
{
    pane_bind(pane);
    GhosttyResult result = ghostty_terminal_new(NULL, &pane->terminal, 80, 24);
    if (result != GHOSTTY_SUCCESS || !selection_ui_init()) return false;
    pane->theme=client->appearance.defaults;
    ui_apply_terminal(pane->terminal,&pane->theme);
    size_t history = 1000;
    ghostty_terminal_set(pane->terminal, GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_LINES, &history);
    ghostty_terminal_resize(pane->terminal, 80, 24, client->cell_width, client->cell_height);
    pane->fd = remote_connect(pane->socket_path, 80, 24, client->cell_width, client->cell_height);
    if (pane->fd < 0) return false;
    pane->effects = (EffectsContext){ .pty_fd = pane->fd,
        .cell_width = client->cell_width, .cell_height = client->cell_height,
        .cols = 80, .rows = 24, .theme=&pane->theme };
    ghostty_terminal_set(pane->terminal, GHOSTTY_TERMINAL_OPT_USERDATA, &pane->effects);
    ghostty_terminal_set(pane->terminal, GHOSTTY_TERMINAL_OPT_WRITE_PTY, (const void *)effect_write_pty);
    ghostty_terminal_set(pane->terminal, GHOSTTY_TERMINAL_OPT_SIZE, (const void *)effect_size);
    ghostty_terminal_set(pane->terminal, GHOSTTY_TERMINAL_OPT_DEVICE_ATTRIBUTES, (const void *)effect_device_attributes);
    ghostty_terminal_set(pane->terminal, GHOSTTY_TERMINAL_OPT_XTVERSION, (const void *)effect_xtversion);
    /* Pane OSC titles must never rename the shared application window. */
    ghostty_terminal_set(pane->terminal, GHOSTTY_TERMINAL_OPT_COLOR_SCHEME, (const void *)effect_color_scheme);
    uint64_t image_budget = 16 * 1024 * 1024;
    ghostty_terminal_set(pane->terminal, GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_STORAGE_LIMIT, &image_budget);
    result = ghostty_key_encoder_new(NULL, &pane->keys);
    if (result == GHOSTTY_SUCCESS) result = ghostty_key_event_new(NULL, &pane->key);
    if (result == GHOSTTY_SUCCESS) result = ghostty_mouse_encoder_new(NULL, &pane->mouse);
    if (result == GHOSTTY_SUCCESS) result = ghostty_mouse_event_new(NULL, &pane->mouse_event);
    if (result == GHOSTTY_SUCCESS) result = ghostty_render_state_new(NULL, &pane->render);
    if (result == GHOSTTY_SUCCESS) result = ghostty_render_state_row_iterator_new(NULL, &pane->rows);
    if (result == GHOSTTY_SUCCESS) result = ghostty_render_state_row_cells_new(NULL, &pane->cells);
    if (result == GHOSTTY_SUCCESS) result = ghostty_kitty_graphics_placement_iterator_new(NULL, &pane->images);
    pane_unbind();
    return result == GHOSTTY_SUCCESS;
}

#include "catalog_view.h"
static void overview_move_saved(WorkspaceClient *client);
#include "management_ui.h"
#include <ctype.h>
static Rectangle workspace_pane_bounds(WorkspaceClient *client, int count, int first, int pane)
{
    int left = WORKBENCH_SIDEBAR + WORKBENCH_GAP;
    int available = GetScreenWidth() - left - WORKBENCH_GAP * 2;
    int width = (int)(available * client->divider);
    int height = GetScreenHeight() - WORKBENCH_HEADER_HEIGHT - WORKBENCH_GAP * 2;
    if (count - first == 1) return (Rectangle){left, 42, available + WORKBENCH_GAP, height};
    if (pane == first) return (Rectangle){left, 42, width, height};
    return (Rectangle){left + width + WORKBENCH_GAP, 42, available - width, height};
}
static void pane_preview_refresh(WorkspaceClient *client, WorkbenchPane *pane);
static void pane_preview_draw(WorkspaceClient *client, WorkbenchPane *pane, Rectangle target);
#include "overview_ui.h"
#include "theme_ui.h"

static bool workspace_layout(WorkspaceClient *client)
{
    /* Hidden workspaces retain their geometry until entry. */
    WorkbenchWorkspace *workspace = &client->workspaces[client->active];
    if (!workspace->count) return true;
    int visible = workspace->count - workspace->first;
    if (visible > 2) visible = 2;
    for (int p = workspace->first; p < workspace->first + visible; p++)
        workspace->panes[p]->bounds = workspace_pane_bounds(client, workspace->count, workspace->first, p);
    for (int p = workspace->first; p < workspace->first + visible; p++) {
        WorkbenchPane *pane = workspace->panes[p];
        pane_bind(pane);
        int cols = ((int)pane->bounds.width - 8) / client->cell_width;
        int rows = ((int)pane->bounds.height - WORKBENCH_HEADER_HEIGHT - 8) / client->cell_height;
        if (cols < 1) cols = 1;
        if (rows < 1) rows = 1;
        if (cols > 1000) cols = 1000;
        if (rows > 1000) rows = 1000;
        if (cols != pane->transport.wanted_cols || rows != pane->transport.wanted_rows)
            remote_resize(pane->fd, (uint16_t)cols, (uint16_t)rows);
        int width = (int)pane->bounds.width, canvas_height = (int)pane->bounds.height;
        if (pane->canvas.texture.width != width || pane->canvas.texture.height != canvas_height) {
            if (pane->canvas.id) UnloadRenderTexture(pane->canvas);
            pane->canvas = LoadRenderTexture(width, canvas_height);
            if (!pane->canvas.id) {
                fprintf(stderr, "could not allocate terminal canvas\n");
                pane_unbind();
                return false;
            }
        }
    }
    pane_unbind();
    return true;
}

static void pane_focus_event(WorkbenchPane *pane, bool focused)
{
    if (pane->focused == focused) return;
    pane_bind(pane);
    pane->focused = focused;
    char bytes[16]; size_t written = 0;
    GhosttyTerminalModeConfig mode = { .mode = GHOSTTY_MODE_FOCUS_EVENT };
    ghostty_terminal_get(pane->terminal, GHOSTTY_TERMINAL_DATA_MODE, &mode);
    if (mode.value)
        ghostty_focus_encode(focused ? GHOSTTY_FOCUS_GAINED : GHOSTTY_FOCUS_LOST,
                             bytes, sizeof(bytes), &written);
    if (written) pty_write(pane->fd, bytes, written);
    pane_unbind();
}

static Rectangle workspace_row(int index)
{
    return (Rectangle){8, 88 + index * 44, WORKBENCH_SIDEBAR - 16, 36};
}

static void workspace_chrome(WorkspaceClient *client)
{
    DrawRectangle(0, 32, WORKBENCH_SIDEBAR, GetScreenHeight() - 32, ui.surface);
    DrawTextEx(client->font, "WORKSPACES", (Vector2){16, 56}, 14, 0, ui.muted);
    int visible_rows = (GetScreenHeight() - 268) / 44;
    if (visible_rows < 1) visible_rows = 1;
    for (int w = client->sidebar_first; w < client->count && w < client->sidebar_first + visible_rows; w++) {
        Rectangle row = workspace_row(w - client->sidebar_first);
        bool active = w == client->active;
        bool unread = false;
        for (int p = 0; p < client->workspaces[w].count; p++) unread |= client->workspaces[w].panes[p]->unread;
        bool hovered=CheckCollisionPointRec(GetMousePosition(),row);
        DrawRectangleRounded(row,.22f,8,active ? ui.selected : hovered ? ui.hover : ui.surface);
        if (active) DrawRectangleRounded((Rectangle){row.x+2,row.y+10,3,row.height-20},.5f,6,ui.accent);
        BeginScissorMode((int)row.x + 8, (int)row.y, (int)row.width - 32, (int)row.height);
        DrawTextEx(client->font, client->workspaces[w].name, (Vector2){row.x + 10, row.y + 10}, 14, 0,
                   active ? ui.text : ui.muted);
        EndScissorMode();
        if (unread) DrawCircle((int)(row.x + row.width - 12), (int)(row.y + 18), 3, ui.waiting);
    }
    draw_header_button(client->font, theme_button(), "Appearance", !client->dialog_open);
    DrawTextEx(client->font, "Dot: unseen output", (Vector2){12, GetScreenHeight() - 28}, 12, 0, ui.muted);
    WorkbenchWorkspace *workspace = &client->workspaces[client->active];
    WorkbenchPane *focused = workspace->panes[workspace->focused];
    bool can_copy = focused && selection_active(focused->terminal);
    bool can_paste = focused && !focused->closed && !focused->transport.replaying;
    draw_window_controls(client->font, can_copy, can_paste, clipboard_current_notice());
    management_controls(client);
    draw_header_button(client->font, overview_button(), "Overview", !client->dialog_open);
}

static void pane_draw(WorkspaceClient *client, WorkbenchPane *pane, int index, bool focused)
{
    pane_bind(pane);
    ghostty_render_state_update(pane->render, pane->terminal);
    GhosttyRenderStateColors colors = GHOSTTY_INIT_SIZED(GhosttyRenderStateColors);
    ghostty_render_state_colors_get(pane->render, &colors);
    GhosttyTerminalScrollbar scrollbar = {0};
    ghostty_terminal_get(pane->terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &scrollbar);
    BeginTextureMode(pane->canvas);
    ClearBackground((Color){colors.background.r, colors.background.g, colors.background.b, 255});
    render_terminal(pane->render, pane->rows, pane->cells, client->font,
                    client->cell_width, client->cell_height, 16, 4,
                    &scrollbar, pane->terminal, pane->images);
    DrawRectangle(0, 0, (int)pane->bounds.width, 32, ui.surface);
    (void)index;
    char label[128];
    const char *agent = pane->closed ? "" : remote_agent_name(pane->transport.agent);
    snprintf(label, sizeof(label), "%s%s%s%s%s%s", pane->name, agent[0] ? " / " : "", agent,
             agent[0] ? " / " : "", agent[0] ? remote_attention_name(pane->transport.attention) : "",
             pane->closed ? "  /  connection closed" : pane->transport.replaying ? "  /  connecting" : "");
    BeginScissorMode(8, 0, (int)pane->bounds.width - 150, 32);
    DrawTextEx(client->font, label, (Vector2){10, 8}, 14, 0, ui.text);
    EndScissorMode();
    if (pane->unread) DrawCircle((int)pane->bounds.width - 144, 16, 3, ui.waiting);
    if (client->managed) {
        draw_header_button(client->font, (Rectangle){pane->bounds.width - 132, 6, 54, 20}, "Stop", true);
        draw_header_button(client->font, (Rectangle){pane->bounds.width - 72, 6, 64, 20}, "Rename", true);
    }
    EndTextureMode();
    pane_unbind();
    DrawTextureRec(pane->canvas.texture,
        (Rectangle){0, 0, pane->bounds.width, -pane->bounds.height},
        (Vector2){pane->bounds.x, pane->bounds.y}, WHITE);
    DrawRectangleLinesEx(pane->bounds, 1,
        focused ? ui.accent : ui.border);
}

#include "terminal_preview.h"

static int workspace_main(const char *catalog)
{
    WorkspaceClient *client = calloc(1, sizeof(*client));
    if (!client) return 1;
    if (!workspace_catalog(client, catalog)) { free(client); return 1; }
    client->catalog_path = catalog;
    client->managed = getenv("WORKBENCH_CATALOG_SCRIPT") != NULL;
    client->management_fd = -1;
    client->management_pid = -1;
    client->divider = 0.5f;
    const char *reduced_motion = getenv("WORKBENCH_REDUCED_MOTION");
    client->overview_animation_enabled = !reduced_motion || strcmp(reduced_motion, "1");
    client->overview_jump_workspace = client->overview_jump_pane = -1;
    int exit_code = 0;
    if (!theme_initialize(client)) { free(client); return 1; }
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    InitWindow(1240, 760, "Terminal Workbench — workspace spike");
    SetExitKey(KEY_NULL);
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetWindowMinSize(800, 300);
    SetTargetFPS(60);
    Vector2 dpi = GetWindowScaleDPI();
    int font_pixels = (int)(16 * dpi.y);
    client->font = LoadFontFromMemory(".ttf", font_jetbrains_mono,
                       (int)sizeof(font_jetbrains_mono), font_pixels, NULL, 0);
    SetTextureFilter(client->font.texture, TEXTURE_FILTER_BILINEAR);
    Vector2 glyph = MeasureTextEx(client->font, "M", font_pixels, 0);
    client->cell_width = (int)(glyph.x / dpi.x);
    client->cell_height = (int)(glyph.y / dpi.y);
    if (client->cell_width < 1) client->cell_width = 1;
    if (client->cell_height < 1) client->cell_height = 1;
    ghostty_sys_set(GHOSTTY_SYS_OPT_DECODE_PNG, (const void *)decode_png);
    for (int w = 0; w < client->count; w++)
        for (int p = 0; p < client->workspaces[w].count; p++)
            if (!pane_init(client, client->workspaces[w].panes[p])) { exit_code = 1; goto cleanup; }

    while (!WindowShouldClose()) {
        overview_motion_tick(client);
        theme_sync(client);
        if (!client->theme_open && !client->dialog_open && !client->overview_open && window_close_requested()) break;
        bool theme_frame=client->theme_open;
        if (!client->theme_open && !client->dialog_open && !client->management_busy && !client->overview_drag_armed
            && !client->overview_closing && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
            && CheckCollisionPointRec(GetMousePosition(),client->overview_open ? overview_theme_button(client) : theme_button())) {
            theme_open(client); theme_frame=true;
        }
        bool toggled_overview = !client->theme_open && !client->dialog_open && !client->overview_drag_armed && !client->management_busy && overview_requested();
        if (toggled_overview) {
            if (client->overview_open && !client->overview_closing) overview_close(client, -1, -1);
            else overview_open(client);
        }
        bool modal_frame = theme_frame || client->dialog_open || client->overview_open || toggled_overview;
        if (!management_poll(client)) { exit_code = 1; break; }
        Vector2 pointer = GetMousePosition();
        bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (!modal_frame && pointer.x < WORKBENCH_SIDEBAR && pointer.y >= 88 && pointer.y < GetScreenHeight() - 180) {
            float wheel = GetMouseWheelMove();
            int visible_rows = (GetScreenHeight() - 268) / 44;
            if (visible_rows < 1) visible_rows = 1;
            if (wheel) client->sidebar_first += wheel > 0 ? -1 : 1;
            int limit = client->count - visible_rows;
            if (limit < 0) limit = 0;
            if (client->sidebar_first < 0) client->sidebar_first = 0;
            if (client->sidebar_first > limit) client->sidebar_first = limit;
        }
        WorkbenchWorkspace *workspace = &client->workspaces[client->active];
        WorkbenchPane *old_focus = workspace->panes[workspace->focused];
        bool gesture_active = old_focus && (old_focus->selection.dragging || old_focus->scrollbar_dragging);
        if (pressed && !gesture_active && !modal_frame) {
            int visible_rows = (GetScreenHeight() - 268) / 44;
            if (visible_rows < 1) visible_rows = 1;
            for (int w = client->sidebar_first; w < client->count && w < client->sidebar_first + visible_rows; w++) {
                if (CheckCollisionPointRec(pointer, workspace_row(w - client->sidebar_first))) client->active = w;
            }
            workspace = &client->workspaces[client->active];
            for (int p = workspace->first; p < workspace->count && p < workspace->first + 2; p++)
                if (CheckCollisionPointRec(pointer, workspace->panes[p]->bounds)) workspace->focused = p;
            management_click(client, pointer);
            if (client->dialog_open) modal_frame = true;
        }
        Rectangle divider = {workspace->count ? workspace->panes[workspace->first]->bounds.x + workspace->panes[workspace->first]->bounds.width : 0,
                             42, WORKBENCH_GAP, GetScreenHeight() - 52};
        if (pressed && !gesture_active && !modal_frame && workspace->count - workspace->first > 1 && CheckCollisionPointRec(pointer, divider)) client->divider_dragging = true;
        if (client->divider_dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float width = GetScreenWidth() - WORKBENCH_SIDEBAR - WORKBENCH_GAP * 3;
            client->divider = (pointer.x - WORKBENCH_SIDEBAR - WORKBENCH_GAP) / width;
            if (client->divider < 0.25f) client->divider = 0.25f;
            if (client->divider > 0.75f) client->divider = 0.75f;
        }
        bool divider_consumed = client->divider_dragging;
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) client->divider_dragging = false;
        theme_sync(client);
        bool layout_deferred = client->theme_open || (client->overview_open && !client->overview_list);
        if (!layout_deferred && !workspace_layout(client)) { exit_code = 1; break; }
        /* All sessions parse output. Only normal visible panes or visible
         * overview cards perform presentation work; previews never resize PTYs. */
        for (int w = 0; w < client->count; w++) {
            for (int p = 0; p < client->workspaces[w].count; p++) {
                WorkbenchPane *pane = client->workspaces[w].panes[p];
                bool focused = !modal_frame && w == client->active && p == workspace->focused && IsWindowFocused();
                pane_bind(pane);
                if (!pane->closed) {
                    uint64_t before = pane->transport.output_revision;
                    if (pty_read(pane->fd, pane->terminal) != PTY_READ_OK) pane->closed = true;
                    pane->effects.cols = pane->transport.cols;
                    pane->effects.rows = pane->transport.rows;
                    if (!focused && pane->transport.output_revision != before) pane->unread = true;
                }
                if (focused) pane->unread = false;
                pane_unbind();
                pane_focus_event(pane, focused && !pane->closed && !pane->transport.replaying);
            }
        }
        WorkbenchPane *focused = workspace->panes[workspace->focused];
        if (focused) {
            pane_bind(focused);
            bool paste_ready = !focused->closed && !focused->transport.replaying;
            /* Header coordinates are window-local. Input and selection below use
             * pane-local coordinates; each pointer event has a single owner. */
            client_surface_active = false;
            bool clipboard_key = modal_frame ? false : handle_clipboard(focused->fd, focused->terminal, paste_ready);
            client_surface_active = true;
            if (!modal_frame && paste_ready && !clipboard_key && IsWindowFocused()) {
                prepare_terminal_keyboard_input(focused->terminal);
                handle_input(focused->fd, focused->keys, focused->key, focused->terminal);
            }
            bool in_pane = CheckCollisionPointRec(pointer, focused->bounds);
            if (!modal_frame && !divider_consumed && (in_pane || focused->selection.dragging || focused->scrollbar_dragging)) {
                bool scroll = handle_scrollbar(focused->terminal, focused->render, &focused->scrollbar_dragging);
                bool selection = handle_selection(focused->terminal, client->cell_width, client->cell_height, 4, scroll);
                if (!scroll && surface_pointer().y >= 32) {
                    if (selection || focused->closed) selection_scroll(focused->terminal);
                    else if (paste_ready) handle_mouse(focused->fd, focused->mouse, focused->mouse_event,
                                                focused->terminal, client->cell_width, client->cell_height, 4);
                }
            }
            pane_unbind();
        }
        if (client->dialog_open) dialog_input(client);
        bool overview_jumped = false;
        if (theme_frame) theme_input(client);
        else if (client->overview_open && !client->overview_closing) overview_jumped = overview_input(client);
        else if (client->overview_closing) {
            while (GetKeyPressed() != 0) { }
            while (GetCharPressed() != 0) { }
        }
        else if (toggled_overview) {
            while (GetKeyPressed() != 0) { }
            while (GetCharPressed() != 0) { }
        }
        workspace = &client->workspaces[client->active];
        if (((overview_jumped && !client->overview_closing) || (layout_deferred && !client->theme_open && (!client->overview_open || client->overview_list))) && !workspace_layout(client)) { exit_code = 1; break; }
        theme_sync(client);
        if (client->overview_open && !client->overview_list) overview_previews_refresh(client);
        BeginDrawing();
        ClearBackground(ui.background);
        if (!client->overview_open || client->overview_list)
        for (int p = workspace->first; p < workspace->count && p < workspace->first + 2; p++)
            pane_draw(client, workspace->panes[p], p, p == workspace->focused);
        if (!workspace->count) DrawTextEx(client->font, "Create a terminal to start working", (Vector2){224, 110}, 16, 0, ui.muted);
        workspace_chrome(client);
        if (client->dialog_open) dialog_draw(client);
        if (client->overview_open) overview_present(client);
        if (client->theme_open) theme_draw(client);
        EndDrawing();
        flush_deferred_textures();
    }
cleanup:
    if (client->management_busy) {
        int status;
        while (waitpid(client->management_pid, &status, 0) < 0 && errno == EINTR) { }
        close(client->management_fd);
    }
    pane_unbind();
    for (int w = 0; w < client->count; w++)
        for (int p = 0; p < client->workspaces[w].count; p++) pane_free(client->workspaces[w].panes[p]);
    flush_deferred_textures();
    if (client->overview_stage.id) UnloadRenderTexture(client->overview_stage);
    UnloadFont(client->font);
    CloseWindow();
    free(client);
    return exit_code;
}
