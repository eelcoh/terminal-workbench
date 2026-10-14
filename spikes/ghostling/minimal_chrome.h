/* Per-window presentation. Runtime actions reuse the existing registry. */
#include "chrome_menu_layout.h"
static Rectangle chrome_menu_button(void) { return (Rectangle){8,6,28,28}; }
static Rectangle chrome_workspace_button(void) { return (Rectangle){44,6,160,28}; }
static Rectangle chrome_rail_button(int i) { return (Rectangle){8,52+i*42,28,28}; }
static Rectangle chrome_help_button(void) { return (Rectangle){8,GetScreenHeight()-40,28,28}; }
static Rectangle chrome_add_button(void) { return (Rectangle){GetScreenWidth()/2.0f+fmaxf(28,chrome_pane_count*28)/2+52,6,28,28}; }
static Rectangle chrome_panel(void) { return (Rectangle){44,40,244,GetScreenHeight()-40}; }
static int chrome_rows(WorkspaceClient *client)
{ return (int)fmaxf(1,fminf(24,(GetScreenHeight()-(client->chrome_agents ? 160 : 150))/52)); }
static Rectangle chrome_row(WorkspaceClient *client,int i)
{ return (Rectangle){56,(client->chrome_agents ? 140 : 94)+i*52,220,46}; }
static Rectangle chrome_workspace_close(WorkspaceClient *client,int row)
{Rectangle b=chrome_row(client,row);return (Rectangle){b.x+b.width-30,b.y+10,26,26};}
static void chrome_remove_workspace(WorkspaceClient *client,int workspace)
{
    if (!client->managed || client->management_busy || workspace<0 || workspace>=client->count) return;
    navigation_finish(client);client->divider_dragging=false;
    dialog_open_workspace(client,DIALOG_WORKSPACE_CLOSE,workspace);
    if (!client->chrome_pinned) client->chrome_drawer=false;
    client->chrome_keyboard=false;
}
static void chrome_close_transient(WorkspaceClient *client)
{ client->chrome_menu=0;client->chrome_keyboard=false;if (!client->chrome_pinned) client->chrome_drawer=false; }
static void chrome_pin(WorkspaceClient *client)
{
    client->chrome_pinned=!client->chrome_pinned;client->chrome_drawer=client->chrome_pinned;client->chrome_keyboard=false;
    chrome_sidebar_width=client->chrome_pinned ? 288 : 44;
    client->navigation_reset=true;client->divider_dragging=false;
}
static int chrome_agent_items(WorkspaceClient *client,OverviewItem *items,int *waiting)
{
    int count=0;*waiting=0;
    for (int w=0;w<client->count;w++) for (int p=0;p<client->workspaces[w].count;p++) {
        WorkbenchPane *pane=client->workspaces[w].panes[p];
        if (!remote_agent_name(pane->transport.agent)[0]) continue;
        bool needs=!pane->closed && pane->transport.attention==2;
        if (needs) (*waiting)++;
        if (!client->chrome_waiting || needs) items[count++]=(OverviewItem){w,p};
    }
    return count;
}
/* A pane menu is anchored to stable identity, never an array index. */
static bool chrome_menu_target(WorkspaceClient *client)
{
    if (client->chrome_menu==1) return true;
    for (int w=0;w<client->count;w++) for (int p=0;p<client->workspaces[w].count;p++)
        if (!strcmp(client->workspaces[w].panes[p]->id,client->chrome_target)) {
            client->active=w;workspace_focus(&client->workspaces[w],p);return true;
        }
    client->chrome_menu=0;return false;
}
static int chrome_menu_actions(WorkspaceClient *client,int *actions)
{
    const int app[]={COMMAND_TERMINAL_CREATE,COMMAND_WORKSPACE_CREATE,COMMAND_WORKSPACE_FOLDER,COMMAND_WORKSPACE_RENAME,
        COMMAND_WORKSPACE_STOP,COMMAND_PALETTE,COMMAND_APPEARANCE,COMMAND_FONT_SETTINGS,COMMAND_SHORTCUTS,COMMAND_HELP,COMMAND_COPY,COMMAND_PASTE,COMMAND_SESSION_STOP,-1};
    const int pane[]={COMMAND_TERMINAL_RENAME,COMMAND_APPEARANCE,COMMAND_WIDTH_THIRD,COMMAND_WIDTH_HALF,
        COMMAND_WIDTH_TWO_THIRDS,COMMAND_WIDTH_FULL,-2,COMMAND_TERMINAL_STOP};
    int count=client->chrome_menu==1 ? (int)(sizeof(app)/sizeof(*app)) : client->chrome_menu==3 ? client->count : (int)(sizeof(pane)/sizeof(*pane));
    for (int i=0;i<count;i++) actions[i]=client->chrome_menu==1 ? app[i] : client->chrome_menu==3 ? -3 : pane[i];
    return count;
}
static Rectangle chrome_popup(WorkspaceClient *client)
{
    int actions[16],count=chrome_menu_actions(client,actions);
    float height=fminf(chrome_menu_height(client->chrome_menu,count),GetScreenHeight()-52);
    return (Rectangle){fmaxf(8,fminf(client->chrome_anchor.x,GetScreenWidth()-288)),
        fmaxf(44,fminf(client->chrome_anchor.y+client->chrome_anchor.height+4,GetScreenHeight()-height-8)),280,height};
}
static Rectangle chrome_menu_row(WorkspaceClient *client,int i)
{ Rectangle b=chrome_popup(client);return (Rectangle){b.x+6,b.y+6+chrome_menu_offset(client->chrome_menu,i)-chrome_menu_offset(client->chrome_menu,client->chrome_menu_first),b.width-12,30}; }
static void chrome_open_menu(WorkspaceClient *client,int kind,Rectangle anchor)
{
    navigation_finish(client);client->chrome_menu=kind;client->chrome_anchor=anchor;client->chrome_menu_first=0;
    client->chrome_focus=(UiFocus){.item=-1,.visible=true};client->divider_dragging=false;client->forwarding=false;
    if (kind!=1) strcpy(client->chrome_target,client->workspaces[client->active].panes[client->workspaces[client->active].focused]->id);
}
static void chrome_activate(WorkspaceClient *client,int action,int index)
{
    int kind=client->chrome_menu;
    client->chrome_menu=0;
    if (action==-1) client->chrome_quit=true;
    else if (action==-2) chrome_open_menu(client,3,client->chrome_anchor);
    else if (action==-3) {
        WorkbenchWorkspace *workspace=&client->workspaces[client->active];
        if (index!=client->active && client->managed && !client->management_busy)
            management_move_start(client,workspace->panes[workspace->focused]->id,client->workspaces[index].id,client->workspaces[index].count);
    } else {
        if (action==COMMAND_APPEARANCE && kind==2) {
            theme_open_target(client,true);
        } else command_dispatch(client,action);
    }
    if (client->dialog_open || client->text_open || client->theme_open || client->command_open || client->help_open || client->overview_open)
        if (!client->chrome_pinned) client->chrome_drawer=false;
}
/* Return true for the whole dismissal/dispatch frame: no click-through or PTY keys. */
static bool chrome_input(WorkspaceClient *client)
{
    Vector2 pointer=GetMousePosition();bool pressed=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool transient=client->chrome_menu || (client->chrome_drawer && (!client->chrome_pinned || client->chrome_keyboard));
    bool blocked=client->dialog_open || client->text_open || client->theme_open || client->command_open || client->help_open || client->overview_open;
    if (blocked) { chrome_close_transient(client);return false; }
    if (client->chrome_menu) {
        if (!chrome_menu_target(client)) return true;
        int actions[16],count=chrome_menu_actions(client,actions);uint32_t enabled=0;
        for (int i=0;i<count;i++) {
            int action=actions[i];bool available=action>=0 ? !command_unavailable(client,action) :
                action==-3 ? client->managed && !client->management_busy && i!=client->active && client->workspaces[i].count<WORKBENCH_PANES :
                action==-2 ? client->managed && !client->management_busy : true;
            if (available) enabled|=ui_focus_bit(i);
        }
        ui_focus_begin(&client->chrome_focus,enabled,enabled);
        if (client->chrome_focus.item<0) client->chrome_focus.item=ui_focus_next(-1,1,enabled);
        int key;bool escape=false;
        while ((key=GetKeyPressed())!=0) {
            if (key==KEY_UP || key==KEY_DOWN) { ui_focus_step(&client->chrome_focus,key==KEY_UP);continue; }
            escape|=key==KEY_ESCAPE;ui_focus_key(&client->chrome_focus,key);
        }
        while (GetCharPressed()!=0) { }
        if (escape || (pressed && !CheckCollisionPointRec(pointer,chrome_popup(client)))) { client->chrome_menu=0;return true; }
        int height=(int)chrome_popup(client).height;
        int last_first=chrome_menu_last_first(client->chrome_menu,count,height);
        float wheel=GetMouseWheelMove();
        if (wheel && CheckCollisionPointRec(pointer,chrome_popup(client))) {
            client->chrome_menu_first=(int)fmaxf(0,fminf(last_first,client->chrome_menu_first+(wheel>0 ? -1 : 1)));
            client->chrome_focus.item=ui_focus_next(client->chrome_menu_first-1,1,enabled);client->chrome_focus.visible=false;
        }
        if (client->chrome_focus.item>=0 && client->chrome_focus.item<client->chrome_menu_first) client->chrome_menu_first=client->chrome_focus.item;
        while (client->chrome_focus.item>=0 && client->chrome_menu_first<last_first && !chrome_menu_fits(client->chrome_menu,client->chrome_menu_first,client->chrome_focus.item,height)) client->chrome_menu_first++;
        client->chrome_menu_first=(int)fmaxf(0,fminf(last_first,client->chrome_menu_first));
        for (int i=client->chrome_menu_first;i<count && chrome_menu_fits(client->chrome_menu,client->chrome_menu_first,i,height);i++)
            if (ui_focus_hit(&client->chrome_focus,i,chrome_menu_row(client,i))) { chrome_activate(client,actions[i],i);break; }
        return true;
    }
    OverviewItem drawer_items[64];int drawer_waiting=0;
    int drawer_count=client->chrome_drawer && client->chrome_agents ? chrome_agent_items(client,drawer_items,&drawer_waiting) : client->count;
    int drawer_rows=chrome_rows(client);
    client->chrome_first=(int)fmaxf(0,fminf(client->chrome_first,fmaxf(0,drawer_count-drawer_rows)));
    uint32_t enabled=ui_focus_bit(0);
    if (client->chrome_agents) enabled|=ui_focus_bit(1)|ui_focus_bit(2);
    else if (client->managed && !command_unavailable(client,COMMAND_WORKSPACE_CREATE)) enabled|=ui_focus_bit(3);
    for (int i=0;i<drawer_rows && i+client->chrome_first<drawer_count;i++) {
        enabled|=ui_focus_bit(i+4);
        if (!client->chrome_agents && client->managed && !client->management_busy) enabled|=ui_focus_bit(i+12);
    }
    ui_focus_begin(&client->chrome_focus,enabled,enabled);
    if (transient) {
        int key;bool escape=false;
        while ((key=GetKeyPressed())!=0) {
            escape|=key==KEY_ESCAPE;
            if (key==KEY_DOWN || key==KEY_UP) {
                if (key==KEY_DOWN && (client->chrome_focus.item==drawer_rows+3 || (!client->chrome_agents && client->chrome_focus.item==drawer_rows+11)) && client->chrome_first+drawer_rows<drawer_count)
                    client->chrome_first++;
                else if (key==KEY_UP && (client->chrome_focus.item==4 || (!client->chrome_agents && client->chrome_focus.item==12)) && client->chrome_first>0) client->chrome_first--;
                else ui_focus_step(&client->chrome_focus,key==KEY_UP);
                client->chrome_focus.visible=true;continue;
            }
            ui_focus_key(&client->chrome_focus,key);
        }
        while (GetCharPressed()!=0) { }
        if (escape) { if (!client->chrome_pinned) client->chrome_drawer=false;client->chrome_keyboard=false;return true; }
        if (client->chrome_focus.activated==0) { chrome_pin(client);return true; }
        if (client->chrome_focus.activated==1 || client->chrome_focus.activated==2) {
            client->chrome_waiting=client->chrome_focus.activated==2;client->chrome_first=0;return true;
        }
        if (client->chrome_focus.activated==3) { command_dispatch(client,COMMAND_WORKSPACE_CREATE);return true; }
        if (!client->chrome_agents && client->chrome_focus.activated>=12) {
            chrome_remove_workspace(client,client->chrome_focus.activated-12+client->chrome_first);return true;
        }
        if (client->chrome_focus.activated>=4) {
            int i=client->chrome_focus.activated-4+client->chrome_first;
            if (i<drawer_count) {
                client->active=client->chrome_agents ? drawer_items[i].workspace : i;
                WorkbenchWorkspace *w=&client->workspaces[client->active];
                workspace_focus(w,client->chrome_agents ? drawer_items[i].pane : w->focused);
                if (!client->chrome_pinned) client->chrome_drawer=false;
                client->chrome_keyboard=false;client->navigation_reset=true;return true;
            }
        }
    }
    if (pressed && CheckCollisionPointRec(pointer,chrome_menu_button())) { chrome_open_menu(client,1,chrome_menu_button());return true; }
    if (pressed && (CheckCollisionPointRec(pointer,chrome_workspace_button()) || CheckCollisionPointRec(pointer,chrome_rail_button(0)) || CheckCollisionPointRec(pointer,chrome_rail_button(1)))) {
        bool agents=CheckCollisionPointRec(pointer,chrome_rail_button(1));
        client->chrome_drawer=client->chrome_pinned || !client->chrome_drawer || client->chrome_agents!=agents;
        client->chrome_agents=agents;client->chrome_keyboard=client->chrome_drawer;client->chrome_first=0;client->chrome_focus=(UiFocus){.item=0};return true;
    }
    if (pressed && CheckCollisionPointRec(pointer,chrome_help_button())) { command_dispatch(client,COMMAND_HELP);return true; }
    if (pressed && CheckCollisionPointRec(pointer,chrome_add_button())) { command_dispatch(client,COMMAND_TERMINAL_CREATE);return true; }
    if (client->chrome_drawer) {
        OverviewItem items[64];int waiting=0,count=client->chrome_agents ? chrome_agent_items(client,items,&waiting) : client->count;
        Rectangle panel=chrome_panel();int rows=chrome_rows(client);
        if (CheckCollisionPointRec(pointer,panel)) {
            float wheel=GetMouseWheelMove();if (wheel) client->chrome_first+=wheel>0 ? -1 : 1;
            client->chrome_first=(int)fmaxf(0,fminf(client->chrome_first,fmaxf(0,count-rows)));
            if (pressed && CheckCollisionPointRec(pointer,(Rectangle){228,54,48,28})) { chrome_pin(client);return true; }
            if (client->chrome_agents && pressed && pointer.y>=98 && pointer.y<130) {
                client->chrome_waiting=pointer.x>=150;client->chrome_first=0;return true;
            }
            if (!client->chrome_agents && pressed && CheckCollisionPointRec(pointer,(Rectangle){56,GetScreenHeight()-46,220,30})) {
                command_dispatch(client,COMMAND_WORKSPACE_CREATE);return true;
            }
            for (int i=client->chrome_first;i<count && i<client->chrome_first+rows;i++) if (pressed && CheckCollisionPointRec(pointer,chrome_row(client,i-client->chrome_first))) {
                if (!client->chrome_agents && CheckCollisionPointRec(pointer,chrome_workspace_close(client,i-client->chrome_first))) {
                    chrome_remove_workspace(client,i);return true;
                }
                client->active=client->chrome_agents ? items[i].workspace : i;
                WorkbenchWorkspace *workspace=&client->workspaces[client->active];
                workspace_focus(workspace,client->chrome_agents ? items[i].pane : workspace->focused);
                if (!client->chrome_pinned) client->chrome_drawer=false;
                client->chrome_keyboard=false;client->navigation_reset=true;return true;
            }
            return pressed || transient || GetMouseWheelMove()!=0;
        }
        if (transient && pressed) { if (!client->chrome_pinned) client->chrome_drawer=false;client->chrome_keyboard=false;return true; }
    }
    return transient;
}
static void chrome_popup_draw(WorkspaceClient *client)
{
    if (!client->chrome_menu) return;
    int actions[16],count=chrome_menu_actions(client,actions);Rectangle popup=chrome_popup(client);
    DrawRectangleRounded(popup,ui_roundness(popup,10),12,ui.surface);
    DrawRectangleRoundedLinesEx(popup,ui_roundness(popup,10),12,1,ui.border);
    int height=(int)popup.height;const char *unavailable=NULL;
    for (int i=client->chrome_menu_first;i<count && chrome_menu_fits(client->chrome_menu,client->chrome_menu_first,i,height);i++) {
        Rectangle section=chrome_menu_row(client,i);
        if (i>client->chrome_menu_first && chrome_menu_separator(client->chrome_menu,i))
            DrawLineEx((Vector2){section.x+8,section.y-5},(Vector2){section.x+section.width-8,section.y-5},1,ui_mix(ui.surface,ui.border,.7f));
        int action=actions[i];Rectangle row=section;bool enabled=client->chrome_focus.enabled & ui_focus_bit(i);
        if (CheckCollisionPointRec(GetMousePosition(),row) || client->chrome_focus.item==i)
            DrawRectangleRounded(row,ui_roundness(row,6),8,ui.hover);
        const char *title=action>=0 ? command_definitions[action].title : action==-1 ? "Close window" : action==-2 ? "Move to workspace…" : client->workspaces[i].name;
        if (action==COMMAND_APPEARANCE && client->chrome_menu==2) title="Terminal theme…";
        if (!enabled && action>=0 && CheckCollisionPointRec(GetMousePosition(),row)) unavailable=command_unavailable(client,action);
        char key[96]={0};if (action>=0 && client->shortcuts.bindings[action].key) shortcut_label(client->shortcuts.bindings[action],shortcut_platform_apple(),key,sizeof(key));
        float key_width=key[0] ? MeasureTextEx(client->ui_font,key,UI_TEXT_CAPTION,0).x+12 : 0;
        Color color=!enabled ? ui.muted : action==COMMAND_TERMINAL_STOP || action==COMMAND_WORKSPACE_STOP ? ui.danger : ui.text;
        chrome_name(client->ui_font,title,(Rectangle){row.x+8,row.y+8,row.width-key_width-16,20},UI_TEXT_BODY,color);
        if (key[0]) DrawTextEx(client->ui_font,key,(Vector2){row.x+row.width-key_width,row.y+9},UI_TEXT_CAPTION,0,ui.muted);
        ui_focus_ring(client->chrome_focus,i,row);
    }
    if (unavailable) {
        Rectangle hint={popup.x,fminf(popup.y+popup.height+6,GetScreenHeight()-34),popup.width,28};
        DrawRectangleRounded(hint,ui_roundness(hint,6),8,ui.raised);
        chrome_name(client->ui_font,unavailable,(Rectangle){hint.x+8,hint.y+8,hint.width-16,18},UI_TEXT_CAPTION,ui.text);
    }
}
static void minimal_chrome_draw(WorkspaceClient *client)
{
    WorkbenchWorkspace *workspace=&client->workspaces[client->active];
    DrawRectangle(0,0,GetScreenWidth(),40,ui.surface);
    Rectangle menu=chrome_menu_button();
    draw_quiet_button(client->ui_font,menu,"",true,UI_TEXT_LEAD);
    for (int i=0;i<3;i++) DrawLineEx((Vector2){menu.x+6,menu.y+8+i*6},(Vector2){menu.x+22,menu.y+8+i*6},1.5f,ui.text);
    chrome_name(client->ui_font,workspace->name,(Rectangle){52,13,152,20},UI_TEXT_BODY,ui.text);
    Rectangle positions=terminal_navigation_strip();DrawRectangleRounded(positions,ui_roundness(positions,14),12,ui.background);
    float estate=workspace_estate(),scroll=navigation_moving(client) ? overview_animation_value(&client->navigation_animation,GetTime()) : workspace->scroll;
    PaneWidth widths[8];workspace_widths(workspace,widths);
    for (int p=0;p<workspace->count;p++) {
        Rectangle b=terminal_position(p);bool focus=p==workspace->focused;
        bool visible=pane_width_visible(widths,p,scroll,estate,WORKBENCH_GAP);
        Rectangle pip={b.x+(focus ? 2 : 7),16,focus ? 24 : 14,8};
        DrawRectangleRounded(pip,1,8,focus ? ui.accent : visible ? ui_mix(ui.surface,ui.accent,.55f) : ui.border);
        if (!workspace->panes[p]->closed && workspace->panes[p]->transport.attention==2) DrawCircle(b.x+25,10,2,ui.waiting);
    }
    char count[16];snprintf(count,sizeof(count),"%d / %d",workspace->count ? workspace->focused+1 : 0,workspace->count);
    DrawTextEx(client->ui_font,count,(Vector2){positions.x+positions.width+8,14},UI_TEXT_CAPTION,0,ui.muted);
    draw_quiet_button(client->ui_font,chrome_add_button(),"+",client->managed && workspace->count<8,UI_TEXT_LEAD);
    draw_quiet_button(client->ui_font,overview_button(),"Overview",true,UI_TEXT_BODY);
    DrawRectangle(0,40,44,GetScreenHeight()-40,ui.surface);
    for (int i=0;i<2;i++) {
        Rectangle b=chrome_rail_button(i);draw_quiet_button(client->ui_font,b,"",true,UI_TEXT_LEAD);
        if (client->chrome_drawer && client->chrome_agents==(i==1)) DrawRectangleRounded(b,ui_roundness(b,6),8,ui.selected);
        if (!i) for (int j=0;j<2;j++) DrawRectangleLinesEx((Rectangle){b.x+6,b.y+6+j*10,16,6},1,ui.text);
        else { Vector2 line[]={{b.x+4,b.y+14},{b.x+8,b.y+14},{b.x+11,b.y+7},{b.x+16,b.y+21},{b.x+20,b.y+14},{b.x+24,b.y+14}};
            for (int j=1;j<6;j++) DrawLineEx(line[j-1],line[j],1.5f,ui.text); }
    }
    draw_quiet_button(client->ui_font,chrome_help_button(),"?",true,UI_TEXT_LEAD);
    OverviewItem items[64];int waiting=0,agents=chrome_agent_items(client,items,&waiting);
    if (waiting) { DrawCircle(33,94,7,ui.waiting);char n[8];snprintf(n,sizeof(n),"%d",waiting);DrawTextEx(client->ui_font,n,(Vector2){29,89},10,0,ui.surface); }
    if (client->chrome_drawer) {
        Rectangle panel=chrome_panel();DrawRectangleRec(panel,ui.surface);DrawLine(287,40,287,GetScreenHeight(),ui.border);
        DrawTextEx(client->ui_font,client->chrome_agents ? "Agents" : "Workspaces",(Vector2){60,60},UI_TEXT_LEAD,0,ui.text);
        draw_quiet_button(client->ui_font,(Rectangle){228,54,48,28},client->chrome_pinned ? "Hide" : "Pin",true,UI_TEXT_CAPTION);
        if (client->chrome_agents) {
            draw_header_button(client->ui_font,(Rectangle){56,98,88,28},"All agents",true);
            draw_header_button(client->ui_font,(Rectangle){150,98,126,28},"Needs input",true);
            Rectangle filter=client->chrome_waiting ? (Rectangle){150,98,126,28} : (Rectangle){56,98,88,28};
            DrawRectangleRoundedLinesEx(filter,ui_roundness(filter,6),8,1,ui.accent);
        }
        if (client->chrome_keyboard || !client->chrome_pinned) {
            ui_focus_ring(client->chrome_focus,0,(Rectangle){228,54,48,28});
            if (client->chrome_agents) {
                ui_focus_ring(client->chrome_focus,1,(Rectangle){56,98,88,28});ui_focus_ring(client->chrome_focus,2,(Rectangle){150,98,126,28});
            } else ui_focus_ring(client->chrome_focus,3,(Rectangle){56,GetScreenHeight()-46,220,30});
        }
        int total=client->chrome_agents ? agents : client->count,rows=chrome_rows(client);
        if (!total) DrawTextEx(client->ui_font,client->chrome_waiting ? "No agents need input" : "No agents detected",(Vector2){60,156},UI_TEXT_CAPTION,0,ui.muted);
        for (int i=client->chrome_first;i<total && i<client->chrome_first+rows;i++) {
            Rectangle row=chrome_row(client,i-client->chrome_first);int w=client->chrome_agents ? items[i].workspace : i;
            WorkbenchPane *pane=client->chrome_agents ? client->workspaces[w].panes[items[i].pane] : NULL;
            bool selected=w==client->active && (!pane || items[i].pane==workspace->focused);
            DrawRectangleRounded(row,ui_roundness(row,8),8,selected ? ui.selected : CheckCollisionPointRec(GetMousePosition(),row) ? ui.hover : ui.surface);
            if (client->chrome_keyboard || !client->chrome_pinned) ui_focus_ring(client->chrome_focus,i-client->chrome_first+4,row);
            if (selected) DrawRectangleRounded((Rectangle){row.x+2,row.y+10,3,26},1,6,ui.accent);
            if (!pane) {
                chrome_name(client->ui_font,client->workspaces[w].name,(Rectangle){row.x+12,row.y+14,142,20},UI_TEXT_BODY,ui.text);
                bool unread=false;for (int p=0;p<client->workspaces[w].count;p++) unread|=client->workspaces[w].panes[p]->unread;
                if (unread) DrawCircle(row.x+183,row.y+row.height/2,2,ui.waiting);
                char n[8];snprintf(n,sizeof(n),"%d",client->workspaces[w].count);DrawTextEx(client->ui_font,n,(Vector2){row.x+162,row.y+15},UI_TEXT_CAPTION,0,ui.muted);
                Rectangle close=chrome_workspace_close(client,i-client->chrome_first);
                bool enabled=client->managed && !client->management_busy;
                bool hover=enabled && CheckCollisionPointRec(GetMousePosition(),close);
                if (hover) DrawRectangleRounded(close,ui_roundness(close,6),8,ui_mix(ui.surface,ui.danger,.12f));
                Color ink=hover ? ui.danger : enabled ? ui.muted : ui_mix(ui.surface,ui.muted,.35f);
                DrawLineEx((Vector2){close.x+9,close.y+9},(Vector2){close.x+17,close.y+17},1.3f,ink);
                DrawLineEx((Vector2){close.x+17,close.y+9},(Vector2){close.x+9,close.y+17},1.3f,ink);
                if (client->chrome_keyboard || !client->chrome_pinned) ui_focus_ring(client->chrome_focus,i-client->chrome_first+12,close);
            } else {
                chrome_name(client->ui_font,remote_agent_name(pane->transport.agent),(Rectangle){row.x+12,row.y+7,82,20},UI_TEXT_BODY,ui.text);
                const char *status=pane->closed ? (pane->transport.attached_elsewhere ? "Elsewhere" : pane->runtime_state==RUNTIME_STOPPED ? "Stopped" : "Unavailable") : remote_attention_name(pane->transport.attention);
                DrawTextEx(client->ui_font,status,(Vector2){row.x+100,row.y+8},UI_TEXT_CAPTION,0,pane->closed ? ui.muted : remote_attention_color(pane->transport.attention));
                char context[140];snprintf(context,sizeof(context),"%s · %s",client->workspaces[w].name,pane->name);
                chrome_name(client->ui_font,context,(Rectangle){row.x+12,row.y+27,196,16},UI_TEXT_CAPTION,ui.muted);
            }
        }
        if (!client->chrome_agents) draw_quiet_button(client->ui_font,(Rectangle){56,GetScreenHeight()-46,220,30},client->count>=8 ? "Workspace limit (8)" : "+ New workspace",client->managed && client->count<8,UI_TEXT_BODY);
    }
    const char *notice=client->forwarding ? "Next key (Esc cancels)" : clipboard_current_notice();
    if (notice && !client->chrome_drawer && !client->chrome_menu) {
        float width=MeasureTextEx(client->ui_font,notice,UI_TEXT_CAPTION,0).x+20;
        Rectangle b={fmaxf(54,(GetScreenWidth()-width)/2),GetScreenHeight()-36,width,26};
        DrawRectangleRounded(b,ui_roundness(b,8),8,ui.surface);DrawTextEx(client->ui_font,notice,(Vector2){b.x+10,b.y+7},UI_TEXT_CAPTION,0,ui.muted);
    }
    if (!client->chrome_menu && !client->dialog_open && !client->overview_open && !client->text_open && !client->theme_open && !client->help_open && !client->command_open) {
        char remove_hint[128];const char *hint=NULL;Vector2 pointer=GetMousePosition();
        if (client->chrome_drawer && !client->chrome_agents) for (int i=client->chrome_first;i<client->count && i<client->chrome_first+chrome_rows(client);i++)
            if (CheckCollisionPointRec(pointer,chrome_workspace_close(client,i-client->chrome_first))) {
                snprintf(remove_hint,sizeof(remove_hint),"Remove %s…",client->workspaces[i].name);
                hint=client->management_busy ? "A session change is still being saved" : client->managed ? remove_hint : "Workspace management requires the launcher";
            }
        if (CheckCollisionPointRec(pointer,chrome_rail_button(0))) hint="Workspaces";
        if (CheckCollisionPointRec(pointer,chrome_rail_button(1))) hint="Agents across workspaces";
        if (CheckCollisionPointRec(pointer,chrome_help_button())) hint="Shortcut help";
        if (CheckCollisionPointRec(pointer,chrome_menu_button())) hint="Workbench menu";
        if (CheckCollisionPointRec(pointer,chrome_add_button())) hint=command_unavailable(client,COMMAND_TERMINAL_CREATE) ? command_unavailable(client,COMMAND_TERMINAL_CREATE) : "New terminal";
        if (CheckCollisionPointRec(pointer,chrome_workspace_button())) hint="Choose workspace";
        for (int p=0;p<workspace->count;p++) if (CheckCollisionPointRec(pointer,terminal_position(p))) hint=workspace->panes[p]->pane_label[0] ? workspace->panes[p]->pane_label : workspace->panes[p]->name;
        if (hint) { float width=fminf(MeasureTextEx(client->ui_font,hint,UI_TEXT_CAPTION,0).x+16,fmaxf(16,GetScreenWidth()-16));Rectangle b={fminf(pointer.x+12,GetScreenWidth()-width-8),fminf(pointer.y+28,GetScreenHeight()-32),width,24};
            DrawRectangleRounded(b,ui_roundness(b,6),8,ui.raised);chrome_name(client->ui_font,hint,(Rectangle){b.x+8,b.y+6,b.width-16,16},UI_TEXT_CAPTION,ui.text); }
    }
    chrome_popup_draw(client);
}
