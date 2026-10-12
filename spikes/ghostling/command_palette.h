/* Client actions and a searchable palette. Shared facts still use management. */
/* Focus order: search, selected result (arrows choose), Done. */
static Rectangle command_button(void) { return (Rectangle){8,GetScreenHeight()-26,168,22}; }
static Rectangle command_bounds(void)
{
    float width=fminf(640,GetScreenWidth()-48),height=fminf(490,GetScreenHeight()-40);
    return (Rectangle){(GetScreenWidth()-width)/2,(GetScreenHeight()-height)/2,width,height};
}
static int command_visible_rows(void) { return (int)((command_bounds().height-150)/46); }
static Rectangle command_row(int index)
{
    Rectangle b=command_bounds();
    return (Rectangle){b.x+16,b.y+100+index*46,b.width-32,42};
}
static int command_matches(WorkspaceClient *client, int *matches)
{
    int count=0;
    for (int i=0;i<COMMAND_COUNT;i++)
        if (overview_matches(command_definitions[i].title,client->command_query)
            || overview_matches(command_definitions[i].detail,client->command_query)) matches[count++]=i;
    return count;
}
static const char *command_unavailable(WorkspaceClient *client, int command)
{
    WorkbenchWorkspace *workspace=&client->workspaces[client->active];
    WorkbenchPane *pane=workspace->count ? workspace->panes[workspace->focused] : NULL;
    if (command==COMMAND_HELP) return NULL;
    if (client->management_busy) return "A session change is still being saved";
    switch (command) {
    case COMMAND_OVERVIEW: case COMMAND_APPEARANCE: case COMMAND_PALETTE: case COMMAND_SHORTCUTS: return NULL;
    case COMMAND_WIDTH_CYCLE: case COMMAND_WIDTH_FULL: case COMMAND_WIDTH_THIRD: case COMMAND_WIDTH_HALF: case COMMAND_WIDTH_TWO_THIRDS:
        return pane ? NULL : "This workspace has no terminals";
    case COMMAND_COPY: return pane && selection_active(pane->terminal) ? NULL : "Select terminal text to copy";
    case COMMAND_PASTE: case COMMAND_FORWARD: return pane && !pane->closed && !pane->transport.replaying ? NULL : "Focused terminal is not available for input";
    case COMMAND_TERMINAL_PREVIOUS: return workspace->focused>0 ? NULL : "Already at the first terminal";
    case COMMAND_TERMINAL_NEXT: return workspace->focused+1<workspace->count ? NULL : "No following terminal";
    case COMMAND_WORKSPACE_PREVIOUS: return client->active>0 ? NULL : "Already at the first workspace";
    case COMMAND_WORKSPACE_NEXT: return client->active+1<client->count ? NULL : "No following workspace";
    default: break;
    }
    if (!client->managed) return "Session management requires the workspace launcher";
    if (command==COMMAND_WORKSPACE_CREATE)
        return client->count<WORKBENCH_MAX_WORKSPACES ? NULL : "Eight-workspace limit; close one to create another";
    if (command==COMMAND_TERMINAL_CREATE)
        return workspace->count<WORKBENCH_PANES ? NULL : "Eight-terminal limit; stop one to create another";
    if (command==COMMAND_TERMINAL_RENAME || command==COMMAND_TERMINAL_STOP || command==COMMAND_TERMINAL_RECOVER) {
        if (!pane) return "This workspace has no terminals";
        if (command==COMMAND_TERMINAL_RECOVER && (!pane->closed || pane->runtime_state!=RUNTIME_STOPPED))
            return "Select a terminal confirmed as stopped";
    }
    return NULL;
}
static bool command_requested(void)
{
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(),command_button());
}
static void command_open(WorkspaceClient *client)
{
    navigation_finish(client);
    client->forwarding=false;client->command_settings=false;client->shortcut_recording=false;
    client->help_open=false;client->command_open=true; client->command_query[0]=0;
    client->command_focus=(UiFocus){.item=0};
    client->command_selected=client->command_first=0; client->command_all=false;
    client->divider_dragging=false;
    for (int w=0;w<client->count;w++) for (int p=0;p<client->workspaces[w].count;p++) {
        client->workspaces[w].panes[p]->selection.dragging=false;
        client->workspaces[w].panes[p]->scrollbar_dragging=false;
    }
}
static void command_dispatch(WorkspaceClient *client, int command)
{
    if (command_unavailable(client,command)) return;
    client->command_open=false;client->divider_dragging=false;
    WorkbenchWorkspace *workspace=&client->workspaces[client->active];
    switch (command) {
    case COMMAND_WIDTH_CYCLE: case COMMAND_WIDTH_FULL: case COMMAND_WIDTH_THIRD: case COMMAND_WIDTH_HALF: case COMMAND_WIDTH_TWO_THIRDS: {
        PaneWidth *width=&workspace->panes[workspace->focused]->width;
        if (command==COMMAND_WIDTH_FULL) width->full=!width->full;
        else {
            width->preset=command==COMMAND_WIDTH_CYCLE ? (width->preset+1)%3 :
                command==COMMAND_WIDTH_THIRD ? 2 : command==COMMAND_WIDTH_HALF ? 0 : 1;
            width->full=false;
        }
        workspace_focus(workspace,workspace->focused);client->navigation_reset=true;break;
    }
    case COMMAND_HELP: shortcut_help_open(client);break;
    case COMMAND_PALETTE: command_open(client);break;
    case COMMAND_SHORTCUTS: command_open(client);shortcut_load(client);client->command_settings=true;break;
    case COMMAND_COPY: {
        WorkbenchPane *pane=workspace->panes[workspace->focused];pane_bind(pane);clipboard_copy(pane->terminal);pane_unbind();break;
    }
    case COMMAND_PASTE: {
        WorkbenchPane *pane=workspace->panes[workspace->focused];pane_bind(pane);clipboard_paste(pane->fd,pane->terminal);pane_unbind();break;
    }
    case COMMAND_FORWARD:
        client->forwarding=true;strcpy(client->forward_terminal,workspace->panes[workspace->focused]->id);client->forward_deadline=GetTime()+10;break;
    case COMMAND_OVERVIEW: overview_open(client); break;
    case COMMAND_APPEARANCE: theme_open(client); break;
    case COMMAND_WORKSPACE_CREATE: dialog_open(client,DIALOG_WORKSPACE_CREATE); break;
    case COMMAND_WORKSPACE_RENAME: dialog_open(client,DIALOG_WORKSPACE_RENAME); break;
    case COMMAND_WORKSPACE_FOLDER: dialog_open(client,DIALOG_WORKSPACE_DIRECTORY); break;
    case COMMAND_WORKSPACE_STOP: dialog_open(client,DIALOG_WORKSPACE_CLOSE); break;
    case COMMAND_TERMINAL_CREATE: dialog_open(client,DIALOG_TERMINAL_CREATE); break;
    case COMMAND_TERMINAL_RENAME: dialog_open(client,DIALOG_TERMINAL_RENAME); break;
    case COMMAND_TERMINAL_STOP: dialog_open(client,DIALOG_TERMINAL_CLOSE); break;
    case COMMAND_TERMINAL_RECOVER: dialog_open(client,DIALOG_TERMINAL_RECOVER); break;
    case COMMAND_TERMINAL_PREVIOUS: case COMMAND_TERMINAL_NEXT:
        workspace_focus(workspace,workspace->focused+(command==COMMAND_TERMINAL_NEXT ? 1 : -1));
        break;
    case COMMAND_WORKSPACE_PREVIOUS: case COMMAND_WORKSPACE_NEXT: {
        client->active+=command==COMMAND_WORKSPACE_NEXT ? 1 : -1;
        int visible=(GetScreenHeight()-268)/44; if (visible<1) visible=1;
        if (client->active<client->sidebar_first) client->sidebar_first=client->active;
        if (client->active>=client->sidebar_first+visible) client->sidebar_first=client->active-visible+1;
        break;
    }
    default: break;
    }
}
static void command_reveal(WorkspaceClient *client, int count)
{
    if (!count) { client->command_selected=client->command_first=0; return; }
    if (client->command_selected<0) client->command_selected=0;
    if (client->command_selected>=count) client->command_selected=count-1;
    int rows=command_visible_rows();
    if (client->command_first>client->command_selected) client->command_first=client->command_selected;
    if (client->command_selected>=client->command_first+rows) client->command_first=client->command_selected-rows+1;
    int limit=count-rows; if (limit<0) limit=0;
    if (client->command_first>limit) client->command_first=limit;
}
static void command_input(WorkspaceClient *client)
{
    if (client->shortcut_recording) { shortcut_record_input(client);return; }
    ui_focus_begin(&client->command_focus,7,6);
    Rectangle focus_bounds=command_bounds();
    ui_focus_hit(&client->command_focus,0,(Rectangle){focus_bounds.x+16,focus_bounds.y+54,focus_bounds.width-32,32});
    bool enter=false,escape=false,backspace=false,changed=false; int direction=0,key;
    while ((key=GetKeyPressed())!=0) {
        if (ui_focus_key(&client->command_focus,key)) continue;
        enter|=key==KEY_ENTER; escape|=key==KEY_ESCAPE; backspace|=key==KEY_BACKSPACE;
        if (key==KEY_UP) direction--; if (key==KEY_DOWN) direction++;
    }
    if (IsKeyPressedRepeat(KEY_UP)) direction--;
    if (IsKeyPressedRepeat(KEY_DOWN)) direction++;
    if (client->command_focus.item==2) direction=0;
    bool editing=client->command_focus.item==0;
    bool modifier=text_edit_modifier_down();
    if (editing && modifier && IsKeyPressed(KEY_A)) client->command_all=true;
    if (editing && modifier && IsKeyPressed(KEY_V)) {
        const char *text=GetClipboardText(); size_t existing=client->command_all ? 0 : strlen(client->command_query);
        size_t length=text ? strnlen(text,sizeof(client->command_query)) : 0;
        bool valid=length && existing+length<sizeof(client->command_query);
        for (size_t i=0;valid && i<length;i++) if ((unsigned char)text[i]<32 || text[i]==127) valid=false;
        if (valid) { memcpy(client->command_query+existing,text,length+1); client->command_all=false; changed=true; }
    }
    int character;
    while ((character=GetCharPressed())!=0) {
        if (!editing || modifier || character<32 || character==127) continue;
        char bytes[4]; int length=utf8_encode((uint32_t)character,bytes);
        size_t existing=client->command_all ? 0 : strlen(client->command_query);
        if (existing+(size_t)length<sizeof(client->command_query)) {
            memcpy(client->command_query+existing,bytes,length); client->command_query[existing+length]=0;
            client->command_all=false; changed=true;
        }
    }
    if (editing && (backspace || IsKeyPressedRepeat(KEY_BACKSPACE))) {
        size_t length=strlen(client->command_query);
        if (client->command_all) length=0;
        else if (length) { length--; while (length && ((unsigned char)client->command_query[length]&0xc0)==0x80) length--; }
        client->command_query[length]=0; client->command_all=false; changed=true;
    }
    int matches[COMMAND_COUNT],count=command_matches(client,matches);
    if (changed) client->command_selected=client->command_first=0;
    client->command_selected+=direction;
    float wheel=GetMouseWheelMove(); if (wheel) client->command_selected+=wheel>0 ? -1 : 1;
    command_reveal(client,count);
    bool clicked=IsMouseButtonPressed(MOUSE_BUTTON_LEFT); Vector2 pointer=GetMousePosition();
    Rectangle bounds=command_bounds();
    if (escape || ui_focus_hit(&client->command_focus,2,(Rectangle){bounds.x+bounds.width-80,bounds.y+14,64,24})) {
        client->command_open=false; return;
    }
    if (clicked) for (int i=client->command_first;i<count && i<client->command_first+command_visible_rows();i++)
        if (CheckCollisionPointRec(pointer,command_row(i-client->command_first))) {
            client->command_selected=i; if (client->command_settings) shortcut_edit_open(client,matches[i]); else command_dispatch(client,matches[i]); return;
        }
    if (((enter && client->command_focus.item<=1) || client->command_focus.activated==1) && count) {
        if (client->command_settings) shortcut_edit_open(client,matches[client->command_selected]);
        else command_dispatch(client,matches[client->command_selected]);
    }
}
static void command_draw(WorkspaceClient *client)
{
    if (client->shortcut_recording) { shortcut_record_draw(client);return; }
    Rectangle b=command_bounds();
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){0,0,0,160}); ui_panel(b,ui.surface,true);
    DrawTextEx(client->ui_font,client->command_settings ? "Shortcut settings" : "Commands",(Vector2){b.x+20,b.y+20},UI_TEXT_TITLE,0,ui.text);
    ui_focus_button(client->ui_font,client->command_focus,2,(Rectangle){b.x+b.width-80,b.y+14,64,24},"Done",true);
    Rectangle field={b.x+16,b.y+54,b.width-32,32};
    DrawRectangleRounded(field,.2f,8,ui.background); DrawRectangleRoundedLinesEx(field,.2f,8,1,ui.accent);
    ui_focus_ring(client->command_focus,0,field);
    BeginScissorMode((int)field.x+8,(int)field.y,(int)field.width-16,(int)field.height);
    Vector2 size=MeasureTextEx(client->ui_font,client->command_query,UI_TEXT_BODY,0);
    if (client->command_focus.item==0 && client->command_all && client->command_query[0]) DrawRectangleRec((Rectangle){field.x+8,field.y+6,size.x,20},ui_color(current_theme.selection_background));
    DrawTextEx(client->ui_font,client->command_query[0] ? client->command_query : "Search actions...",(Vector2){field.x+8,field.y+8},UI_TEXT_BODY,0,
        !client->command_query[0] ? ui.muted : client->command_focus.item==0 && client->command_all ? ui_color(current_theme.selection_foreground) : ui.text);
    if (client->command_focus.item==0 && !client->command_all && ((int)(GetTime()*2)%2==0)) DrawLine((int)(field.x+8+size.x),(int)field.y+6,(int)(field.x+8+size.x),(int)field.y+26,ui.text);
    EndScissorMode();
    int matches[COMMAND_COUNT],count=command_matches(client,matches);
    for (int i=client->command_first;i<count && i<client->command_first+command_visible_rows();i++) {
        int command=matches[i]; Rectangle row=command_row(i-client->command_first);
        const char *reason=client->command_settings ? NULL : command_unavailable(client,command); bool selected=i==client->command_selected;
        DrawRectangleRounded(row,.2f,8,selected ? ui.selected : CheckCollisionPointRec(GetMousePosition(),row) ? ui.hover : ui.surface);
        if (selected) ui_focus_ring(client->command_focus,1,row);
        if (selected) DrawRectangleRounded((Rectangle){row.x+2,row.y+8,3,row.height-16},.5f,6,ui.accent);
        char binding[96];shortcut_label(client->shortcuts.bindings[command],shortcut_platform_apple(),binding,sizeof(binding));
        if (!client->shortcuts.bindings[command].key) strcpy(binding,client->command_settings ? "Unbound" : "");
        Vector2 binding_size=MeasureTextEx(client->font,binding,12,0);
        DrawTextEx(client->font,binding,(Vector2){row.x+row.width-binding_size.x-12,row.y+7},12,0,ui.muted);
        BeginScissorMode((int)row.x+10,(int)row.y,(int)(row.width-28-binding_size.x),(int)row.height);
        DrawTextEx(client->ui_font,command_definitions[command].title,(Vector2){row.x+12,row.y+5},UI_TEXT_LEAD,0,reason ? ui.muted : ui.text);
        DrawTextEx(client->ui_font,reason ? reason : client->command_settings ? ((command==COMMAND_OVERVIEW || command==COMMAND_HELP) ? "Working view / overview; Enter edits" : "Working view; Enter edits binding") : command_definitions[command].detail,(Vector2){row.x+12,row.y+24},UI_TEXT_CAPTION,0,reason ? ui.waiting : ui.muted);
        EndScissorMode();
    }
    if (!count) DrawTextEx(client->ui_font,"No matching actions",(Vector2){b.x+24,b.y+110},UI_TEXT_BODY,0,ui.muted);
    char hint[160],opening[96];shortcut_label(client->shortcuts.bindings[COMMAND_PALETTE],shortcut_platform_apple(),opening,sizeof(opening));
    snprintf(hint,sizeof(hint),"%s   Tab focuses   Arrows choose   Enter %s   Esc closes",opening,client->command_settings ? "edits" : "runs");
    DrawTextEx(client->ui_font,client->shortcut_error[0] ? client->shortcut_error : hint,(Vector2){b.x+20,b.y+b.height-24},UI_TEXT_CAPTION,0,client->shortcut_error[0] ? ui.waiting : ui.muted);
}
