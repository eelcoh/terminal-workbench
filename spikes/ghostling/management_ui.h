/* Client dialogs call a neutral local management CLI. No shell interpolation. */
#include <spawn.h>
extern char **environ;

enum { DIALOG_WORKSPACE_CREATE = 1, DIALOG_WORKSPACE_RENAME,
       DIALOG_TERMINAL_CREATE, DIALOG_TERMINAL_RENAME, DIALOG_TERMINAL_CLOSE, DIALOG_WORKSPACE_CLOSE, DIALOG_TERMINAL_MOVE };

static void dialog_open(WorkspaceClient *client, int kind)
{
    WorkbenchWorkspace *workspace = &client->workspaces[client->active];
    client->dialog_open = true;
    client->dialog_kind = kind;
    client->dialog_error[0] = '\0';
    client->dialog_all = true;
    if (kind == DIALOG_WORKSPACE_CLOSE) {
        strcpy(client->edit_name, workspace->name);
    } else if (kind == DIALOG_TERMINAL_CLOSE) {
        strcpy(client->edit_name, workspace->panes[workspace->focused]->name);
    } else if (kind == DIALOG_WORKSPACE_RENAME)
        strcpy(client->edit_name, workspace->name);
    else if (kind == DIALOG_TERMINAL_RENAME)
        strcpy(client->edit_name, workspace->panes[workspace->focused]->name);
    else
        snprintf(client->edit_name, sizeof(client->edit_name), "%s %d",
            kind == DIALOG_WORKSPACE_CREATE ? "Workspace" : "Shell",
            kind == DIALOG_WORKSPACE_CREATE ? client->count + 1 : workspace->count + 1);
}

static void management_read(WorkspaceClient *client)
{
    while (client->management_length < sizeof(client->management_reply) - 1) {
        ssize_t n = read(client->management_fd,
            client->management_reply + client->management_length,
            sizeof(client->management_reply) - 1 - client->management_length);
        if (n > 0) client->management_length += (size_t)n;
        else if (n < 0 && errno == EINTR) continue;
        else break;
    }
    client->management_reply[client->management_length] = '\0';
}

static bool management_poll(WorkspaceClient *client)
{
    if (!client->management_busy) return true;
    management_read(client);
    int status = 0;
    pid_t result = waitpid(client->management_pid, &status, WNOHANG);
    if (result == 0 || (result < 0 && errno == EINTR)) return true;
    management_read(client);
    close(client->management_fd);
    client->management_fd = -1;
    client->management_busy = false;
    client->management_pid = -1;
    if (result > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        struct stat stamp;
        bool observed=stat(client->catalog_path,&stamp)==0;
        char created_terminal[41]={0}, created_workspace[41]={0};
        const char *created=strstr(client->management_reply,"CREATED\t");
        if (created && sscanf(created,"CREATED\t%40[^\t]\t%40[^\r\n]",created_terminal,created_workspace)==2)
            strcpy(client->claim_new,created_terminal);
        bool reloaded=workspace_reload(client);
        client->claim_new[0]=0;
        if (!reloaded) {
            fprintf(stderr, "catalog updated, but viewer could not attach new terminals\n");
            return false;
        }
        if (observed) client->catalog_stamp=stamp;
        if (client->dialog_kind == DIALOG_WORKSPACE_CREATE) {
            client->active = client->count ? client->count - 1 : 0;
            for (int w=0;w<client->count;w++)
                if (!strcmp(client->workspaces[w].id,created_workspace)) client->active=w;
            int visible_rows = (GetScreenHeight() - 268) / 44;
            if (visible_rows < 1) visible_rows = 1;
            if (client->active >= client->sidebar_first + visible_rows) client->sidebar_first = client->active - visible_rows + 1;
        }
        WorkbenchWorkspace *workspace = &client->workspaces[client->active];
        if (client->dialog_kind == DIALOG_TERMINAL_CREATE) {
            workspace->focused=workspace->count ? workspace->count-1 : 0;
            for (int p=0;p<workspace->count;p++)
                if (!strcmp(workspace->panes[p]->id,created_terminal)) workspace->focused=p;
        }
        if (workspace->focused < workspace->first) workspace->first = workspace->focused;
        if (workspace->focused >= workspace->first + 2) workspace->first = workspace->focused - 1;
        client->dialog_open = false;
        if (client->dialog_kind == DIALOG_TERMINAL_MOVE) overview_move_saved(client);
        client->appearance_dirty=true; client->navigation_reset=true;
        clipboard_notice("Saved");
    } else {
        const char *message = strstr(client->management_reply, "ERROR\t");
        message = message ? message + 6 : "Could not save; session metadata was preserved";
        snprintf(client->dialog_error, sizeof(client->dialog_error), "%.*s",
                 (int)strcspn(message, "\r\n"), message);
        if (client->dialog_kind == DIALOG_TERMINAL_MOVE) {
            snprintf(client->overview_notice, sizeof(client->overview_notice), "%s", client->dialog_error);
            client->overview_notice_until = GetTime() + 5;
        }
    }
    return true;
}

