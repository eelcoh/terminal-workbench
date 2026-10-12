/* Client dialogs call a neutral local management CLI. No shell interpolation. */
#include <spawn.h>
extern char **environ;

enum { DIALOG_WORKSPACE_CREATE = 1, DIALOG_WORKSPACE_RENAME,
       DIALOG_TERMINAL_CREATE, DIALOG_TERMINAL_RENAME, DIALOG_TERMINAL_CLOSE, DIALOG_WORKSPACE_CLOSE, DIALOG_TERMINAL_MOVE, DIALOG_TERMINAL_RECOVER, DIALOG_TERMINAL_STATUS, DIALOG_WORKSPACE_DIRECTORY };
enum { RUNTIME_CHECKING, RUNTIME_RUNNING, RUNTIME_STOPPED, RUNTIME_UNAVAILABLE };

static void dialog_open(WorkspaceClient *client, int kind)
{
    WorkbenchWorkspace *workspace = &client->workspaces[client->active];
    client->dialog_open = true;
    client->dialog_kind = kind;
    client->dialog_error[0] = '\0';
    client->dialog_all = true;
    client->edit_folder=kind==DIALOG_WORKSPACE_DIRECTORY;
    const char *folder=workspace->directory[0] ? workspace->directory : getenv("WORKBENCH_PROJECT");
    if (kind==DIALOG_WORKSPACE_CREATE) folder=getenv("WORKBENCH_PROJECT");
    snprintf(client->edit_directory,sizeof(client->edit_directory),"%s",folder ? folder : "");
    client->management_target[0]=0;
    if (kind==DIALOG_WORKSPACE_DIRECTORY) strcpy(client->management_target,workspace->id);
    if (kind == DIALOG_TERMINAL_RECOVER) strcpy(client->management_target,workspace->panes[workspace->focused]->id);
    if (kind == DIALOG_WORKSPACE_CLOSE) {
        strcpy(client->edit_name, workspace->name);
    } else if (kind == DIALOG_TERMINAL_CLOSE || kind == DIALOG_TERMINAL_RECOVER) {
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
    if (client->dialog_kind == DIALOG_TERMINAL_STATUS) {
        char identity[41]={0}, state[24]={0};
        bool valid=result>0 && WIFEXITED(status) && WEXITSTATUS(status)==0
            && sscanf(client->management_reply,"STATE\t%40[^\t]\t%23[^\r\n]",identity,state)==2
            && !strcmp(identity,client->management_target);
        for (int w=0;w<client->count;w++) for (int p=0;p<client->workspaces[w].count;p++) {
            WorkbenchPane *pane=client->workspaces[w].panes[p];
            if (strcmp(pane->id,client->management_target) || !pane->closed) continue;
            pane->runtime_state=valid && !strcmp(state,"stopped") ? RUNTIME_STOPPED
                : valid && !strcmp(state,"running") ? RUNTIME_RUNNING : RUNTIME_UNAVAILABLE;
            pane->status_checked=true;
            if (pane->runtime_state==RUNTIME_STOPPED) pane->transport.attached_elsewhere=false;
        }
        client->navigation_reset=true;
        return true;
    }
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
        workspace_focus(workspace,workspace->focused);
        client->dialog_open = false;
        if (client->dialog_kind == DIALOG_TERMINAL_MOVE) overview_move_saved(client);
        client->appearance_dirty=true; client->navigation_reset=true;
        if (client->dialog_kind == DIALOG_TERMINAL_RECOVER) {
            bool attached=false;
            for (int w=0;w<client->count;w++) for (int p=0;p<client->workspaces[w].count;p++) {
                WorkbenchPane *pane=client->workspaces[w].panes[p];
                if (!strcmp(pane->id,client->management_target)) attached=pane_attach(client,pane,false);
            }
            clipboard_notice(attached ? "New shell started" : "Shell started; reconnect to continue");
        } else clipboard_notice("Saved");
    } else {
        const char *message = strstr(client->management_reply, "ERROR\t");
        message = message ? message + 6 : "Could not save; session metadata was preserved";
        snprintf(client->dialog_error, sizeof(client->dialog_error), "%.*s",
                 (int)strcspn(message, "\r\n"), message);
        if (client->dialog_kind == DIALOG_TERMINAL_RECOVER) {
            fprintf(stderr,"Terminal recovery failed: %s\n",client->dialog_error);
            for (int w=0;w<client->count;w++) for (int p=0;p<client->workspaces[w].count;p++)
                if (!strcmp(client->workspaces[w].panes[p]->id,client->management_target))
                    client->workspaces[w].panes[p]->status_checked=false;
        }
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
    char name_argument[72], position_argument[24], directory_argument[1040];
    snprintf(directory_argument,sizeof(directory_argument),"--directory=%s",client->edit_directory);
    snprintf(name_argument, sizeof(name_argument), "--name=%s", title);
    snprintf(position_argument, sizeof(position_argument), "%d", position);
    char *arguments[] = {"python3", (char *)script, "--state-dir", (char *)directory,
        "--runtime", (char *)runtime, "--cwd", (char *)cwd,
        "--command", (char *)method, "--id", (char *)identity,
        name_argument, directory_argument, "--to", (char *)destination, "--position", position_argument, NULL};
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
    if (client->dialog_kind == DIALOG_TERMINAL_RECOVER) {
        management_execute(client,"terminal.recover",client->management_target,"","",-1);
        return;
    }
    if (client->dialog_kind==DIALOG_WORKSPACE_DIRECTORY) {
        management_execute(client,"workspace.directory",client->management_target,"","",-1);
        return;
    }
    const char *methods[] = {"", "workspace.create_at", "workspace.rename", "terminal.create", "terminal.rename", "terminal.close", "workspace.close"};
    const char *identity = (client->dialog_kind == DIALOG_TERMINAL_RENAME || client->dialog_kind == DIALOG_TERMINAL_CLOSE)
        ? workspace->panes[workspace->focused]->id : workspace->id;
    management_execute(client, methods[client->dialog_kind], identity, client->edit_name, "", -1);
}
static void management_check_status(WorkspaceClient *client)
{
    if (!client->managed || client->management_busy || client->dialog_open || client->theme_open || client->command_open || client->overview_open) return;
    WorkbenchWorkspace *workspace=&client->workspaces[client->active];
    for (int p=0;p<workspace->count;p++) {
        if (!workspace_visible(workspace,p)) continue;
        WorkbenchPane *pane=workspace->panes[p];
        if (!pane->closed || pane->status_checked) continue;
        client->dialog_kind=DIALOG_TERMINAL_STATUS;
        strcpy(client->management_target,pane->id);
        pane->status_checked=true; /* One bounded request per disconnection; retry is explicit. */
        management_execute(client,"terminal.status",pane->id,"","",-1);
        if (!client->management_busy) pane->runtime_state=RUNTIME_UNAVAILABLE;
        return;
    }
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
    if (index==3) return (Rectangle){118,GetScreenHeight()-134,58,30};
    return (Rectangle){8, GetScreenHeight() - 172 + index * 38, index==1 ? 104 : WORKBENCH_SIDEBAR - 16, 30};
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
    return (Rectangle){bounds.x+bounds.width-(bounds.width<300 ? 94 : 224),bounds.y+6,86,20};
}
static Rectangle terminal_move_here(WorkbenchPane *pane)
{
    if (pane->runtime_state==RUNTIME_STOPPED)
        return (Rectangle){pane->bounds.x+pane->bounds.width-(pane->bounds.width<300 ? 134 : 264),pane->bounds.y+6,126,20};
    return terminal_move_here_bounds(pane->bounds);
}
static Rectangle workspace_stop(void) { return (Rectangle){150, 51, 26, 22}; }

static void management_controls(WorkspaceClient *client)
{
    const char *labels[] = {"New workspace", "Rename", "New terminal", "Folder"};
    for (int i = 0; i < 4; i++) {
        Rectangle button = management_button(i);
        bool enabled = client->managed && (i != 0 || client->count < WORKBENCH_MAX_WORKSPACES)
            && (i == 0 || client->count > 0)
            && (i != 2 || client->workspaces[client->active].count < WORKBENCH_PANES);
        char limit_label[48];
        snprintf(limit_label,sizeof(limit_label),"Workspace limit (%d)",WORKBENCH_MAX_WORKSPACES);
        const char *label=i==0 && client->count>=WORKBENCH_MAX_WORKSPACES ? limit_label : labels[i];
        draw_header_button(client->font, button, label, enabled);
    }
    if (client->managed && client->count>=WORKBENCH_MAX_WORKSPACES &&
        !client->command_open && !client->dialog_open && !client->theme_open && !client->overview_open &&
        CheckCollisionPointRec(GetMousePosition(),management_button(0))) {
        Rectangle button=management_button(0);
        const char *hint="Close a workspace to create another";
        Vector2 size=MeasureTextEx(client->font,hint,12,0);
        Rectangle tip={button.x+button.width+8,button.y+3,size.x+16,24};
        DrawRectangleRounded(tip,.25f,6,ui.raised);
        DrawTextEx(client->font,hint,(Vector2){tip.x+8,tip.y+6},12,0,ui.text);
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
        bool selected=p==workspace->focused, visible=workspace_visible(workspace,p);
        bool hover=CheckCollisionPointRec(GetMousePosition(),position);
        DrawRectangleRounded((Rectangle){position.x+1,position.y+3,position.width-2,18},.4f,6,
            selected ? ui.selected : hover ? ui.hover : visible ? ui.raised : ui.surface);
        char number[4]; snprintf(number,sizeof(number),"%d",p+1);
        DrawTextEx(client->font,number,(Vector2){position.x+4,position.y+7},11,0,selected ? ui.accent : visible ? ui.text : ui.muted);
        if (selected) DrawRectangleRounded((Rectangle){position.x+3,28,8,2},.5f,6,ui.accent);
    }
    Vector2 pointer=GetMousePosition();
    if (!client->command_open && !client->dialog_open && !client->theme_open && !client->overview_open && CheckCollisionPointRec(pointer,terminal_navigation_strip())) {
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
    if (client->management_busy) return true;
    WorkbenchWorkspace *workspace = &client->workspaces[client->active];
    for (int p=0;p<workspace->count;p++) {
        if (!workspace_visible(workspace,p) || pointer.x<WORKBENCH_SIDEBAR+WORKBENCH_GAP || pointer.x>=GetScreenWidth()-WORKBENCH_GAP) continue;
        WorkbenchPane *pane=workspace->panes[p];
        if (pane->closed && CheckCollisionPointRec(pointer,terminal_move_here(pane))) {
            workspace->focused=p;
            if (client->managed && pane->runtime_state==RUNTIME_STOPPED) dialog_open(client,DIALOG_TERMINAL_RECOVER);
            else if (client->managed && pane->runtime_state==RUNTIME_UNAVAILABLE) {
                pane->status_checked=false; pane->runtime_state=RUNTIME_CHECKING;
            } else if (pane->runtime_state!=RUNTIME_CHECKING || !client->managed) {
                if (!pane_move_here(client,pane)) clipboard_notice("Could not connect; checking runtime status");
            }
            client->navigation_reset=true;
            return true;
        }
    }
    if (client->managed) {
        if (CheckCollisionPointRec(pointer, management_button(0))) {
            if (client->count < WORKBENCH_MAX_WORKSPACES) dialog_open(client,DIALOG_WORKSPACE_CREATE);
            else clipboard_notice("Workspace limit reached; close a workspace to create another");
            return true;
        }
        else if (CheckCollisionPointRec(pointer, management_button(1)) && client->count) dialog_open(client, DIALOG_WORKSPACE_RENAME);
        else if (CheckCollisionPointRec(pointer, management_button(2)) && client->count && workspace->count < WORKBENCH_PANES)
            dialog_open(client, DIALOG_TERMINAL_CREATE);
        else if (client->count && CheckCollisionPointRec(pointer, management_button(3))) dialog_open(client,DIALOG_WORKSPACE_DIRECTORY);
        else if (client->count && CheckCollisionPointRec(pointer, workspace_stop())) dialog_open(client, DIALOG_WORKSPACE_CLOSE);
        else
            for (int p = 0; p < workspace->count; p++)
                if (workspace_visible(workspace,p) && workspace->panes[p]->bounds.width>=300
                    && pointer.x>=WORKBENCH_SIDEBAR+WORKBENCH_GAP && pointer.x<GetScreenWidth()-WORKBENCH_GAP)
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
    workspace_focus(workspace,workspace->focused);
    return false;
}

static Rectangle dialog_bounds(void)
{
    return (Rectangle){(GetScreenWidth() - 420) / 2.0f, (GetScreenHeight() - 210) / 2.0f, 420, 210};
}

static void dialog_input(WorkspaceClient *client)
{
    if (client->dialog_kind >= DIALOG_TERMINAL_CLOSE && client->dialog_kind!=DIALOG_WORKSPACE_DIRECTORY) {
        bool escape = false;
        int key;
        while ((key = GetKeyPressed()) != 0) escape |= key == KEY_ESCAPE;
        while (GetCharPressed() != 0) { }
        if (client->management_busy) return;
        Rectangle bounds = dialog_bounds();
        bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        Vector2 pointer = GetMousePosition();
        if (escape || (click && CheckCollisionPointRec(pointer, (Rectangle){bounds.x + (client->dialog_kind==DIALOG_TERMINAL_RECOVER ? 180 : 216), bounds.y + 162, 80, 30}))) client->dialog_open = false;
        else if (click && CheckCollisionPointRec(pointer, (Rectangle){bounds.x + (client->dialog_kind==DIALOG_TERMINAL_RECOVER ? 270 : 306), bounds.y + 162, client->dialog_kind==DIALOG_TERMINAL_RECOVER ? 126 : 90, 30})) management_start(client);
        return;
    }
    bool dual=client->dialog_kind==DIALOG_WORKSPACE_CREATE;
    Rectangle bounds=dialog_bounds();
    if (!client->management_busy && dual && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        bool folder=CheckCollisionPointRec(GetMousePosition(),(Rectangle){bounds.x+24,bounds.y+116,372,28});
        bool name=CheckCollisionPointRec(GetMousePosition(),(Rectangle){bounds.x+24,bounds.y+64,372,36});
        if (folder || name) { client->edit_folder=folder; client->dialog_all=true; }
    }
    int key;
    bool enter = false, escape = false, backspace = false;
    while ((key = GetKeyPressed()) != 0) {
        if (key==KEY_TAB && dual && !client->management_busy) { client->edit_folder=!client->edit_folder; client->dialog_all=true; }
        enter |= key == KEY_ENTER;
        escape |= key == KEY_ESCAPE;
        backspace |= key == KEY_BACKSPACE;
    }
    char *edit=client->edit_folder ? client->edit_directory : client->edit_name;
    size_t capacity=client->edit_folder ? sizeof(client->edit_directory) : sizeof(client->edit_name);
    bool text_modifier = text_edit_modifier_down();
    bool paste = !client->management_busy && text_modifier && IsKeyPressed(KEY_V);
    if (paste) {
        const char *clipboard = GetClipboardText();
        if (clipboard && clipboard[0]) {
            size_t size = strnlen(clipboard, capacity);
            size_t existing = client->dialog_all ? 0 : strlen(edit);
            bool valid = size + existing < capacity;
            for (size_t i = 0; valid && i < size; i++)
                if ((unsigned char)clipboard[i] < 32 || clipboard[i] == 127) valid = false;
            if (valid) {
                memcpy(edit + existing, clipboard, size + 1);
                client->dialog_all = false;
                client->dialog_error[0] = '\0';
            } else strcpy(client->dialog_error, "Paste a single-line value that fits the field");
        }
    }
    int character;
    while ((character = GetCharPressed()) != 0) {
        if (client->management_busy || text_modifier || character < 32 || character == 127) continue;
        char bytes[4]; int count = utf8_encode((uint32_t)character, bytes);
        size_t length = client->dialog_all ? 0 : strlen(edit);
        if (length + (size_t)count < capacity) {
            memcpy(edit + length, bytes, count);
            edit[length + count] = '\0';
            client->dialog_all = false;
        }
    }
    if (client->management_busy) return;
    if (text_modifier && IsKeyPressed(KEY_A)) client->dialog_all = true;
    if (backspace || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        size_t length = strlen(edit);
        if (client->dialog_all) length = 0;
        else if (length) {
            length--;
            while (length && ((unsigned char)edit[length] & 0xc0) == 0x80) length--;
        }
        edit[length] = '\0';
        client->dialog_all = false;
    }
    Vector2 pointer = GetMousePosition();
    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Rectangle cancel = {bounds.x + 216, bounds.y + 162, 80, 30};
    Rectangle save = {bounds.x + 306, bounds.y + 162, 90, 30};
    if (escape || (click && CheckCollisionPointRec(pointer, cancel))) client->dialog_open = false;
    else if (enter || (click && CheckCollisionPointRec(pointer, save))) management_start(client);
}

static void dialog_field(WorkspaceClient *client, Rectangle field, const char *value, bool active)
{
    DrawRectangleRounded(field,.2f,8,ui.background);
    DrawRectangleRoundedLinesEx(field,.2f,8,1,active ? ui.accent : ui.border);
    Vector2 width=MeasureTextEx(client->font,value,14,0);
    float x=field.x+8-fmaxf(0,width.x-(field.width-20));
    BeginScissorMode((int)field.x+8,(int)field.y,(int)field.width-16,(int)field.height);
    bool selected=active && client->dialog_all && !client->management_busy;
    float text_y=field.y+(field.height-14)/2;
    if (selected && value[0])
        DrawRectangleRec((Rectangle){x-1,text_y-2,width.x+2,18},ui_color(current_theme.selection_background));
    DrawTextEx(client->font,value,(Vector2){x,text_y},14,0,
        selected ? ui_color(current_theme.selection_foreground) : ui.text);
    if (active && !client->dialog_all && !client->management_busy && ((int)(GetTime()*2)%2==0))
        DrawLine((int)(x+width.x+1),(int)field.y+6,(int)(x+width.x+1),(int)(field.y+field.height-6),ui.text);
    EndScissorMode();
}
static void dialog_draw(WorkspaceClient *client)
{
    Rectangle bounds = dialog_bounds();
    const char *titles[] = {"", "New workspace", "Rename workspace", "New terminal", "Rename terminal", "Stop terminal?", "Stop workspace?", "", "Start new shell?", "", "Project folder"};
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 160});
    ui_panel(bounds,ui.surface,true);
    DrawTextEx(client->font, titles[client->dialog_kind], (Vector2){bounds.x + 24, bounds.y + 22}, 16, 0, ui.text);
    if (client->dialog_kind >= DIALOG_TERMINAL_CLOSE && client->dialog_kind!=DIALOG_WORKSPACE_DIRECTORY) {
        WorkbenchWorkspace *workspace = &client->workspaces[client->active];
        char message[160];
        snprintf(message, sizeof(message), "%s", client->edit_name);
        BeginScissorMode((int)bounds.x + 24, (int)bounds.y + 58, 372, 90);
        DrawTextEx(client->font, message, (Vector2){bounds.x + 24, bounds.y + 60}, 14, 0, ui.text);
        if (client->dialog_kind==DIALOG_TERMINAL_RECOVER) {
            DrawTextEx(client->font,"Previous shell has stopped. Starts a fresh shell.",(Vector2){bounds.x+24,bounds.y+86},12,0,ui.muted);
            DrawTextEx(client->font,"Keeps its name and theme; prior jobs do not resume.",(Vector2){bounds.x+24,bounds.y+104},12,0,ui.muted);
            DrawTextEx(client->font,client->management_busy ? "Starting..." : client->dialog_error,
                (Vector2){bounds.x+24,bounds.y+128},12,0,ui.waiting);
            EndScissorMode();
            draw_header_button(client->font,(Rectangle){bounds.x+180,bounds.y+162,80,30},"Cancel",!client->management_busy);
            draw_header_button(client->font,(Rectangle){bounds.x+270,bounds.y+162,126,30},"Start new shell",!client->management_busy);
            return;
        }
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
    bool dual=client->dialog_kind==DIALOG_WORKSPACE_CREATE;
    bool directory=client->dialog_kind==DIALOG_WORKSPACE_DIRECTORY;
    dialog_field(client,(Rectangle){bounds.x+24,bounds.y+64,372,36},directory ? client->edit_directory : client->edit_name,
        directory || !client->edit_folder);
    if (dual) {
        DrawTextEx(client->font,client->edit_folder && client->dialog_all ? "Project folder (selected; typing replaces it)" : "Project folder",
            (Vector2){bounds.x+24,bounds.y+104},10,0,ui.muted);
        dialog_field(client,(Rectangle){bounds.x+24,bounds.y+116,372,28},client->edit_directory,client->edit_folder);
    } else if (directory)
        DrawTextEx(client->font,client->dialog_all ? "Path selected; typing replaces it." : "New and replacement shells start here.",
            (Vector2){bounds.x+24,bounds.y+104},12,0,ui.muted);
    BeginScissorMode((int)bounds.x+24,(int)bounds.y+(dual ? 146 : 110),372,dual ? 14 : 42);
    DrawTextEx(client->font,client->management_busy ? "Saving..." : client->dialog_error,
        (Vector2){bounds.x+24,bounds.y+(dual ? 146 : 128)},12,0,ui.waiting);
    EndScissorMode();
    draw_header_button(client->font, (Rectangle){bounds.x + 216, bounds.y + 162, 80, 30}, "Cancel", !client->management_busy);
    draw_header_button(client->font, (Rectangle){bounds.x + 306, bounds.y + 162, 90, 30},
        client->dialog_kind == DIALOG_WORKSPACE_CREATE || client->dialog_kind == DIALOG_TERMINAL_CREATE ? "Create" : "Save", !client->management_busy);
}
