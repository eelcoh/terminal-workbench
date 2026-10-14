/* Search owns keyboard/pointer input; VT parsing and shell lifetime continue. */
static WorkbenchPane *search_pane(WorkspaceClient *client)
{
    WorkbenchWorkspace *w=&client->workspaces[client->active];
    WorkbenchPane *pane=w->count ? w->panes[w->focused] : NULL;
    return pane && !strcmp(pane->id,client->search.id) && pane->terminal==client->search.terminal ? pane : NULL;
}
static void search_close(WorkspaceClient *client,bool restore)
{
    SearchView *s=&client->search;WorkbenchPane *pane=search_pane(client);
    if(!s->open && !s->restore) return;
    GhosttyTerminalScreen screen=GHOSTTY_TERMINAL_SCREEN_PRIMARY;
    if(pane) ghostty_terminal_get(pane->terminal,GHOSTTY_TERMINAL_DATA_ACTIVE_SCREEN,&screen);
    if(restore && pane && screen==s->screen) {
        GhosttyPointCoordinate point;
        if(s->bottom) ghostty_terminal_scroll_viewport(pane->terminal,(GhosttyTerminalScrollViewport){.tag=GHOSTTY_SCROLL_VIEWPORT_BOTTOM});
        else if(s->restore && ghostty_tracked_grid_ref_point(s->restore,GHOSTTY_POINT_TAG_SCREEN,&point)==GHOSTTY_SUCCESS)
            ghostty_terminal_scroll_viewport(pane->terminal,(GhosttyTerminalScrollViewport){.tag=GHOSTTY_SCROLL_VIEWPORT_ROW,.value.row=point.y});
    }
    if(pane) {GhosttyTerminalScrollbar bar={0};ghostty_terminal_get(pane->terminal,GHOSTTY_TERMINAL_DATA_SCROLLBAR,&bar);
        fprintf(stderr,"Terminal search closed: viewport %zu\n",bar.offset);}
    ghostty_tracked_grid_ref_free(s->restore);s->restore=NULL;s->open=false;s->terminal=NULL;
    s->paint_valid=false;
}
static void search_open(WorkspaceClient *client)
{
    WorkbenchWorkspace *w=&client->workspaces[client->active];WorkbenchPane *pane=w->count ? w->panes[w->focused] : NULL;
    if(!pane || !pane->terminal || pane->transport.replaying) return;
    if(client->search.open) search_close(client,true);
    navigation_finish(client);SearchView *s=&client->search;
    s->open=true;s->dirty=true;s->paint_valid=false;s->query[0]=s->error[0]=0;
    strcpy(s->id,pane->id);s->terminal=pane->terminal;s->edit=(TextEdit){0};s->focus=(UiFocus){.item=0};
    s->scroll=0;s->dragging=false;s->blink=s->changed=GetTime();s->revision=pane->transport.output_revision;
    ghostty_terminal_get(pane->terminal,GHOSTTY_TERMINAL_DATA_VIEWPORT_ACTIVE,&s->bottom);
    ghostty_terminal_get(pane->terminal,GHOSTTY_TERMINAL_DATA_ACTIVE_SCREEN,&s->screen);
    GhosttyPoint point={.tag=GHOSTTY_POINT_TAG_VIEWPORT,.value.coordinate={0,0}};
    ghostty_terminal_grid_ref_track(pane->terminal,point,&s->restore);
    pane->selection.dragging=pane->scrollbar_dragging=false;client->divider_dragging=false;client->forwarding=false;
}
static Rectangle search_bounds(WorkspaceClient *client)
{
    WorkbenchPane *pane=search_pane(client);
    float left=WORKBENCH_SIDEBAR+WORKBENCH_GAP,width=fminf(560,GetScreenWidth()-left-24);
    float x=pane ? pane->bounds.x+pane->bounds.width-width-8 : left+8;
    return (Rectangle){fmaxf(left+8,fminf(GetScreenWidth()-width-16,x)),88,width,72};
}
static Rectangle search_field(WorkspaceClient *client)
{Rectangle b=search_bounds(client);return (Rectangle){b.x+12,b.y+10,b.width-220,30};}
static Rectangle search_button(WorkspaceClient *client,int index)
{Rectangle b=search_bounds(client);return (Rectangle){b.x+b.width-108+(index-1)*32,b.y+10,28,30};}
static float search_text_width(WorkspaceClient *client,size_t bytes)
{
    char prefix[TERMINAL_SEARCH_QUERY];memcpy(prefix,client->search.query,bytes);prefix[bytes]=0;
    return MeasureTextEx(client->ui_font,prefix,UI_TEXT_BODY,0).x;
}
static size_t search_hit(WorkspaceClient *client,float x)
{
    float target=x-search_field(client).x-8+client->search.scroll;
    size_t offset=0;float before=0;
    while(client->search.query[offset]) {
        size_t next=text_edit_next(client->search.query,offset);float after=search_text_width(client,next);
        if(target<(before+after)/2) break;
        offset=next;before=after;
    }
    return offset;
}
static void search_reveal(WorkspaceClient *client)
{
    SearchView *s=&client->search;WorkbenchPane *pane=search_pane(client);TerminalSearch *r=&s->scan.result;
    if(!pane || s->dirty || s->scan.scanning || s->scan.failed || r->selected<0) return;
    TerminalSearchMatch match=r->matches[r->selected];
    size_t center=s->scan.rows/2,row=match.start.row>center ? match.start.row-center : 0;
    ghostty_terminal_scroll_viewport(pane->terminal,(GhosttyTerminalScrollViewport){.tag=GHOSTTY_SCROLL_VIEWPORT_ROW,.value.row=row});
    GhosttyTerminalScrollbar bar={0};ghostty_terminal_get(pane->terminal,GHOSTTY_TERMINAL_DATA_SCROLLBAR,&bar);
    fprintf(stderr,"Terminal search selected: %d/%zu; viewport %zu\n",r->selected+1,r->count,bar.offset);
    s->paint_valid=false;
}
static void search_input(WorkspaceClient *client)
{
    SearchView *s=&client->search;bool ready=!s->dirty && !s->scan.scanning && !s->scan.failed && s->scan.result.count;
    unsigned enabled=9u|(ready ? 6u : 0u);ui_focus_begin(&s->focus,enabled,enabled&~1u);
    bool changed=false,escape=false;int move=0,key;
    while((key=GetKeyPressed())!=0) {
        if(key==KEY_ESCAPE) {escape=true;continue;}
        if(key==KEY_ENTER && s->focus.item==0) {move+=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT) ? -1 : 1;continue;}
        if(ui_focus_key(&s->focus,key)) continue;
        if(key==KEY_UP || key==KEY_DOWN) {move+=key==KEY_UP ? -1 : 1;continue;}
        if(s->focus.item!=0) continue;
        TextEdit *edit=&s->edit;bool modifier=text_edit_modifier_down(),shift=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);
        bool word=shortcut_platform_apple() ? IsKeyDown(KEY_LEFT_ALT)||IsKeyDown(KEY_RIGHT_ALT) : modifier;
        if(modifier && key==KEY_A) text_edit_init(edit,s->query,true);
        else if(modifier && (key==KEY_C || key==KEY_X)) {
            size_t low=text_edit_low(*edit),length=text_edit_high(*edit)-low;
            if(length) {char text[TERMINAL_SEARCH_QUERY];memcpy(text,s->query+low,length);text[length]=0;SetClipboardText(text);
                if(key==KEY_X) changed|=text_edit_replace(edit,s->query,sizeof(s->query),"",0);}
        } else if(modifier && key==KEY_V) {
            const char *text=GetClipboardText();
            if(text && *text) {bool accepted=text_edit_replace(edit,s->query,sizeof(s->query),text,strnlen(text,sizeof(s->query)));
                changed|=accepted;if(!accepted) strcpy(s->error,"Use a single-line query of at most 255 bytes");}
        } else if(key==KEY_HOME || (shortcut_platform_apple() && modifier && key==KEY_LEFT)) text_edit_position(edit,0,shift);
        else if(key==KEY_END || (shortcut_platform_apple() && modifier && key==KEY_RIGHT)) text_edit_position(edit,strlen(s->query),shift);
        else if(key==KEY_LEFT || key==KEY_RIGHT) text_edit_move(edit,s->query,key==KEY_LEFT ? -1 : 1,word,shift);
        else if(key==KEY_BACKSPACE || key==KEY_DELETE) changed|=text_edit_delete(edit,s->query,sizeof(s->query),key==KEY_BACKSPACE,word);
        s->blink=GetTime();
    }
    if(s->focus.item==0 && (IsKeyPressedRepeat(KEY_BACKSPACE)||IsKeyPressedRepeat(KEY_DELETE)))
        changed|=text_edit_delete(&s->edit,s->query,sizeof(s->query),IsKeyPressedRepeat(KEY_BACKSPACE),false);
    int character;
    while((character=GetCharPressed())!=0) {
        if(s->focus.item!=0 || text_edit_modifier_down() || character<32 || character==127) continue;
        char bytes[4];int count=utf8_encode((uint32_t)character,bytes);
        if(count>0) changed|=text_edit_replace(&s->edit,s->query,sizeof(s->query),bytes,(size_t)count);
    }
    Rectangle field=search_field(client);Vector2 pointer=GetMousePosition();bool pressed=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool shift=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);
    if(ui_focus_hit(&s->focus,0,field)) {text_edit_position(&s->edit,search_hit(client,pointer.x),shift);s->dragging=true;s->blink=GetTime();}
    else if(s->dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) text_edit_position(&s->edit,search_hit(client,pointer.x),true);
    if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) s->dragging=false;
    if(ui_focus_hit(&s->focus,1,search_button(client,1))) move--;
    if(ui_focus_hit(&s->focus,2,search_button(client,2))) move++;
    if(ui_focus_hit(&s->focus,3,search_button(client,3))) escape=true;
    if(pressed && !CheckCollisionPointRec(pointer,search_bounds(client))) escape=true;
    if(changed) {s->dirty=true;s->paint_valid=false;s->changed=s->blink=GetTime();s->error[0]=0;ready=false;}
    if(ready && move && terminal_search_move(&s->scan.result,move>0 ? 1 : -1)) search_reveal(client);
    float caret=search_text_width(client,s->edit.caret),space=field.width-20;
    if(caret-s->scroll>space) s->scroll=caret-space;
    if(caret-s->scroll<0) s->scroll=caret;
    s->scroll=fmaxf(0,fminf(s->scroll,fmaxf(0,search_text_width(client,strlen(s->query))-space)));
    if(escape) search_close(client,true);
}
static void search_paint_prepare(WorkspaceClient *client,WorkbenchPane *pane)
{
    SearchView *s=&client->search;TerminalSearch *r=&s->scan.result;GhosttyTerminalScrollbar bar={0};
    ghostty_terminal_get(pane->terminal,GHOSTTY_TERMINAL_DATA_SCROLLBAR,&bar);
    if(s->paint_valid && s->paint_offset==bar.offset && s->paint_selected==r->selected) return;
    size_t size=(size_t)s->scan.cols*s->scan.rows;
    if(s->paint_size<size) {unsigned char *paint=realloc(s->paint,size);if(!paint) {s->paint_valid=false;return;}s->paint=paint;s->paint_size=size;}
    memset(s->paint,0,size);s->paint_offset=bar.offset;s->paint_selected=r->selected;
    for(size_t i=0;i<r->count+1;i++) {
        if(i==r->count && r->selected<0) break;
        TerminalSearchMatch m=r->matches[i==r->count ? (size_t)r->selected : i];
        size_t first=m.start.row>bar.offset ? m.start.row : bar.offset;
        size_t last=m.end.row<bar.offset+s->scan.rows-1 ? m.end.row : bar.offset+s->scan.rows-1;
        if(first>last) continue;
        for(size_t row=first;row<=last;row++) {
            size_t from=row==m.start.row ? m.start.col : 0,to=row==m.end.row ? m.end.col+m.end.width : s->scan.cols;
            if(to>s->scan.cols) to=s->scan.cols;
            if(from<to) memset(s->paint+(row-bar.offset)*s->scan.cols+from,i==r->count ? 2 : 1,to-from);
        }
    }
    s->paint_valid=true;
}
static void search_tick(WorkspaceClient *client)
{
    SearchView *s=&client->search;if(!s->open) return;WorkbenchPane *pane=search_pane(client);
    if(!pane || pane->transport.replaying) {search_close(client,false);return;}
    uint16_t cols=0,rows=0;ghostty_terminal_get(pane->terminal,GHOSTTY_TERMINAL_DATA_COLS,&cols);ghostty_terminal_get(pane->terminal,GHOSTTY_TERMINAL_DATA_ROWS,&rows);
    if(s->revision!=pane->transport.output_revision || (!s->dirty && (cols!=s->scan.cols || rows!=s->scan.rows))) {
        s->revision=pane->transport.output_revision;s->dirty=true;s->paint_valid=false;s->changed=GetTime();
    }
    if(s->dirty) {
        if(GetTime()-s->changed<.12) return;
        terminal_search_scan_begin(&s->scan,pane->terminal,s->query);s->dirty=false;s->paint_valid=false;
    }
    bool scanning=s->scan.scanning;double deadline=GetTime()+.002;size_t cells=0;
    while(s->scan.scanning && cells<4096 && GetTime()<deadline) cells+=terminal_search_scan_step(&s->scan,pane->terminal,256);
    if(scanning && !s->scan.scanning && !s->scan.failed) {
        if(s->scan.result.count) {s->scan.result.selected=(int)s->scan.result.count-1;search_reveal(client);}
        fprintf(stderr,"Terminal search: %zu%s matches, %zu rows, %u columns\n",s->scan.result.count,s->scan.result.limited ? "+" : "",s->scan.total,s->scan.cols);
    }
    if(!s->scan.scanning && !s->scan.failed) search_paint_prepare(client,pane);
}
static void search_highlight(WorkspaceClient *client,WorkbenchPane *pane,Rectangle bounds)
{
    SearchView *s=&client->search;if(!s->open || !s->paint_valid || s->dirty || s->scan.scanning || pane->terminal!=s->terminal) return;
    BeginScissorMode((int)(4*client->raster_scale),(int)(36*client->raster_scale),(int)((bounds.width-8)*client->raster_scale),(int)((bounds.height-40)*client->raster_scale));
    for(size_t row=0;row<s->scan.rows;row++) for(size_t col=0;col<s->scan.cols;) {
        unsigned char ink=s->paint[row*s->scan.cols+col];size_t start=col++;
        if(!ink) continue;
        while(col<s->scan.cols && s->paint[row*s->scan.cols+col]==ink) col++;
        Rectangle rect={4+start*client->cell_width,36+row*client->cell_height,(col-start)*client->cell_width,client->cell_height};
        Color color=ui.accent;color.a=ink==2 ? 75 : 32;DrawRectangleRec(rect,color);
        if(ink==2) DrawRectangleLinesEx(rect,1,ui.accent);
    }
    EndScissorMode();
}
static void search_draw(WorkspaceClient *client)
{
    SearchView *s=&client->search;Rectangle b=search_bounds(client),field=search_field(client);ui_panel(b,ui.surface,true);
    DrawRectangleRounded(field,ui_roundness(field,6),8,ui.background);ui_focus_ring(s->focus,0,field);
    float x=field.x+8-s->scroll,y=field.y+7;
    BeginScissorMode((int)field.x+8,(int)field.y,(int)field.width-16,(int)field.height);
    if(s->focus.item==0 && text_edit_selected(s->edit)) {
        float low=search_text_width(client,text_edit_low(s->edit)),high=search_text_width(client,text_edit_high(s->edit));
        DrawRectangleRec((Rectangle){x+low,y-2,high-low,UI_TEXT_BODY+4},ui_color(current_theme.selection_background));
    }
    DrawTextEx(client->ui_font,s->query[0] ? s->query : "Find in terminal…",(Vector2){s->query[0] ? x : field.x+8,y},UI_TEXT_BODY,0,s->query[0] ? ui.text : ui.muted);
    if(s->focus.item==0 && fmod(GetTime()-s->blink,1)<.5) DrawLine((int)(x+search_text_width(client,s->edit.caret)),(int)field.y+5,(int)(x+search_text_width(client,s->edit.caret)),(int)field.y+25,ui.text);
    EndScissorMode();
    bool ready=!s->dirty && !s->scan.scanning && !s->scan.failed && s->scan.result.count;
    ui_focus_button(client->ui_font,s->focus,1,search_button(client,1),"↑",ready);
    ui_focus_button(client->ui_font,s->focus,2,search_button(client,2),"↓",ready);
    ui_focus_button(client->ui_font,s->focus,3,search_button(client,3),"×",true);
    char count[64];
    if(s->dirty || s->scan.scanning) strcpy(count,"…");
    else snprintf(count,sizeof(count),"%d/%zu%s",s->scan.result.selected+1,s->scan.result.count,s->scan.result.limited ? "+" : "");
    DrawTextEx(client->ui_font,count,(Vector2){field.x+field.width+8,b.y+18},UI_TEXT_CAPTION,0,ui.muted);
    const char *hint=s->error[0] ? s->error : s->dirty || s->scan.scanning ? "Searching… Terminal output continues" : s->scan.failed ? "Could not read retained terminal text" : s->scan.result.limited ? "First 1024 matches · Refine your query" :
        !s->query[0] ? "Literal, case-sensitive search · Current terminal and scrollback" : !ready && !s->dirty && !s->scan.scanning ? "No matches · Enter next · Esc closes" : "Enter next · Shift+Enter previous · Esc restores your view";
    BeginScissorMode((int)b.x+12,(int)b.y+44,(int)b.width-24,24);
    DrawTextEx(client->ui_font,hint,(Vector2){b.x+12,b.y+48},UI_TEXT_CAPTION,0,ui.muted);EndScissorMode();
}