static void management_execute(WorkspaceClient *client, const char *method, const char *identity, const char *title, const char *destination, int position)
{
    const char *script = getenv("WORKBENCH_CATALOG_SCRIPT");
    const char *directory = getenv("WORKBENCH_STATE_DIR");
    const char *runtime = getenv("WORKBENCH_RUNTIME");
    const char *cwd = getenv("WORKBENCH_PROJECT");
    if (!script || !directory || !runtime || !cwd) {
        strcpy(client->dialog_error, "Management requires the workspace launcher"); return;
    }
    char name_argument[72], position_argument[24];
    snprintf(name_argument, sizeof(name_argument), "--name=%s", title);
    snprintf(position_argument, sizeof(position_argument), "%d", position);
    char *arguments[] = {"python3", (char *)script, "--state-dir", (char *)directory,
        "--runtime", (char *)runtime, "--cwd", (char *)cwd,
        "--command", (char *)method, "--id", (char *)identity,
        name_argument, "--to", (char *)destination, "--position", position_argument, NULL};
    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) { strcpy(client->dialog_error, "Could not start save"); return; }
    posix_spawn_file_actions_t actions;
    int error = posix_spawn_file_actions_init(&actions);
    if (!error) {
        error = posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
        if (!error) error = posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDERR_FILENO);
        if (!error) error = posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
        if (!error) error = posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);
        for (int w = 0; !error && w < client->count; w++)
            for (int p = 0; !error && p < client->workspaces[w].count; p++) {
                int fd = client->workspaces[w].panes[p]->fd;
                if (fd >= 0) error = posix_spawn_file_actions_addclose(&actions, fd);
            }
        if (!error) error = posix_spawnp(&client->management_pid, "python3", &actions, NULL, arguments, environ);
        posix_spawn_file_actions_destroy(&actions);
    }
    close(pipe_fds[1]);
    if (error) {
        close(pipe_fds[0]); strcpy(client->dialog_error, "Could not start save"); return;
    }
    int flags = fcntl(pipe_fds[0], F_GETFL);
    if (flags < 0 || fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) < 0) {
        close(pipe_fds[0]);
        waitpid(client->management_pid, NULL, 0);
        client->management_pid = -1;
        strcpy(client->dialog_error, "Could not read save result; reopen to refresh"); return;
    }
    client->management_fd = pipe_fds[0];
    client->management_length = 0;
    client->management_busy = true;
    client->dialog_error[0] = '\0';
}

static void management_start(WorkspaceClient *client)
{
    WorkbenchWorkspace *workspace = &client->workspaces[client->active];
    const char *methods[] = {"", "workspace.create", "workspace.rename", "terminal.create", "terminal.rename", "terminal.close", "workspace.close"};
    const char *identity = (client->dialog_kind == DIALOG_TERMINAL_RENAME || client->dialog_kind == DIALOG_TERMINAL_CLOSE)
        ? workspace->panes[workspace->focused]->id : workspace->id;
    management_execute(client, methods[client->dialog_kind], identity, client->edit_name, "", -1);
}
static void management_move_start(WorkspaceClient *client, const char *identity, const char *destination, int position)
{
    client->dialog_kind = DIALOG_TERMINAL_MOVE;
    snprintf(client->overview_move_id, sizeof(client->overview_move_id), "%s", identity);
    management_execute(client, "terminal.move", identity, "", destination, position);
    if (!client->management_busy) {
        snprintf(client->overview_notice, sizeof(client->overview_notice), "%s", client->dialog_error);
        client->overview_notice_until = GetTime() + 5;
    }
}

