#include "theme_catalog.h"
#include "ui_font.h"
#include "pixel_geometry.h"
#include "platform_display.h"
#include "appearance_state.h"
/* Experimental client layout. Runtime processes remain independently owned.
 * Terminal setup/rendering builds on the MIT-licensed Ghostling example;
 * see LICENSE.ghostling for its copyright and permission notice. */
#undef GetScreenWidth
#undef GetScreenHeight
#undef GetMousePosition

#include "overview_animation.h"
#include "pane_navigation.h"
#include "pane_width.h"
#include "overview_scroll.h"
#include "window_state.h"
#include "shortcut_state.h"
#include "ui_focus.h"
#include <sys/stat.h>
#include <limits.h>

#define WORKBENCH_MAX_WORKSPACES 8
#define WORKBENCH_PANES 8
static int chrome_sidebar_width=44,chrome_pane_count=0;
#define WORKBENCH_SIDEBAR chrome_sidebar_width
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
    bool closed, scrollbar_dragging, unread, focused, status_checked;
    int runtime_state;
    PaneWidth width;
} WorkbenchPane;

typedef struct {
    char id[41], name[64], directory[1024];
    WorkbenchPane *panes[WORKBENCH_PANES];
    int focused, first, count;
    float scroll, estate;
    bool manual_scroll;
} WorkbenchWorkspace;

typedef struct { RenderTexture2D canvas; uint64_t epoch; } NavigationTile;

