/* Desktop shortcut routing and the client-owned binding editor. */
/* Focus order: capture, Clear, Default, Cancel, Save. */
static const ShortcutState *shortcut_base(WorkspaceClient *client)
{ return getenv("WORKBENCH_LAYERED_SHORTCUTS") ? &client->config.shortcuts : NULL; }
static char shortcut_copy_hint[128],shortcut_paste_hint[128];
static void shortcut_refresh_hints(WorkspaceClient *client)
{
    char label[96];shortcut_label(client->shortcuts.bindings[COMMAND_COPY],shortcut_platform_apple(),label,sizeof(label));
    snprintf(shortcut_copy_hint,sizeof(shortcut_copy_hint),"Copy selection (%s)",client->shortcuts.bindings[COMMAND_COPY].key ? label : "mouse / Commands");
    shortcut_label(client->shortcuts.bindings[COMMAND_PASTE],shortcut_platform_apple(),label,sizeof(label));
    snprintf(shortcut_paste_hint,sizeof(shortcut_paste_hint),"Paste (%s)",client->shortcuts.bindings[COMMAND_PASTE].key ? label : "mouse / Commands");
    window_copy_hint=shortcut_copy_hint;window_paste_hint=shortcut_paste_hint;
}
static void shortcut_load(WorkspaceClient *client)
{
    shortcut_refresh_hints(client);
    if (!client->shortcut_path) return;
    ShortcutState next;
    if (shortcut_read_layer(client->shortcut_path,shortcut_platform_apple(),shortcut_base(client),&next,client->shortcut_error,sizeof(client->shortcut_error))) {
        client->shortcuts=next;shortcut_refresh_hints(client);client->shortcut_error[0]=0;
    }
}
static void shortcut_edit_open(WorkspaceClient *client, int action)
{
    client->shortcut_focus=(UiFocus){.item=0};
    client->shortcut_edit=action;client->shortcut_recording=true;client->shortcut_capture_valid=true;
    client->shortcut_candidate=client->shortcuts.bindings[action];client->shortcut_error[0]=0;
}
static void shortcut_edit_save(WorkspaceClient *client)
{
    if (!client->shortcut_capture_valid) return;
    if (!client->shortcut_path) { strcpy(client->shortcut_error,"Use just spike to save shortcut settings");return; }
    ShortcutState next;
    if (!shortcut_read_layer(client->shortcut_path,shortcut_platform_apple(),shortcut_base(client),&next,client->shortcut_error,sizeof(client->shortcut_error))) return;
    next.bindings[client->shortcut_edit]=client->shortcut_candidate;
    if (!shortcut_write_layer(client->shortcut_path,&next,shortcut_base(client),client->shortcut_error,sizeof(client->shortcut_error))) return;
    client->shortcuts=next;shortcut_refresh_hints(client);client->shortcut_recording=false;client->shortcut_error[0]=0;
}
static void shortcut_record_input(WorkspaceClient *client)
{
    ui_focus_begin(&client->shortcut_focus,31,30);
    Rectangle capture_bounds=dialog_bounds();
    ui_focus_hit(&client->shortcut_focus,0,(Rectangle){capture_bounds.x+24,capture_bounds.y+72,372,32});
    int key;bool escape=false,save=false;
    while ((key=GetKeyPressed())!=0) {
        if (ui_focus_key(&client->shortcut_focus,key)) continue;
        if (key==KEY_F12) continue; /* Reserved screenshot key is never recorded. */
        if (key==KEY_ESCAPE) { escape=true;continue; }
        if (key==KEY_ENTER && !shortcut_modifiers()) { save=true;continue; }
        if (client->shortcut_focus.item!=0) continue;
        if (key>=KEY_LEFT_SHIFT && key<=KEY_RIGHT_SUPER) continue;
        ShortcutChord candidate={key,shortcut_modifiers()};
        if (shortcut_allowed(candidate)) { client->shortcut_candidate=candidate;client->shortcut_capture_valid=true;client->shortcut_error[0]=0; }
        else { client->shortcut_capture_valid=false;strcpy(client->shortcut_error,"Reserved or unsupported chord; try another key"); }
        if (key>=0 && key<512) workbench_key_blocked[key]=true;
    }
    while (GetCharPressed()!=0) { }
    Rectangle b=dialog_bounds();
    if (ui_focus_hit(&client->shortcut_focus,1,(Rectangle){b.x+24,b.y+162,72,30})) { client->shortcut_candidate=(ShortcutChord){0};client->shortcut_capture_valid=true;client->shortcut_error[0]=0; }
    if (ui_focus_hit(&client->shortcut_focus,2,(Rectangle){b.x+104,b.y+162,92,30})) {
        client->shortcut_candidate=shortcut_base(client) ? client->config.shortcuts.bindings[client->shortcut_edit] : shortcut_defaults(shortcut_platform_apple()).bindings[client->shortcut_edit];client->shortcut_capture_valid=true;client->shortcut_error[0]=0;
    }
    if (escape || ui_focus_hit(&client->shortcut_focus,3,(Rectangle){b.x+216,b.y+162,80,30})) client->shortcut_recording=false;
    else if (save || ui_focus_hit(&client->shortcut_focus,4,(Rectangle){b.x+306,b.y+162,90,30})) shortcut_edit_save(client);
}
static void shortcut_record_draw(WorkspaceClient *client)
{
    Rectangle b=dialog_bounds();char label[96];shortcut_label(client->shortcut_candidate,shortcut_platform_apple(),label,sizeof(label));
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){0,0,0,160});ui_panel(b,ui.surface,true);
    DrawTextEx(client->ui_font,"Set shortcut",(Vector2){b.x+24,b.y+22},UI_TEXT_TITLE,0,ui.text);
    DrawTextEx(client->ui_font,command_definitions[client->shortcut_edit].title,(Vector2){b.x+24,b.y+50},UI_TEXT_BODY,0,ui.muted);
    DrawRectangleRounded((Rectangle){b.x+24,b.y+72,372,32},.2f,8,ui.background);
    ui_focus_ring(client->shortcut_focus,0,(Rectangle){b.x+24,b.y+72,372,32});
    DrawTextEx(client->ui_font,!client->shortcut_candidate.key ? "Unbound" : label,(Vector2){b.x+32,b.y+80},UI_TEXT_BODY,0,ui.text);
    DrawTextEx(client->ui_font,"Press a shortcut, then Save. Escape cancels.",(Vector2){b.x+24,b.y+112},UI_TEXT_CAPTION,0,ui.muted);
    BeginScissorMode((int)b.x+24,(int)b.y+130,372,28);
    DrawTextEx(client->ui_font,client->shortcut_error,(Vector2){b.x+24,b.y+132},UI_TEXT_CAPTION,0,ui.waiting);EndScissorMode();
    ui_focus_button(client->ui_font,client->shortcut_focus,1,(Rectangle){b.x+24,b.y+162,72,30},"Clear",true);
    ui_focus_button(client->ui_font,client->shortcut_focus,2,(Rectangle){b.x+104,b.y+162,92,30},"Default",true);
    ui_focus_button(client->ui_font,client->shortcut_focus,3,(Rectangle){b.x+216,b.y+162,80,30},"Cancel",true);
    ui_focus_action(client->ui_font,client->shortcut_focus,4,(Rectangle){b.x+306,b.y+162,90,30},"Save",client->shortcut_capture_valid,false);
}
static void shortcut_drain(void)
{
    while (GetKeyPressed()!=0) { }
    while (GetCharPressed()!=0) { }
}
static bool shortcut_route(WorkspaceClient *client, bool *bypass)
{
    *bypass=false;
    for (int key=0;key<512;key++) if (workbench_key_blocked[key] && !IsKeyDown(key) && !IsKeyReleased(key)) workbench_key_blocked[key]=false;
    if (client->passthrough_key && !IsKeyDown(client->passthrough_key) && !IsKeyReleased(client->passthrough_key)) client->passthrough_key=0;
    if (!IsWindowFocused()) { client->forwarding=false;return false; }
    if (client->search.open || client->command_open || client->text_open || client->theme_open || client->dialog_open) return false;
    WorkbenchWorkspace *w=&client->workspaces[client->active];WorkbenchPane *pane=w->count ? w->panes[w->focused] : NULL;
    if (client->forwarding) {
        if (!pane || pane->closed || pane->transport.replaying || strcmp(pane->id,client->forward_terminal) || GetTime()>client->forward_deadline || client->overview_open || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            client->forwarding=false;clipboard_notice("Send-next-key cancelled; terminal focus or availability changed");
        } else {
            workbench_input_peek();
            if (workbench_input_has_key(KEY_ESCAPE) || IsKeyPressed(KEY_ESCAPE)) {
                client->forwarding=false;shortcut_drain();clipboard_notice("Send-next-key cancelled");return true;
            }
            for (int key=32;key<340;key++) if (!workbench_key_blocked[key] && (workbench_input_has_key(key) || IsKeyPressed(key))) {
                client->forwarding=false;client->passthrough_key=key;*bypass=true;
                clipboard_notice("Key sent to terminal");return false;
            }
        }
    }
    if (client->passthrough_key) { *bypass=true;return false; }
    unsigned mods=shortcut_modifiers();
    for (int i=0;i<COMMAND_COUNT;i++) {
        ShortcutChord chord=client->shortcuts.bindings[i];
        bool repeat=i==COMMAND_TERMINAL_PREVIOUS || i==COMMAND_TERMINAL_NEXT || i==COMMAND_WORKSPACE_PREVIOUS || i==COMMAND_WORKSPACE_NEXT || i==COMMAND_AGENT_INPUT_NEXT || i==COMMAND_AGENT_INPUT_PREVIOUS;
        if (!chord.key || chord.mods!=mods || (!IsKeyPressed(chord.key) && !(repeat && IsKeyPressedRepeat(chord.key)))) continue;
        if (client->help_open && i!=COMMAND_HELP) continue;
        if (client->overview_open && ((i!=COMMAND_OVERVIEW && i!=COMMAND_HELP) || client->overview_drag_armed || client->management_busy)) return false;
        workbench_key_blocked[chord.key]=true;shortcut_drain();
        if (i==COMMAND_HELP) {
            if (client->help_open) client->help_open=false;
            else if (!client->overview_closing) shortcut_help_open(client);
        }
        else if (client->overview_open && !client->overview_closing) overview_close(client,-1,-1);
        else if (client->overview_open && client->overview_closing) overview_open(client);
        else {
            const char *reason=command_unavailable(client,i);
            if (reason) clipboard_notice(reason);
            else command_dispatch(client,i);
        }
        return true;
    }
    return false;
}