static Rectangle management_button(int index)
{
    return (Rectangle){8, GetScreenHeight() - 172 + index * 38, WORKBENCH_SIDEBAR - 16, 30};
}
static Rectangle terminal_navigation(int next)
{
    return (Rectangle){194 + next * 64, 4, 58, 24};
}
static Rectangle terminal_navigation_strip(void) { return (Rectangle){194,0,274,32}; }
static Rectangle terminal_position(int index) { return (Rectangle){356+index*14,4,14,24}; }
static Rectangle terminal_rename(WorkbenchPane *pane)
{
    return (Rectangle){pane->bounds.x + pane->bounds.width - 72, pane->bounds.y + 6, 64, 20};
}

static Rectangle terminal_stop(WorkbenchPane *pane)
{
    return (Rectangle){pane->bounds.x + pane->bounds.width - 132, pane->bounds.y + 6, 54, 20};
}
/* One rectangle contract for window hit testing and canvas-local drawing. */
static Rectangle terminal_move_here_bounds(Rectangle bounds)
{
    return (Rectangle){bounds.x+bounds.width-224,bounds.y+6,86,20};
}
static Rectangle terminal_move_here(WorkbenchPane *pane)
{
    return terminal_move_here_bounds(pane->bounds);
}
static Rectangle workspace_stop(void) { return (Rectangle){150, 51, 26, 22}; }

static void management_controls(WorkspaceClient *client)
{
    const char *labels[] = {"New workspace", "Rename workspace", "New terminal"};
    for (int i = 0; i < 3; i++) {
        Rectangle button = management_button(i);
        bool enabled = client->managed && (i != 0 || client->count < WORKBENCH_MAX_WORKSPACES)
            && (i == 0 || client->count > 0)
            && (i != 2 || client->workspaces[client->active].count < WORKBENCH_PANES);
        draw_header_button(client->font, button, labels[i], enabled);
    }
    draw_header_button(client->font, workspace_stop(), "x", client->managed && client->count > 0);
    WorkbenchWorkspace *workspace = &client->workspaces[client->active];
    DrawRectangle(188, 0, 280, 32, ui.surface);
    draw_header_button(client->font, terminal_navigation(0), "Prev", workspace->focused > 0);
    draw_header_button(client->font, terminal_navigation(1), "Next", workspace->focused + 1 < workspace->count);
    char count[48];
    snprintf(count, sizeof(count), "%d/%d", workspace->count ? workspace->focused + 1 : 0, workspace->count);
    DrawTextEx(client->font, count, (Vector2){326, 9}, 12, 0, ui.muted);
    for (int p=0;p<workspace->count;p++) {
        Rectangle position=terminal_position(p);
        bool selected=p==workspace->focused, visible=p>=workspace->first && p<workspace->first+2;
        bool hover=CheckCollisionPointRec(GetMousePosition(),position);
        DrawRectangleRounded((Rectangle){position.x+1,position.y+3,position.width-2,18},.4f,6,
            selected ? ui.selected : hover ? ui.hover : visible ? ui.raised : ui.surface);
        char number[4]; snprintf(number,sizeof(number),"%d",p+1);
        DrawTextEx(client->font,number,(Vector2){position.x+4,position.y+7},11,0,selected ? ui.accent : visible ? ui.text : ui.muted);
        if (selected) DrawRectangleRounded((Rectangle){position.x+3,28,8,2},.5f,6,ui.accent);
    }
    Vector2 pointer=GetMousePosition();
    if (!client->dialog_open && !client->theme_open && !client->overview_open && CheckCollisionPointRec(pointer,terminal_navigation_strip())) {
        const char *hint="Scroll here to move between terminals";
        for (int p=0;p<workspace->count;p++) if (CheckCollisionPointRec(pointer,terminal_position(p))) hint=workspace->panes[p]->name;
        Vector2 size=MeasureTextEx(client->font,hint,12,0);
        DrawRectangleRounded((Rectangle){194,36,size.x+16,24},.25f,6,ui.raised);
        DrawTextEx(client->font,hint,(Vector2){202,42},12,0,ui.muted);
    }
}

