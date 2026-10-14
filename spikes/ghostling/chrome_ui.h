/* Client chrome only: names, selection and metadata already present in the client. */
static float chrome_name(Font font, const char *name, Rectangle area, int size, Color color)
{
    if (area.width<=0) return 0;
    float width=MeasureTextEx(font,name,size,0).x;
    if (width<=area.width) { DrawTextEx(font,name,(Vector2){area.x,area.y},size,0,color);return width; }
    const char *ellipsis="…";float tail=MeasureTextEx(font,ellipsis,size,0).x;
    if (tail>area.width) return 0;
    /* One bounded linear pass; no snapshots, allocations or repeated prefix measurement. */
    char label[128];size_t used=0;float advance=0,scale=(float)size/font.baseSize;
    for (const char *p=name;*p;) {
        int bytes=0,cp=GetCodepointNext(p,&bytes);
        if (bytes<=0 || used+(size_t)bytes+4>=sizeof(label)) break;
        int index=terminal_glyph_index(font,cp);
        if (index<0) index=terminal_glyph_index(font,'?');
        if (index<0) break;
        GlyphInfo glyph=font.glyphs[index];
        float step=(glyph.advanceX ? glyph.advanceX : font.recs[index].width+glyph.offsetX)*scale;
        if (advance+step+tail>area.width) break;
        memcpy(label+used,p,(size_t)bytes);used+=(size_t)bytes;p+=bytes;advance+=step;
    }
    memcpy(label+used,ellipsis,4);
    DrawTextEx(font,label,(Vector2){area.x,area.y},size,0,color);return advance+tail;
}
static void pane_header(WorkspaceClient *client, WorkbenchPane *pane, Rectangle bounds)
{
    WorkbenchWorkspace *workspace=&client->workspaces[client->active];
    bool focused=workspace->count && workspace->panes[workspace->focused]==pane;
    Color surface=focused ? ui_mix(ui.surface,ui.selected,.45f) : ui.surface;
    DrawRectangle(0,0,(int)bounds.width,WORKBENCH_HEADER_HEIGHT,surface);
    DrawLine(0,31,(int)bounds.width,31,ui_mix(surface,ui.border,.45f));
    if (focused) DrawRectangleRounded((Rectangle){0,9,3,14},.8f,6,ui.accent);
    else if (pane->unread) DrawCircle(5,16,2,ui.waiting);
    const char *agent=pane->closed ? "" : remote_agent_name(pane->transport.agent);
    const char *status=pane->closed ? (client->managed && pane->runtime_state==RUNTIME_STOPPED ? "Stopped" : client->managed && pane->runtime_state==RUNTIME_CHECKING ? "Checking" :
        client->managed && pane->runtime_state==RUNTIME_UNAVAILABLE ? "Unavailable" : pane->transport.attached_elsewhere ? "Elsewhere" : "Disconnected") :
        pane->transport.replaying ? "Connecting" : agent[0] ? remote_attention_name(pane->transport.attention) : "";
    Color status_color=pane->closed || pane->transport.replaying ? ui.muted : remote_attention_color(pane->transport.attention);
    int reserve=pane->closed ? (pane->runtime_state==RUNTIME_STOPPED ? 172 : 132) : 38;
    float right=fmaxf(12,bounds.width-reserve),space=right-12;
    float status_width=status[0] ? MeasureTextEx(client->ui_font,status,UI_TEXT_CAPTION,0).x+14 : 0;
    bool badge=status_width>0 && space>=status_width+44;
    if (badge) {
        Rectangle pill={right-status_width,6,status_width,20};
        DrawRectangleRounded(pill,ui_roundness(pill,5),8,ui_mix(surface,status_color,.10f));
        DrawTextEx(client->ui_font,status,(Vector2){pill.x+7,10},UI_TEXT_CAPTION,0,status_color);
        space-=status_width+8;
    }
    float agent_width=agent[0] ? MeasureTextEx(client->ui_font,agent,UI_TEXT_CAPTION,0).x : 0;
    bool show_agent=agent_width>0 && space>=agent_width+12+44;
    float name_space=space-(show_agent ? agent_width+12 : 0);
    float name_width=chrome_name(client->ui_font,pane->pane_label[0] ? pane->pane_label : pane->name,(Rectangle){12,8,name_space,20},UI_TEXT_BODY,focused ? ui.text : ui_mix(ui.surface,ui.text,.85f));
    if (show_agent)
        DrawTextEx(client->ui_font,agent,(Vector2){12+name_width+12,10},UI_TEXT_CAPTION,0,ui.muted);
    if (pane->closed) {
        Rectangle action=pane->runtime_state==RUNTIME_STOPPED ? (Rectangle){bounds.width-164,6,126,20}
            : terminal_move_here_bounds((Rectangle){0,0,bounds.width,bounds.height});
        const char *label=client->managed && pane->runtime_state==RUNTIME_STOPPED ? "Start new shell" :
            client->managed && pane->runtime_state==RUNTIME_UNAVAILABLE ? "Check again" :
            client->managed && pane->runtime_state==RUNTIME_CHECKING ? "Checking..." :
            pane->transport.takeover_unsupported ? "Reconnect" : "Move here";
        draw_header_button(client->ui_font,action,label,!client->managed || pane->runtime_state!=RUNTIME_CHECKING);
    }
    Rectangle overflow={bounds.width-30,4,24,24};
    draw_quiet_button(client->ui_font,overflow,"",true,UI_TEXT_BODY);
    for (int i=0;i<3;i++) DrawCircle(overflow.x+7+i*5,16,1.2f,ui.muted);
}