typedef struct {
    WorkbenchWorkspace workspaces[WORKBENCH_MAX_WORKSPACES];
    int count, active;
    const char *window_state_path;
    WindowState saved_view;
    double view_checked;
    bool view_loaded, view_restore_overview, view_has_overview;
    int divider_pane, divider_preset;
    float divider_preview;
    bool divider_dragging;
    int cell_width, cell_height;
    Font font, ui_font;
    float raster_scale, raster_override;
    double display_checked;
    DisplayWorkarea display_work;
    const char *catalog_path;
    struct stat catalog_stamp;
    double catalog_checked;
    bool managed, dialog_open, dialog_all, management_busy, refreshing;
    char claim_new[41];
    UiFocus dialog_focus, command_focus, help_focus, theme_focus, overview_focus, shortcut_focus;
    bool command_open, command_all, help_open;
    float help_scroll;
    RenderTexture2D help_canvas;
    bool help_dirty, help_cache_failed;
    char help_bindings[COMMAND_COUNT][96];
    int command_selected, command_first;
    char command_query[64];
    ShortcutState shortcuts;
    const char *shortcut_path;
    char shortcut_error[256];
    bool command_settings, shortcut_recording, shortcut_capture_valid, forwarding;
    int shortcut_edit, passthrough_key;
    ShortcutChord shortcut_candidate;
    char forward_terminal[41];
    double forward_deadline;
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
    char edit_directory[1024];
    bool edit_folder;
    char edit_name[64], dialog_error[256], management_reply[512], management_target[41];
    size_t management_length;
    int sidebar_first;
    bool chrome_pinned, chrome_drawer, chrome_agents, chrome_waiting, chrome_quit, chrome_keyboard;
    int chrome_menu, chrome_menu_first, chrome_first;
    UiFocus chrome_focus;
    Rectangle chrome_anchor;
    char chrome_target[41];
    bool navigation_valid, navigation_reset;
    char navigation_workspace[41];
    int navigation_first, navigation_focused, navigation_width, navigation_height;
    float navigation_scroll;
    uint64_t navigation_epoch;
    NavigationTile navigation_tiles[WORKBENCH_PANES];
    OverviewAnimation navigation_animation;
    PaneNavigationWheel navigation_wheel;
    bool overview_open, overview_all, overview_agents, overview_waiting, overview_list;
    int overview_selected, overview_first;
    char overview_query[64];
    int overview_columns[WORKBENCH_MAX_WORKSPACES];
    OverviewScroll overview_scroll, overview_horizontal[WORKBENCH_MAX_WORKSPACES];
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
    free(pane->transport.snapshot);
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

static bool pane_attach(WorkspaceClient *client, WorkbenchPane *pane, bool takeover)
{
    pane_bind(pane);
    if (pane->fd >= 0) close(pane->fd);
    if (pane->transport.agent_fd >= 0) close(pane->transport.agent_fd);
    free(pane->transport.snapshot);
    bool earlier_runtime=takeover && pane->transport.takeover_unsupported;
    memset(&pane->transport,0,sizeof(pane->transport));
    pane->transport.agent_fd=-1;
    pane->transport.replaying=true;
    pane->transport.takeover=takeover && !earlier_runtime;
    pane->transport.takeover_unsupported=earlier_runtime;
    ghostty_selection_gesture_reset(pane->selection.gesture,pane->terminal);
    pane->selection.dragging=false;
    pane->scrollbar_dragging=false;
    pane->focused=false;
    pane->fd=remote_connect(pane->socket_path,80,24,client->cell_width,client->cell_height);
    pane->effects.pty_fd=pane->fd;
    pane->closed=pane->fd<0;
    pane->status_checked=false;
    pane->runtime_state=0;
    pane_unbind();
    return !pane->closed;
}

static bool pane_move_here(WorkspaceClient *client, WorkbenchPane *pane)
{
    return pane_attach(client,pane,true);
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
    pane->transport.takeover=client->claim_new[0] && !strcmp(client->claim_new,pane->id);
    pane->fd = remote_connect(pane->socket_path, 80, 24, client->cell_width, client->cell_height);
    /* Managed windows keep stopped saved entries visible for explicit recovery.
     * A standalone viewer still reports an unusable startup endpoint as failure. */
    if (pane->fd < 0 && !client->refreshing && !client->managed) return false;
    pane->closed=pane->fd<0;
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

static float workspace_estate(void) { return GetScreenWidth()-WORKBENCH_SIDEBAR-WORKBENCH_GAP*2; }
static void workspace_widths(const WorkbenchWorkspace *workspace, PaneWidth *widths)
{ for (int p=0;p<workspace->count;p++) widths[p]=workspace->panes[p]->width; }
static float workspace_offset(WorkbenchWorkspace *workspace, int pane)
{
    PaneWidth widths[WORKBENCH_PANES]; workspace_widths(workspace,widths);
    return pane_width_start(widths,pane,workspace_estate(),WORKBENCH_GAP);
}
static bool workspace_visible(WorkbenchWorkspace *workspace, int pane)
{
    PaneWidth widths[WORKBENCH_PANES]; workspace_widths(workspace,widths);
    return pane_width_visible(widths,pane,workspace->scroll,workspace_estate(),WORKBENCH_GAP);
}
static void workspace_sync_view(WorkbenchWorkspace *workspace)
{
    int requested=workspace->focused;
    if (!workspace->count) { workspace->focused=workspace->first=0; workspace->scroll=0; return; }
    if (requested<0) requested=0;
    if (requested>=workspace->count) requested=workspace->count-1;
    workspace->focused=requested;
    float estate=workspace_estate();
    if (workspace->estate>0 && workspace->estate!=estate)
        workspace->scroll*=(estate+WORKBENCH_GAP)/(workspace->estate+WORKBENCH_GAP);
    workspace->estate=estate;
    PaneWidth widths[WORKBENCH_PANES]; workspace_widths(workspace,widths);
    if (workspace->manual_scroll) {
        float maximum=fmaxf(0,pane_width_start(widths,workspace->count,estate,WORKBENCH_GAP)-WORKBENCH_GAP-estate);
        workspace->scroll=fmaxf(0,fminf(maximum,workspace->scroll));
    } else workspace->scroll=pane_width_reveal(widths,workspace->count,requested,workspace->scroll,estate,WORKBENCH_GAP);
    workspace->first=0;
    while (workspace->first<workspace->count-1 && !workspace_visible(workspace,workspace->first)) workspace->first++;
}
static void workspace_focus(WorkbenchWorkspace *workspace,int requested)
{
    workspace->focused=requested;workspace->manual_scroll=false;workspace_sync_view(workspace);
}
static Rectangle workspace_pane_bounds(WorkbenchWorkspace *workspace, int pane)
{
    PaneWidth widths[WORKBENCH_PANES]; workspace_widths(workspace,widths);
    return (Rectangle){WORKBENCH_SIDEBAR+WORKBENCH_GAP+workspace_offset(workspace,pane)-workspace->scroll,50,
        pane_width_pixels(widths,pane,workspace_estate(),WORKBENCH_GAP),GetScreenHeight()-60};
}

#include "catalog_view.h"
static void overview_move_saved(WorkspaceClient *client);
static void navigation_finish(WorkspaceClient *client);
static bool navigation_moving(WorkspaceClient *client);
#include "ui_focus_draw.h"
#include "management_ui.h"
#include <ctype.h>
static void pane_preview_refresh(WorkspaceClient *client, WorkbenchPane *pane);
static void pane_preview_draw(WorkspaceClient *client, WorkbenchPane *pane, Rectangle target);
static void window_view_restore_overview(WorkspaceClient *client);
#include "overview_ui.h"
#include "theme_ui.h"
static const char *command_unavailable(WorkspaceClient *client, int command);
static void command_dispatch(WorkspaceClient *client, int command);
static void command_open(WorkspaceClient *client);
static void shortcut_help_open(WorkspaceClient *client);
#include "shortcut_client.h"
#include "command_palette.h"
#include "shortcut_help_ui.h"

static bool workspace_layout(WorkspaceClient *client)
{
    /* Hidden workspaces retain their geometry until entry. */
    WorkbenchWorkspace *workspace = &client->workspaces[client->active];
    if (!workspace->count) return true;
    workspace_sync_view(workspace);
    for (int p=0;p<workspace->count;p++)
        workspace->panes[p]->bounds=workspace_pane_bounds(workspace,p);
    for (int p=0;p<workspace->count;p++) {
        if (!workspace_visible(workspace,p)) continue;
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
        int width=pixel_extent((int)pane->bounds.width,client->raster_scale),canvas_height=pixel_extent((int)pane->bounds.height,client->raster_scale);
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

#include "chrome_ui.h"

#include "minimal_chrome.h"

static void workspace_chrome(WorkspaceClient *client) { minimal_chrome_draw(client); }

/* Render at presentation geometry without changing pane bounds or PTY size. */
static void pane_render_canvas(WorkspaceClient *client, WorkbenchPane *pane, RenderTexture2D canvas, Rectangle bounds)
{
    pane_bind(pane);
    client_surface=bounds;
    ghostty_render_state_update(pane->render, pane->terminal);
    GhosttyRenderStateColors colors = GHOSTTY_INIT_SIZED(GhosttyRenderStateColors);
    ghostty_render_state_colors_get(pane->render, &colors);
    GhosttyTerminalScrollbar scrollbar = {0};
    ghostty_terminal_get(pane->terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &scrollbar);
    BeginTextureMode(canvas);
    BeginMode2D((Camera2D){.zoom=client->raster_scale});
    ClearBackground((Color){colors.background.r, colors.background.g, colors.background.b, 255});
    render_terminal(pane->render, pane->rows, pane->cells, client->font,
                    client->cell_width, client->cell_height, 16, 4,
                    &scrollbar, pane->terminal, pane->images);
    if (pane->closed) {
        /* Retain the frozen terminal screen, but keep connection feedback out
         * of its text. The opaque footer belongs to client chrome. */
        Rectangle footer={0,fmaxf(32,bounds.height-34),bounds.width,34};
        DrawRectangleRec(footer,ui.surface);
        BeginScissorMode((int)(8*client->raster_scale),(int)(footer.y*client->raster_scale),(int)((bounds.width-16)*client->raster_scale),(int)(34*client->raster_scale));
        const char *hint=client->managed && pane->runtime_state==RUNTIME_STOPPED
            ? "Shell stopped. Start a new shell to continue; prior jobs do not resume."
            : client->managed && pane->runtime_state==RUNTIME_UNAVAILABLE
            ? "Runtime status unavailable. Check again before recovering."
            : client->managed && pane->runtime_state==RUNTIME_CHECKING
            ? "Checking runtime status..."
            : pane->transport.attached_elsewhere
            ? (pane->transport.takeover_unsupported
                ? "Earlier runtime: close the owning window, then Reconnect."
                : "Move here to continue in the live terminal.")
            : "Connection closed. Move here to try reconnecting.";
        DrawTextEx(client->ui_font,hint,(Vector2){10,footer.y+10},UI_TEXT_CAPTION,0,ui.muted);
        EndScissorMode();
    }
    pane_header(client,pane,bounds);
    EndMode2D();
    EndTextureMode();
    pane_unbind();
}

#include "terminal_preview.h"
#include "navigation_ui.h"
#include "window_state_client.h"
#include "display_client.h"

/* One metadata check per window, four times/second; no pane-scaled I/O.
 * The atomic catalog projection is the shared organization authority for the
 * spike. Focus, viewport and animation remain local to this window. */
static void workspace_refresh(WorkspaceClient *client)
{
    double now=GetTime();
    if (now-client->catalog_checked<.25) return;
    client->catalog_checked=now;
    if (client->management_busy || client->dialog_open || client->theme_open || client->command_open || client->help_open
        || client->overview_drag_armed || client->overview_closing || client->divider_dragging
        || navigation_moving(client) || overview_animation_busy(&client->overview_animation,now)) return;
    for (int w=0;w<client->count;w++) for (int p=0;p<client->workspaces[w].count;p++)
        if (client->workspaces[w].panes[p]->selection.dragging
            || client->workspaces[w].panes[p]->scrollbar_dragging) return;
    struct stat stamp;
    if (stat(client->catalog_path,&stamp)<0) return;
    if (stamp.st_dev==client->catalog_stamp.st_dev && stamp.st_ino==client->catalog_stamp.st_ino
        && stamp.st_size==client->catalog_stamp.st_size) return;
    char workspace_ids[WORKBENCH_MAX_WORKSPACES][41], selected_id[41]={0};
    int previous_count=client->count, columns[WORKBENCH_MAX_WORKSPACES];
    OverviewScroll horizontal[WORKBENCH_MAX_WORKSPACES];
    for (int w=0;w<previous_count;w++) {
        strcpy(workspace_ids[w],client->workspaces[w].id);
        columns[w]=client->overview_columns[w];
        horizontal[w]=client->overview_horizontal[w];
    }
    OverviewItem items[WORKBENCH_MAX_WORKSPACES*WORKBENCH_PANES];
    if (client->overview_open) {
        int count=overview_items(client,items), index=client->overview_selected;
        if (index>=0 && index<count)
            strcpy(selected_id,client->workspaces[items[index].workspace].panes[items[index].pane]->id);
    }
    navigation_finish(client);
    client->refreshing=true;
    bool reloaded=workspace_reload(client);
    client->refreshing=false;
    if (!reloaded) return;
    client->catalog_stamp=stamp;
    client->navigation_reset=true;
    client->appearance_dirty=true;
    memset(client->overview_columns,0,sizeof(client->overview_columns));
    memset(client->overview_horizontal,0,sizeof(client->overview_horizontal));
    for (int w=0;w<client->count;w++) for (int old=0;old<previous_count;old++)
        if (!strcmp(client->workspaces[w].id,workspace_ids[old])) {
            client->overview_columns[w]=columns[old];
            client->overview_horizontal[w]=horizontal[old];
        }
    if (client->overview_open) {
        int count=overview_items(client,items);
        for (int i=0;i<count;i++)
            if (!strcmp(client->workspaces[items[i].workspace].panes[items[i].pane]->id,selected_id))
                client->overview_selected=i;
        if (client->overview_selected>=count) client->overview_selected=count ? count-1 : 0;
        overview_scroll_update(client,items,count,false);
    }
}

static int workspace_main(const char *catalog)
{
    WorkspaceClient *client = calloc(1, sizeof(*client));
    if (!client) return 1;
    /* Keep the inherited slot lease in this viewer, not management children. */
    const char *slot_descriptor=getenv("WORKBENCH_WINDOW_SLOT_FD");
    if (slot_descriptor) {
        char *end=NULL; long descriptor=strtol(slot_descriptor,&end,10);
        if (*slot_descriptor && end && !*end && descriptor>=3 && descriptor<=INT_MAX)
            (void)fcntl((int)descriptor,F_SETFD,FD_CLOEXEC);
        unsetenv("WORKBENCH_WINDOW_SLOT_FD");
    }
    client->catalog_path = catalog;
    /* Observe before loading so a concurrent replacement is checked again. */
    (void)stat(catalog,&client->catalog_stamp);
    if (!workspace_catalog(client, catalog)) { fprintf(stderr, "Could not load workspace catalog: %s\n", catalog); free(client); return 1; }
    client->managed = getenv("WORKBENCH_CATALOG_SCRIPT") != NULL;
    workbench_clipboard_keys=false;workbench_window_buttons=false;
    client->shortcuts=shortcut_defaults(shortcut_platform_apple());
    client->shortcut_path=getenv("WORKBENCH_SHORTCUTS");shortcut_load(client);
    client->management_fd = -1;
    client->management_pid = -1;

    const char *reduced_motion = getenv("WORKBENCH_REDUCED_MOTION");
    client->overview_animation_enabled = !reduced_motion || strcmp(reduced_motion, "1");
    client->overview_jump_workspace = client->overview_jump_pane = -1;
    client->window_state_path=getenv("WORKBENCH_WINDOW_STATE");
    client->view_loaded=client->window_state_path && window_state_read(&client->saved_view,client->window_state_path);
    client->view_has_overview=client->view_loaded && client->saved_view.context;
    client->view_restore_overview=client->view_has_overview;
    if (client->view_loaded) {
        client->chrome_pinned=client->saved_view.chrome_pinned;client->chrome_drawer=client->chrome_pinned;
        client->chrome_agents=client->saved_view.chrome_agents;client->chrome_waiting=client->saved_view.chrome_waiting;
        chrome_sidebar_width=client->chrome_pinned ? 288 : 44;
    }
    int exit_code = 0;
    if (!theme_initialize(client)) { free(client); return 1; }
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    InitWindow(client->view_loaded ? client->saved_view.width : 1240, client->view_loaded ? client->saved_view.height : 760, "Terminal Workbench — workspace spike");
    if (!IsWindowReady()) { fprintf(stderr, "Workbench window initialization failed\n"); free(client); return 1; }
    SetExitKey(KEY_NULL);
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetWindowMinSize(800, 300);
    SetTargetFPS(60);
    display_fit(client,true);
    if (client->view_loaded) window_view_restore_layout(client);
    client->raster_scale=display_current_scale(client);
    if (!client->raster_scale) client->raster_scale=1;
    /* Diagnostic override exercises doubled backing textures on a 1x Xvfb display. */
    const char *raster_override=getenv("WORKBENCH_RENDER_SCALE");
    if (raster_override) {
        char *end=NULL;float scale=strtof(raster_override,&end);
        if (end && !*end && isfinite(scale) && scale>=1 && scale<=4) client->raster_scale=client->raster_override=scale;
    }
    int font_pixels=(int)(16*client->raster_scale);
    client->font = terminal_font_load(font_pixels);
    SetTextureFilter(client->font.texture, TEXTURE_FILTER_BILINEAR);
    Vector2 glyph = MeasureTextEx(client->font, "M", font_pixels, 0);
    client->cell_width = (int)(glyph.x / client->raster_scale);
    client->cell_height = (int)(glyph.y / client->raster_scale);
    if (client->cell_width < 1) client->cell_width = 1;
    if (client->cell_height < 1) client->cell_height = 1;
    client->ui_font=ui_font_load(client->raster_scale,client->font);
    ghostty_sys_set(GHOSTTY_SYS_OPT_DECODE_PNG, (const void *)decode_png);
    for (int w = 0; w < client->count; w++)
        for (int p = 0; p < client->workspaces[w].count; p++)
            if (!pane_init(client, client->workspaces[w].panes[p])) {
                fprintf(stderr, "Could not initialize terminal %s (socket: %s)\n",
                    client->workspaces[w].panes[p]->id, client->workspaces[w].panes[p]->socket_path);
                exit_code = 1; goto cleanup;
            }

    if (client->view_loaded && client->saved_view.overview) overview_open(client);

    while (!WindowShouldClose()) {
        display_tick(client);
        workspace_refresh(client);
        overview_motion_tick(client);
        if (client->help_open || client->command_open || client->theme_open || client->dialog_open || client->overview_open || IsWindowResized()) navigation_finish(client);
        theme_sync(client);

        bool shortcut_bypass=false;
        chrome_pane_count=client->workspaces[client->active].count;
        bool chrome_frame=chrome_input(client);
        if (client->chrome_quit) break;
        bool shortcut_frame=chrome_frame ? false : shortcut_route(client,&shortcut_bypass);
        bool help_frame=client->help_open;
        bool command_started=shortcut_frame && client->command_open;
        if (!chrome_frame && !shortcut_frame && !help_frame && !client->command_open && !client->theme_open && !client->dialog_open && !client->overview_open && !client->management_busy && command_requested()) {
            command_open(client); command_started=true;
        }
        bool command_frame=client->command_open;
        bool theme_frame=client->theme_open;
        if (!chrome_frame && !shortcut_frame && !help_frame && !command_frame && !client->theme_open && !client->dialog_open && !client->management_busy && !client->overview_drag_armed
            && !client->overview_closing && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
            && CheckCollisionPointRec(GetMousePosition(),client->overview_open ? overview_theme_button(client) : theme_button())) {
            theme_open(client); theme_frame=true;
        }
        bool toggled_overview = !chrome_frame && !shortcut_frame && !help_frame && !command_frame && !client->theme_open && !client->dialog_open && !client->overview_drag_armed && !client->management_busy && overview_requested();
        if (toggled_overview) {
            if (client->overview_open && !client->overview_closing) overview_close(client, -1, -1);
            else overview_open(client);
        }
        bool modal_frame = chrome_frame || help_frame || shortcut_frame || command_frame || theme_frame || client->dialog_open || client->overview_open || toggled_overview;
        if (!management_poll(client)) { exit_code = 1; break; }
        if (command_frame) {
            if (command_started) {
                while (GetKeyPressed()!=0) { }
                while (GetCharPressed()!=0) { }
            } else if (!chrome_frame) command_input(client);
        }
        Vector2 pointer = GetMousePosition();
        bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        WorkbenchWorkspace *workspace = &client->workspaces[client->active];
        WorkbenchPane *old_focus = workspace->panes[workspace->focused];
        bool gesture_active = old_focus && (old_focus->selection.dragging || old_focus->scrollbar_dragging);
        Vector2 body_wheel=GetMouseWheelMoveV();
        bool swipe_frame=!modal_frame && !gesture_active && !client->divider_dragging && !client->management_busy
            && CheckCollisionPointRec(pointer,navigation_area()) && pane_navigation_horizontal(body_wheel.x,body_wheel.y);
        if (swipe_frame) navigation_pan(client,-body_wheel.x);
        bool navigation_hover = CheckCollisionPointRec(pointer, terminal_navigation_strip());
        if (!modal_frame && !gesture_active && !client->divider_dragging && !client->management_busy && navigation_hover) {
            Vector2 wheel=GetMouseWheelMoveV();
            float delta=wheel.x!=0 ? -wheel.x : -wheel.y;
            int step=pane_navigation_wheel(&client->navigation_wheel,delta,GetTime());
            if (step) workspace_focus(workspace,workspace->focused+step);
        } else client->navigation_wheel=(PaneNavigationWheel){0};
        if (navigation_moving(client) && CheckCollisionPointRec(pointer,navigation_area())
            && (pressed || IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)
                || IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) || (!swipe_frame && (GetMouseWheelMoveV().x || GetMouseWheelMoveV().y)))) navigation_finish(client);
        if (pressed && !gesture_active && !modal_frame && !shortcut_bypass) {
            workspace = &client->workspaces[client->active];
            for (int p = 0; p < workspace->count; p++)
                if (workspace_visible(workspace,p) && pointer.x>=WORKBENCH_SIDEBAR+WORKBENCH_GAP
                    && pointer.x<GetScreenWidth()-WORKBENCH_GAP)
                if (CheckCollisionPointRec(pointer, workspace->panes[p]->bounds)) workspace_focus(workspace,p);
            if (management_click(client, pointer)) modal_frame=true;
            if (client->dialog_open) modal_frame = true;
        }
        if (pressed && !gesture_active && !modal_frame && !shortcut_bypass) {
            for (int p=0;p<workspace->count;p++) {
                Rectangle b=workspace_pane_bounds(workspace,p);
                Rectangle edge={b.x+b.width,50,WORKBENCH_GAP,b.height};
                if (workspace_visible(workspace,p) && CheckCollisionPointRec(pointer,edge)) {
                    navigation_finish(client); client->divider_dragging=true;client->divider_pane=p;
                    client->divider_preset=workspace->panes[p]->width.preset;
                    client->divider_preview=b.x+b.width;
                    break;
                }
            }
        }
        bool divider_consumed=client->divider_dragging;
        if (client->divider_dragging) {
            int p=client->divider_pane;
            Rectangle b=workspace_pane_bounds(workspace,p);
            client->divider_preset=pane_width_snap((pointer.x-b.x+WORKBENCH_GAP)/(workspace_estate()+WORKBENCH_GAP));
            PaneWidth widths[WORKBENCH_PANES]; workspace_widths(workspace,widths);
            widths[p]=(PaneWidth){.preset=client->divider_preset};
            client->divider_preview=b.x+pane_width_pixels(widths,p,workspace_estate(),WORKBENCH_GAP);
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                workspace->panes[p]->width=(PaneWidth){.preset=client->divider_preset};
                workspace_focus(workspace,workspace->focused);client->navigation_reset=true;client->divider_dragging=false;
            }
        }
        theme_sync(client);
        navigation_prepare(client);
        bool layout_deferred = client->help_open || help_frame || client->theme_open || (client->overview_open && !client->overview_list);
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
                    if (pty_read(pane->fd, &pane->terminal, &pane->effects) != PTY_READ_OK) {
                        pane->closed=true; pane->status_checked=false; pane->runtime_state=0;
                    }
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
            bool clipboard_key = (modal_frame || shortcut_bypass) ? false : handle_clipboard(focused->fd, focused->terminal, paste_ready);
            client_surface_active = true;
            if (!modal_frame && paste_ready && !clipboard_key && IsWindowFocused()) {
                prepare_terminal_keyboard_input(focused->terminal);
                handle_input(focused->fd, focused->keys, focused->key, focused->terminal);
            }
            bool in_pane = CheckCollisionPointRec(pointer, focused->bounds);
            if (!modal_frame && !swipe_frame && !divider_consumed && (in_pane || focused->selection.dragging || focused->scrollbar_dragging)) {
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
        if (!chrome_frame && !help_frame && !shortcut_frame && !command_frame && client->dialog_open) dialog_input(client);
        bool overview_jumped = false;
        if (chrome_frame || shortcut_frame || command_frame) { /* The entire action-dispatch frame stays modal. */ }
        else if (help_frame) shortcut_help_input(client);
        else if (theme_frame) theme_input(client);
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
        if (((overview_jumped && !client->overview_closing) || (layout_deferred && !help_frame && !client->help_open && !client->theme_open && (!client->overview_open || client->overview_list))) && !workspace_layout(client)) { exit_code = 1; break; }
        theme_sync(client);
        if (overview_jumped) { navigation_finish(client); client->navigation_valid=false; }
        if (client->overview_open && !client->overview_list) overview_previews_refresh(client);
        if (client->help_open) shortcut_help_prepare(client);
        chrome_pane_count=workspace->count;
        BeginDrawing();
        ClearBackground(ui.background);
        if (!client->overview_open || client->overview_list) {
            navigation_present(client,workspace,workspace->first);
            if (client->divider_dragging) {
                DrawRectangle((int)client->divider_preview,50,2,GetScreenHeight()-60,ui.accent);
                const char *label=client->divider_preset==1 ? "2/3" : client->divider_preset==2 ? "1/3" : "1/2";
                DrawText(label,(int)client->divider_preview-32,48,18,ui.accent);
            }
        }
        if (!workspace->count) DrawTextEx(client->ui_font, "Create a terminal to start working", (Vector2){WORKBENCH_SIDEBAR+36, 118},UI_TEXT_LEAD, 0, ui.muted);
        workspace_chrome(client);
        if (client->dialog_open) dialog_draw(client);
        if (client->overview_open) overview_present(client);
        if (client->theme_open) theme_draw(client);
        if (client->command_open) command_draw(client);
        if (client->help_open) shortcut_help_draw(client);
        EndDrawing();
        flush_deferred_textures();
        window_view_save(client,false);
        management_check_status(client);
    }
    window_view_save(client,true);
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
    if (client->help_canvas.id) UnloadRenderTexture(client->help_canvas);
    for (int p=0;p<WORKBENCH_PANES;p++)
        if (client->navigation_tiles[p].canvas.id) UnloadRenderTexture(client->navigation_tiles[p].canvas);
    ui_font_unload(client->ui_font,client->font);
    terminal_font_unload(client->font);
    CloseWindow();
    free(client);
    return exit_code;
}