static bool management_click(WorkspaceClient *client, Vector2 pointer)
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return false;
    WorkbenchWorkspace *workspace = &client->workspaces[client->active];
    for (int p=workspace->first;p<workspace->count && p<workspace->first+2;p++) {
        WorkbenchPane *pane=workspace->panes[p];
        if (pane->closed && CheckCollisionPointRec(pointer,terminal_move_here(pane))) {
            workspace->focused=p;
            if (!pane_move_here(client,pane)) clipboard_notice("Could not connect; reopen to recover the session");
            client->navigation_reset=true;
            return true;
        }
    }
    if (client->managed) {
        if (CheckCollisionPointRec(pointer, management_button(0)) && client->count < WORKBENCH_MAX_WORKSPACES)
            dialog_open(client, DIALOG_WORKSPACE_CREATE);
        else if (CheckCollisionPointRec(pointer, management_button(1)) && client->count) dialog_open(client, DIALOG_WORKSPACE_RENAME);
        else if (CheckCollisionPointRec(pointer, management_button(2)) && client->count && workspace->count < WORKBENCH_PANES)
            dialog_open(client, DIALOG_TERMINAL_CREATE);
        else if (client->count && CheckCollisionPointRec(pointer, workspace_stop())) dialog_open(client, DIALOG_WORKSPACE_CLOSE);
        else
            for (int p = workspace->first; p < workspace->count && p < workspace->first + 2; p++)
                if (CheckCollisionPointRec(pointer, terminal_rename(workspace->panes[p]))) {
                    workspace->focused = p;
                    dialog_open(client, DIALOG_TERMINAL_RENAME);
                } else if (CheckCollisionPointRec(pointer, terminal_stop(workspace->panes[p]))) {
                    workspace->focused = p;
                    dialog_open(client, DIALOG_TERMINAL_CLOSE);
                }
    }
    if (CheckCollisionPointRec(pointer, terminal_navigation(0)) && workspace->focused > 0) workspace->focused--;
    if (CheckCollisionPointRec(pointer, terminal_navigation(1)) && workspace->focused + 1 < workspace->count) workspace->focused++;
    for (int p=0;p<workspace->count;p++)
        if (CheckCollisionPointRec(pointer,terminal_position(p))) workspace->focused=p;
    pane_navigation_focus(workspace->count,workspace->focused,&workspace->focused,&workspace->first);
    return false;
}

static Rectangle dialog_bounds(void)
{
    return (Rectangle){(GetScreenWidth() - 420) / 2.0f, (GetScreenHeight() - 210) / 2.0f, 420, 210};
}

static void dialog_input(WorkspaceClient *client)
{
    if (client->dialog_kind >= DIALOG_TERMINAL_CLOSE) {
        bool escape = false;
        int key;
        while ((key = GetKeyPressed()) != 0) escape |= key == KEY_ESCAPE;
        while (GetCharPressed() != 0) { }
        if (client->management_busy) return;
        Rectangle bounds = dialog_bounds();
        bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        Vector2 pointer = GetMousePosition();
        if (escape || (click && CheckCollisionPointRec(pointer, (Rectangle){bounds.x + 216, bounds.y + 162, 80, 30}))) client->dialog_open = false;
        else if (click && CheckCollisionPointRec(pointer, (Rectangle){bounds.x + 306, bounds.y + 162, 90, 30})) management_start(client);
        return;
    }
    int key;
    bool enter = false, escape = false, backspace = false;
    while ((key = GetKeyPressed()) != 0) {
        enter |= key == KEY_ENTER;
        escape |= key == KEY_ESCAPE;
        backspace |= key == KEY_BACKSPACE;
    }
    bool text_modifier = text_edit_modifier_down();
    bool paste = !client->management_busy && text_modifier && IsKeyPressed(KEY_V);
    if (paste) {
        const char *clipboard = GetClipboardText();
        if (clipboard && clipboard[0]) {
            size_t size = strnlen(clipboard, 64);
            size_t existing = client->dialog_all ? 0 : strlen(client->edit_name);
            bool valid = size + existing < sizeof(client->edit_name);
            for (size_t i = 0; valid && i < size; i++)
                if ((unsigned char)clipboard[i] < 32 || clipboard[i] == 127) valid = false;
            if (valid) {
                memcpy(client->edit_name + existing, clipboard, size + 1);
                client->dialog_all = false;
                client->dialog_error[0] = '\0';
            } else strcpy(client->dialog_error, "Paste a single-line name of at most 63 bytes");
        }
    }
    int character;
    while ((character = GetCharPressed()) != 0) {
        if (client->management_busy || text_modifier || character < 32 || character == 127) continue;
        char bytes[4]; int count = utf8_encode((uint32_t)character, bytes);
        size_t length = client->dialog_all ? 0 : strlen(client->edit_name);
        if (length + (size_t)count < sizeof(client->edit_name)) {
            memcpy(client->edit_name + length, bytes, count);
            client->edit_name[length + count] = '\0';
            client->dialog_all = false;
        }
    }
    if (client->management_busy) return;
    if (text_modifier && IsKeyPressed(KEY_A)) client->dialog_all = true;
    if (backspace || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        size_t length = strlen(client->edit_name);
        if (client->dialog_all) length = 0;
        else if (length) {
            length--;
            while (length && ((unsigned char)client->edit_name[length] & 0xc0) == 0x80) length--;
        }
        client->edit_name[length] = '\0';
        client->dialog_all = false;
    }
    Rectangle bounds = dialog_bounds();
    Vector2 pointer = GetMousePosition();
    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Rectangle cancel = {bounds.x + 216, bounds.y + 162, 80, 30};
    Rectangle save = {bounds.x + 306, bounds.y + 162, 90, 30};
    if (escape || (click && CheckCollisionPointRec(pointer, cancel))) client->dialog_open = false;
    else if (enter || (click && CheckCollisionPointRec(pointer, save))) management_start(client);
}

