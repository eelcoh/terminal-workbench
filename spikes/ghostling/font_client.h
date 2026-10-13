/* Fonts change only on an explicit setting or backing-density event. */
static bool text_font_replace(WorkspaceClient *client,FontPreferences next,float scale)
{
    if (!font_preferences_valid(next) || !isfinite(scale) || scale<.5f || scale>4) return false;
    bool logical=next.size!=client->text.size || next.family!=client->text.family;
    bool density=fabsf(scale-client->raster_scale)>=.01f;
    if (!logical && !density) return true;
    Font old_primary=client->font,old_fallback=terminal_fallback_font,old_ui=client->ui_font;
    int width=logical ? terminal_font_cell_width(next.family,next.size) : client->cell_width;
    if (width<=0) {strcpy(client->text_error,"Could not measure this font; previous settings retained");return false;}
    Font primary=terminal_font_load_family(next.family,(int)roundf(next.size*scale)),fallback=terminal_fallback_font;
    if (!primary.texture.id || primary.texture.id==GetFontDefault().texture.id || !fallback.texture.id) {
        terminal_font_unload(primary);terminal_fallback_font=old_fallback;
        strcpy(client->text_error,"Could not load this font; previous settings retained");return false;
    }
    SetTextureFilter(primary.texture,TEXTURE_FILTER_BILINEAR);
    bool replace_ui=density || old_ui.texture.id==old_primary.texture.id;
    Font interface=replace_ui ? ui_font_load(scale,primary) : old_ui;
    navigation_finish(client);client->divider_dragging=false;
    if (replace_ui) ui_font_unload(old_ui,old_primary);
    terminal_fallback_font=old_fallback;terminal_font_unload(old_primary);
    terminal_fallback_font=fallback;client->font=primary;client->ui_font=interface;
    client->text=next;client->raster_scale=scale;client->cell_width=width;client->cell_height=next.size;
    if (logical) for (int w=0;w<client->count;w++) for (int p=0;p<client->workspaces[w].count;p++) {
        WorkbenchPane *pane=client->workspaces[w].panes[p];
        pane->selection.dragging=false;pane->scrollbar_dragging=false;
        pane->effects.cell_width=width;pane->effects.cell_height=next.size;
        pane->transport.cell_width=width;pane->transport.cell_height=next.size;
        uint16_t cols=0,rows=0;
        ghostty_terminal_get(pane->terminal,GHOSTTY_TERMINAL_DATA_COLS,&cols);
        ghostty_terminal_get(pane->terminal,GHOSTTY_TERMINAL_DATA_ROWS,&rows);
        if (cols && rows) ghostty_terminal_resize(pane->terminal,cols,rows,width,next.size);
    }
    display_release_canvases(client);client->text_error[0]=0;
    fprintf(stderr,"Terminal font updated: %s, %d pt, %.3gx backing; cells %dx%d\n",font_preferences_name(next.family),next.size,scale,width,next.size);
    return true;
}
static Rectangle text_bounds(void)
{
    float width=fminf(560,GetScreenWidth()-40),height=fminf(346,GetScreenHeight()-40);
    return (Rectangle){(GetScreenWidth()-width)/2,(GetScreenHeight()-height)/2,width,height};
}
static Rectangle text_family_button(int family)
{Rectangle b=text_bounds();return (Rectangle){b.x+24+family*(b.width-48)/2,b.y+(b.height<300 ? 62 : 72),(b.width-60)/2,34};}
static Rectangle text_size_button(bool increase)
{Rectangle b=text_bounds();return (Rectangle){b.x+24+(increase ? 170 : 0),b.y+(b.height<300 ? 104 : 126),34,32};}
static Rectangle text_defaults_button(void)
{Rectangle b=text_bounds();return (Rectangle){b.x+24,b.y+b.height-48,120,32};}
static Rectangle text_cancel_button(void)
{Rectangle b=text_bounds();return (Rectangle){b.x+b.width-210,b.y+b.height-48,80,32};}
static Rectangle text_apply_button(void)
{Rectangle b=text_bounds();return (Rectangle){b.x+b.width-120,b.y+b.height-48,96,32};}
static void text_open(WorkspaceClient *client)
{
    navigation_finish(client);client->text_open=true;client->text_before=client->text;
    client->text_focus=(UiFocus){.item=0};client->text_error[0]=0;
    for (int w=0;w<client->count;w++) for (int p=0;p<client->workspaces[w].count;p++) {
        client->workspaces[w].panes[p]->selection.dragging=false;client->workspaces[w].panes[p]->scrollbar_dragging=false;
    }
}
static void text_adjust(WorkspaceClient *client,int direction)
{
    FontPreferences next=client->text;
    next.size=direction ? font_preferences_step(next.size,direction) : client->config.font.size;
    if (text_font_replace(client,next,client->raster_scale)) {
        char message[96];snprintf(message,sizeof(message),"%s · %d pt",font_preferences_name(client->text.family),client->text.size);
        clipboard_notice(message);window_view_save(client,true);
    } else clipboard_notice(client->text_error);
}
static void text_input(WorkspaceClient *client)
{
    uint32_t enabled=0x7f;
    if (client->text.size<=TERMINAL_FONT_MIN) enabled&=~ui_focus_bit(2);
    if (client->text.size>=TERMINAL_FONT_MAX) enabled&=~ui_focus_bit(3);
    ui_focus_begin(&client->text_focus,enabled,enabled);
    bool escape=false;int key;
    while ((key=GetKeyPressed())!=0) {if (ui_focus_key(&client->text_focus,key)) continue;escape|=key==KEY_ESCAPE;}
    while (GetCharPressed()!=0) { }
    FontPreferences next=client->text;
    for (int i=0;i<TERMINAL_FONT_FAMILIES;i++) if (ui_focus_hit(&client->text_focus,i,text_family_button(i))) next.family=i;
    if (ui_focus_hit(&client->text_focus,2,text_size_button(false))) next.size=font_preferences_step(next.size,-1);
    if (ui_focus_hit(&client->text_focus,3,text_size_button(true))) next.size=font_preferences_step(next.size,1);
    if (ui_focus_hit(&client->text_focus,4,text_defaults_button())) next=client->config.font;
    if (escape || ui_focus_hit(&client->text_focus,5,text_cancel_button())) {
        if (text_font_replace(client,client->text_before,client->raster_scale)) client->text_open=false;
        return;
    }
    if (!text_font_replace(client,next,client->raster_scale)) return;
    if (ui_focus_hit(&client->text_focus,6,text_apply_button())) {
        client->text_open=false;window_view_save(client,true);clipboard_notice("Terminal text saved for this window");
    }
}
static void text_draw(WorkspaceClient *client)
{
    Rectangle b=text_bounds();DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){0,0,0,75});ui_panel(b,ui.surface,true);
    DrawTextEx(client->ui_font,"Terminal text",(Vector2){b.x+24,b.y+20},UI_TEXT_DISPLAY,0,ui.text);
    DrawTextEx(client->ui_font,"Font and size for this window",(Vector2){b.x+24,b.y+50},UI_TEXT_CAPTION,0,ui.muted);
    for (int i=0;i<TERMINAL_FONT_FAMILIES;i++) {
        Rectangle choice=text_family_button(i);ui_focus_button(client->ui_font,client->text_focus,i,choice,font_preferences_name(i),true);
        if (client->text.family==i) DrawRectangleRoundedLinesEx(choice,ui_roundness(choice,6),8,1.001f,ui.accent);
    }
    ui_focus_button(client->ui_font,client->text_focus,2,text_size_button(false),"−",client->text.size>TERMINAL_FONT_MIN);
    ui_focus_button(client->ui_font,client->text_focus,3,text_size_button(true),"+",client->text.size<TERMINAL_FONT_MAX);
    char size[40];snprintf(size,sizeof(size),"%d pt",client->text.size);
    DrawTextEx(client->ui_font,size,(Vector2){b.x+87,b.y+(b.height<300 ? 112 : 134)},UI_TEXT_BODY,0,ui.text);
    DrawTextEx(client->ui_font,"Interface text stays the same size",(Vector2){b.x+244,b.y+(b.height<300 ? 113 : 135)},UI_TEXT_CAPTION,0,ui.muted);
    Rectangle sample={b.x+24,b.y+(b.height<300 ? 148 : 178),b.width-48,fmaxf(48,b.height-248)};
    DrawRectangleRounded(sample,ui_roundness(sample,8),8,ui.background);
    BeginScissorMode((int)sample.x,(int)sample.y,(int)sample.width,(int)sample.height);
    DrawTextEx(client->font,"$ git status  |  main",(Vector2){sample.x+12,sample.y+12},client->text.size,0,ui.text);
    EndScissorMode();
    if (client->text_error[0]) DrawTextEx(client->ui_font,client->text_error,(Vector2){b.x+24,b.y+b.height-70},UI_TEXT_CAPTION,0,ui.danger);
    ui_focus_button(client->ui_font,client->text_focus,4,text_defaults_button(),"Use defaults",true);
    ui_focus_button(client->ui_font,client->text_focus,5,text_cancel_button(),"Cancel",true);
    ui_focus_primary(client->ui_font,client->text_focus,6,text_apply_button(),"Apply",true);
}
