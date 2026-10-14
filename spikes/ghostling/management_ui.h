/* Client dialogs call a neutral local management CLI. No shell interpolation. */
/* Focus order: name/path, optional project folder, Cancel, Save/Create. Confirmations start unfocused. */
#include <spawn.h>
extern char **environ;

enum { DIALOG_WORKSPACE_CREATE = 1, DIALOG_WORKSPACE_RENAME,
       DIALOG_TERMINAL_CREATE, DIALOG_TERMINAL_RENAME, DIALOG_TERMINAL_CLOSE, DIALOG_WORKSPACE_CLOSE, DIALOG_TERMINAL_MOVE, DIALOG_TERMINAL_RECOVER, DIALOG_TERMINAL_STATUS, DIALOG_WORKSPACE_DIRECTORY };
enum { RUNTIME_CHECKING, RUNTIME_RUNNING, RUNTIME_STOPPED, RUNTIME_UNAVAILABLE };
#include "dialog_text.h"

static void dialog_open_workspace(WorkspaceClient *client,int kind,int index)
{
    WorkbenchWorkspace *workspace=&client->workspaces[index];
    client->dialog_terminal_count=workspace->count;
    client->dialog_open = true;
    client->dialog_kind = kind;
    client->dialog_focus=(UiFocus){.item=kind>=DIALOG_TERMINAL_CLOSE && kind!=DIALOG_WORKSPACE_DIRECTORY ? -1 : 0};
    client->dialog_error[0] = '\0';
    const char *folder=workspace->directory[0] ? workspace->directory : getenv("WORKBENCH_PROJECT");
    if (kind==DIALOG_WORKSPACE_CREATE) folder=getenv("WORKBENCH_PROJECT");
    snprintf(client->edit_directory,sizeof(client->edit_directory),"%s",folder ? folder : "");
    client->management_target[0]=0;
    if (kind==DIALOG_WORKSPACE_DIRECTORY) strcpy(client->management_target,workspace->id);
    if (kind == DIALOG_TERMINAL_RECOVER) strcpy(client->management_target,workspace->panes[workspace->focused]->id);
    if (kind == DIALOG_WORKSPACE_CLOSE) {
        strcpy(client->management_target,workspace->id);
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
    memset(client->edit_fields,0,sizeof(client->edit_fields));
    text_edit_init(&client->edit_fields[0].selection,client->edit_name,
        kind==DIALOG_WORKSPACE_CREATE || kind==DIALOG_TERMINAL_CREATE);
    text_edit_init(&client->edit_fields[1].selection,client->edit_directory,false);
    client->edit_fields[0].blink=client->edit_fields[1].blink=GetTime();
}

static void dialog_open(WorkspaceClient *client,int kind)
{dialog_open_workspace(client,kind,client->active);}

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
    if (client->dialog_kind==DIALOG_WORKSPACE_CLOSE) {
        management_execute(client,"workspace.close",client->management_target,"","",-1);return;
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
    if (!client->managed || client->chrome_menu || client->chrome_keyboard || client->management_busy || client->dialog_open || client->theme_open || client->command_open || client->help_open || client->overview_open) return;
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

static Rectangle terminal_navigation_strip(void) { float width=fmaxf(28,chrome_pane_count*28);return (Rectangle){GetScreenWidth()/2.0f-width/2,6,width,28}; }
static Rectangle terminal_position(int index) { Rectangle b=terminal_navigation_strip();return (Rectangle){b.x+index*28,6,28,28}; }
static Rectangle terminal_overflow(WorkbenchPane *pane)
{ return (Rectangle){pane->bounds.x+pane->bounds.width-30,pane->bounds.y+4,24,24}; }
static void chrome_open_menu(WorkspaceClient *client,int kind,Rectangle anchor);
/* One rectangle contract for window hit testing and canvas-local drawing. */
static Rectangle terminal_move_here_bounds(Rectangle bounds)
{
    return (Rectangle){bounds.x+bounds.width-124,bounds.y+6,86,20};
}
static Rectangle terminal_move_here(WorkbenchPane *pane)
{
    if (pane->runtime_state==RUNTIME_STOPPED)
        return (Rectangle){pane->bounds.x+pane->bounds.width-164,pane->bounds.y+6,126,20};
    return terminal_move_here_bounds(pane->bounds);
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
    for (int p=0;p<workspace->count;p++) {
        WorkbenchPane *pane=workspace->panes[p];
        if (workspace_visible(workspace,p) && CheckCollisionPointRec(pointer,terminal_overflow(pane))) {
            workspace_focus(workspace,p);chrome_open_menu(client,2,terminal_overflow(pane));return true;
        }
    }
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
    bool destructive=client->dialog_kind>=DIALOG_TERMINAL_CLOSE && client->dialog_kind!=DIALOG_WORKSPACE_DIRECTORY;
    ui_focus_begin(&client->dialog_focus,client->management_busy ? 0 : destructive ? 12 : client->dialog_kind==DIALOG_WORKSPACE_CREATE ? 15 : 13,12);
    if (destructive) {
        bool escape = false;
        int key;
        while ((key = GetKeyPressed()) != 0) { if (ui_focus_key(&client->dialog_focus,key)) continue; escape |= key == KEY_ESCAPE; }
        while (GetCharPressed() != 0) { }
        if (client->management_busy) return;
        Rectangle bounds = dialog_bounds();
        if (escape || ui_focus_hit(&client->dialog_focus,2,(Rectangle){bounds.x + (client->dialog_kind==DIALOG_TERMINAL_RECOVER ? 180 : 216), bounds.y + 162, 80, 30})) client->dialog_open = false;
        else if (ui_focus_hit(&client->dialog_focus,3,(Rectangle){bounds.x + (client->dialog_kind==DIALOG_TERMINAL_RECOVER ? 270 : 306), bounds.y + 162, client->dialog_kind==DIALOG_TERMINAL_RECOVER ? 126 : 90, 30})) management_start(client);
        return;
    }
    bool dual=client->dialog_kind==DIALOG_WORKSPACE_CREATE;
    Rectangle bounds=dialog_bounds();
    dialog_text_mouse(client,bounds,dual);
    int key;bool enter=false,escape=false;
    const int repeating[]={KEY_LEFT,KEY_RIGHT,KEY_HOME,KEY_END,KEY_BACKSPACE,KEY_DELETE};
    unsigned handled=0;
    while((key=GetKeyPressed())!=0) {
        if(ui_focus_key(&client->dialog_focus,key)) {
            if(client->dialog_focus.item>=0 && client->dialog_focus.item<=1)
                client->edit_fields[dialog_text_index(client)].blink=GetTime();
            continue;
        }
        enter |= key==KEY_ENTER;escape |= key==KEY_ESCAPE;
        for(size_t i=0;i<sizeof(repeating)/sizeof(repeating[0]);i++) if(key==repeating[i]) handled|=1u<<i;
        dialog_text_key(client,key);
    }
    for(size_t i=0;i<sizeof(repeating)/sizeof(repeating[0]);i++)
        if(!(handled&(1u<<i)) && IsKeyPressedRepeat(repeating[i])) dialog_text_key(client,repeating[i]);
    dialog_text_characters(client);
    bool editing=client->dialog_focus.item==0 || client->dialog_focus.item==1;
    dialog_text_scroll(client,client->dialog_kind==DIALOG_WORKSPACE_DIRECTORY ? 1 : 0,dialog_text_bounds(bounds,false));
    if(dual) dialog_text_scroll(client,1,dialog_text_bounds(bounds,true));
    if(client->management_busy) return;
    Rectangle cancel = {bounds.x + 216, bounds.y + 162, 80, 30};
    Rectangle save = {bounds.x + 306, bounds.y + 162, 90, 30};
    if (escape || ui_focus_hit(&client->dialog_focus,2,cancel)) client->dialog_open = false;
    else if ((enter && editing) || ui_focus_hit(&client->dialog_focus,3,save)) management_start(client);
}

static void dialog_draw(WorkspaceClient *client)
{
    Rectangle bounds = dialog_bounds();
    const char *titles[] = {"", "New workspace", "Rename workspace", "New terminal", "Rename terminal", "Stop terminal?", "Stop workspace?", "", "Start new shell?", "", "Project folder"};
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 160});
    ui_panel(bounds,ui.surface,true);
    DrawTextEx(client->ui_font, titles[client->dialog_kind], (Vector2){bounds.x + 24, bounds.y + 22},UI_TEXT_TITLE,0,ui.text);
    if (client->dialog_kind >= DIALOG_TERMINAL_CLOSE && client->dialog_kind!=DIALOG_WORKSPACE_DIRECTORY) {
        char message[160];
        snprintf(message, sizeof(message), "%s", client->edit_name);
        BeginScissorMode((int)bounds.x + 24, (int)bounds.y + 58, 372, 90);
        DrawTextEx(client->ui_font, message, (Vector2){bounds.x + 24, bounds.y + 60},UI_TEXT_BODY, 0, ui.text);
        if (client->dialog_kind==DIALOG_TERMINAL_RECOVER) {
            DrawTextEx(client->ui_font,"Previous shell has stopped. Starts a fresh shell.",(Vector2){bounds.x+24,bounds.y+86},UI_TEXT_CAPTION,0,ui.muted);
            DrawTextEx(client->ui_font,"Keeps its name and theme; prior jobs do not resume.",(Vector2){bounds.x+24,bounds.y+104},UI_TEXT_CAPTION,0,ui.muted);
            DrawTextEx(client->ui_font,client->management_busy ? "Starting..." : client->dialog_error[0] ? client->dialog_error : "Tab selects a button; Enter / Space confirms.",
                (Vector2){bounds.x+24,bounds.y+128},UI_TEXT_CAPTION,0,ui.waiting);
            EndScissorMode();
            ui_focus_button(client->ui_font,client->dialog_focus,2,(Rectangle){bounds.x+180,bounds.y+162,80,30},"Cancel",!client->management_busy);
            ui_focus_action(client->ui_font,client->dialog_focus,3,(Rectangle){bounds.x+270,bounds.y+162,126,30},"Start new shell",!client->management_busy,client->dialog_kind==DIALOG_TERMINAL_CLOSE || client->dialog_kind==DIALOG_WORKSPACE_CLOSE);
            return;
        }
        snprintf(message, sizeof(message), "Removes %d shell%s; stops foreground jobs.",
            client->dialog_kind == DIALOG_WORKSPACE_CLOSE ? client->dialog_terminal_count : 1,
            client->dialog_kind == DIALOG_WORKSPACE_CLOSE && client->dialog_terminal_count != 1 ? "s" : "");
        DrawTextEx(client->ui_font, message, (Vector2){bounds.x + 24, bounds.y + 86},UI_TEXT_CAPTION, 0, ui.muted);
        DrawTextEx(client->ui_font, client->management_busy ? "Stopping..." : client->dialog_error[0] ? client->dialog_error : "Tab selects a button; Enter / Space confirms.",
            (Vector2){bounds.x + 24, bounds.y + 116},UI_TEXT_CAPTION, 0, ui.waiting);
        EndScissorMode();
        ui_focus_button(client->ui_font,client->dialog_focus,2,(Rectangle){bounds.x + 216, bounds.y + 162, 80, 30},"Cancel", !client->management_busy);
        ui_focus_action(client->ui_font,client->dialog_focus,3,(Rectangle){bounds.x + 306, bounds.y + 162, 90, 30},"Stop", !client->management_busy,client->dialog_kind==DIALOG_TERMINAL_CLOSE || client->dialog_kind==DIALOG_WORKSPACE_CLOSE);
        return;
    }
    bool dual=client->dialog_kind==DIALOG_WORKSPACE_CREATE;
    bool directory=client->dialog_kind==DIALOG_WORKSPACE_DIRECTORY;
    dialog_field(client,(Rectangle){bounds.x+24,bounds.y+64,372,36},directory ? client->edit_directory : client->edit_name,
        client->dialog_focus.item==0);
    ui_focus_ring(client->dialog_focus,0,(Rectangle){bounds.x+24,bounds.y+64,372,36});
    if (dual) {
        DrawTextEx(client->ui_font,"Project folder",
            (Vector2){bounds.x+24,bounds.y+104},UI_TEXT_CAPTION,0,ui.muted);
        dialog_field(client,(Rectangle){bounds.x+24,bounds.y+116,372,28},client->edit_directory,client->dialog_focus.item==1);
        ui_focus_ring(client->dialog_focus,1,(Rectangle){bounds.x+24,bounds.y+116,372,28});
    } else if (directory)
        DrawTextEx(client->ui_font,"New and replacement shells start here.",
            (Vector2){bounds.x+24,bounds.y+104},UI_TEXT_CAPTION,0,ui.muted);
    BeginScissorMode((int)bounds.x+24,(int)bounds.y+(dual ? 146 : 110),372,dual ? 14 : 42);
    DrawTextEx(client->ui_font,client->management_busy ? "Saving..." : client->dialog_error,
        (Vector2){bounds.x+24,bounds.y+(dual ? 146 : 128)},UI_TEXT_CAPTION,0,ui.waiting);
    EndScissorMode();
    ui_focus_button(client->ui_font,client->dialog_focus,2,(Rectangle){bounds.x + 216, bounds.y + 162, 80, 30},"Cancel", !client->management_busy);
    ui_focus_action(client->ui_font,client->dialog_focus,3,(Rectangle){bounds.x + 306, bounds.y + 162, 90, 30},
        client->dialog_kind == DIALOG_WORKSPACE_CREATE || client->dialog_kind == DIALOG_TERMINAL_CREATE ? "Create" : "Save", !client->management_busy,client->dialog_kind==DIALOG_TERMINAL_CLOSE || client->dialog_kind==DIALOG_WORKSPACE_CLOSE);
}