static void dialog_draw(WorkspaceClient *client)
{
    Rectangle bounds = dialog_bounds();
    const char *titles[] = {"", "New workspace", "Rename workspace", "New terminal", "Rename terminal", "Stop terminal?", "Stop workspace?"};
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 160});
    ui_panel(bounds,ui.surface,true);
    DrawTextEx(client->font, titles[client->dialog_kind], (Vector2){bounds.x + 24, bounds.y + 22}, 16, 0, ui.text);
    if (client->dialog_kind >= DIALOG_TERMINAL_CLOSE) {
        WorkbenchWorkspace *workspace = &client->workspaces[client->active];
        char message[160];
        snprintf(message, sizeof(message), "%s", client->edit_name);
        BeginScissorMode((int)bounds.x + 24, (int)bounds.y + 58, 372, 90);
        DrawTextEx(client->font, message, (Vector2){bounds.x + 24, bounds.y + 60}, 14, 0, ui.text);
        snprintf(message, sizeof(message), "Removes %d shell%s; stops foreground jobs.",
            client->dialog_kind == DIALOG_WORKSPACE_CLOSE ? workspace->count : 1,
            client->dialog_kind == DIALOG_WORKSPACE_CLOSE && workspace->count != 1 ? "s" : "");
        DrawTextEx(client->font, message, (Vector2){bounds.x + 24, bounds.y + 86}, 12, 0, ui.muted);
        DrawTextEx(client->font, client->management_busy ? "Stopping..." : client->dialog_error,
            (Vector2){bounds.x + 24, bounds.y + 116}, 12, 0, ui.waiting);
        EndScissorMode();
        draw_header_button(client->font, (Rectangle){bounds.x + 216, bounds.y + 162, 80, 30}, "Cancel", !client->management_busy);
        draw_header_button(client->font, (Rectangle){bounds.x + 306, bounds.y + 162, 90, 30}, "Stop", !client->management_busy);
        return;
    }
    Rectangle field = {bounds.x + 24, bounds.y + 64, 372, 36};
    DrawRectangleRounded(field,.2f,8,client->dialog_all ? ui.selected : ui.background);
    BeginScissorMode((int)field.x + 8, (int)field.y, (int)field.width - 16, (int)field.height);
    DrawTextEx(client->font, client->edit_name, (Vector2){field.x + 8, field.y + 10}, 14, 0, ui.text);
    if (!client->dialog_all && !client->management_busy && ((int)(GetTime() * 2) % 2 == 0)) {
        Vector2 width = MeasureTextEx(client->font, client->edit_name, 14, 0);
        DrawLine((int)(field.x + 9 + width.x), (int)field.y + 9,
                 (int)(field.x + 9 + width.x), (int)field.y + 25, ui.text);
    }
    EndScissorMode();
    BeginScissorMode((int)bounds.x + 24, (int)bounds.y + 110, 372, 42);
    DrawTextEx(client->font, client->management_busy ? "Saving..." : client->dialog_error,
        (Vector2){bounds.x + 24, bounds.y + 112}, 12, 0, ui.waiting);
    EndScissorMode();
    draw_header_button(client->font, (Rectangle){bounds.x + 216, bounds.y + 162, 80, 30}, "Cancel", !client->management_busy);
    draw_header_button(client->font, (Rectangle){bounds.x + 306, bounds.y + 162, 90, 30},
        client->dialog_kind == DIALOG_WORKSPACE_CREATE || client->dialog_kind == DIALOG_TERMINAL_CREATE ? "Create" : "Save", !client->management_busy);
}
