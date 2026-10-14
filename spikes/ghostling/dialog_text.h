/* Raylib adapter for the client-only name/path editor. */
static bool dialog_text_is_field(WorkspaceClient *client)
{ return client->dialog_focus.item==0 || (client->dialog_kind==DIALOG_WORKSPACE_CREATE && client->dialog_focus.item==1); }
static int dialog_text_index(WorkspaceClient *client)
{ return client->dialog_kind==DIALOG_WORKSPACE_DIRECTORY || client->dialog_focus.item==1 ? 1 : 0; }
static char *dialog_text_value(WorkspaceClient *client,int index)
{ return index ? client->edit_directory : client->edit_name; }
static size_t dialog_text_capacity(WorkspaceClient *client,int index)
{ return index ? sizeof(client->edit_directory) : sizeof(client->edit_name); }
static Rectangle dialog_text_bounds(WorkspaceClient *client,Rectangle bounds,bool second)
{ return (Rectangle){bounds.x+24,bounds.y+(second ? 116 : 64),second || client->dialog_kind==DIALOG_WORKSPACE_DIRECTORY ? 288 : 372,second ? 28 : 36}; }
static float dialog_text_width(WorkspaceClient *client,const char *text,size_t bytes)
{
    char prefix[1024];bytes=bytes<sizeof(prefix) ? bytes : sizeof(prefix)-1;
    memcpy(prefix,text,bytes);prefix[bytes]=0;
    return MeasureTextEx(client->ui_font,prefix,UI_TEXT_LEAD,0).x;
}
static void dialog_text_scroll(WorkspaceClient *client,int index,Rectangle field)
{
    DialogFieldState *state=&client->edit_fields[index];const char *value=dialog_text_value(client,index);
    float caret=dialog_text_width(client,value,state->selection.caret),available=field.width-20;
    if(caret-state->scroll>available) state->scroll=caret-available;
    if(caret-state->scroll<0) state->scroll=caret;
    float maximum=fmaxf(0,dialog_text_width(client,value,strlen(value))-available);
    state->scroll=fmaxf(0,fminf(state->scroll,maximum));
}
static size_t dialog_text_hit(WorkspaceClient *client,int index,Rectangle field,float x)
{
    const char *value=dialog_text_value(client,index);float target=x-field.x-8+client->edit_fields[index].scroll;
    size_t offset=0;float previous=0;
    while(value[offset]) {
        size_t next=text_edit_next(value,offset);char glyph[5];size_t bytes=next-offset;
        memcpy(glyph,value+offset,bytes);glyph[bytes]=0;
        float width=previous+MeasureTextEx(client->ui_font,glyph,UI_TEXT_LEAD,0).x;
        if(target<(previous+width)*.5f) break;
        previous=width;offset=next;
    }
    return offset;
}
static void dialog_text_mouse(WorkspaceClient *client,Rectangle bounds,bool dual)
{
    bool pressed=IsMouseButtonPressed(MOUSE_BUTTON_LEFT),down=IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    bool shift=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);Vector2 pointer=GetMousePosition();
    if(!down || (client->management_busy || client->picker_busy)) for(int i=0;i<2;i++) client->edit_fields[i].dragging=false;
    if((client->management_busy || client->picker_busy)) return;
    for(int field_index=0;field_index<(dual ? 2 : 1);field_index++) {
        int index=client->dialog_kind==DIALOG_WORKSPACE_DIRECTORY ? 1 : field_index;
        DialogFieldState *state=&client->edit_fields[index];Rectangle field=dialog_text_bounds(client,bounds,field_index==1);
        char *value=dialog_text_value(client,index);
        if(pressed && CheckCollisionPointRec(pointer,field)) {
            client->dialog_focus.item=field_index;client->dialog_focus.visible=false;
            size_t offset=dialog_text_hit(client,index,field,pointer.x);
            double now=GetTime();
            bool again=now-state->clicked<.35 && fabsf(pointer.x-state->click_position.x)<4 && fabsf(pointer.y-state->click_position.y)<4;
            state->clicks=!shift && again ? state->clicks%3+1 : 1;
            state->clicked=now;state->click_position=pointer;state->blink=now;
            if(state->clicks==3) text_edit_init(&state->selection,value,true);
            else if(state->clicks==2) text_edit_word_select(&state->selection,value,offset);
            else text_edit_position(&state->selection,offset,shift);
            state->dragging=state->clicks==1;
            client->edit_fields[1-index].dragging=false;
        } else if(down && state->dragging) {
            float edge=pointer.x<field.x+8 ? -1 : pointer.x>field.x+field.width-8 ? 1 : 0;
            float maximum=fmaxf(0,dialog_text_width(client,value,strlen(value))-(field.width-20));
            state->scroll=fmaxf(0,fminf(maximum,state->scroll+edge*240*GetFrameTime()));
            float x=fmaxf(field.x+8,fminf(field.x+field.width-8,pointer.x));
            text_edit_position(&state->selection,dialog_text_hit(client,index,field,x),true);state->blink=GetTime();
        }
        dialog_text_scroll(client,index,field);
    }
}
static void dialog_text_key(WorkspaceClient *client,int key)
{
    if((client->management_busy || client->picker_busy) || !dialog_text_is_field(client)) return;
    int index=dialog_text_index(client);char *value=dialog_text_value(client,index);size_t capacity=dialog_text_capacity(client,index);
    DialogFieldState *state=&client->edit_fields[index];TextEdit *edit=&state->selection;
    bool modifier=text_edit_modifier_down(),shift=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);
    bool word=shortcut_platform_apple() ? IsKeyDown(KEY_LEFT_ALT)||IsKeyDown(KEY_RIGHT_ALT) : modifier;
    bool changed=false;state->blink=GetTime();
    if(modifier && key==KEY_A) text_edit_init(edit,value,true);
    else if(modifier && (key==KEY_C || key==KEY_X)) {
        size_t low=text_edit_low(*edit),length=text_edit_high(*edit)-low;
        if(length) { char selection[1024];memcpy(selection,value+low,length);selection[length]=0;SetClipboardText(selection);
            if(key==KEY_X) changed=text_edit_replace(edit,value,capacity,"",0); }
    } else if(modifier && key==KEY_V) {
        const char *text=GetClipboardText();
        if(text && *text) {
            changed=text_edit_replace(edit,value,capacity,text,strnlen(text,capacity));
            if(!changed) strcpy(client->dialog_error,"Paste a single-line value that fits the field");
        }
    } else if(key==KEY_HOME || (shortcut_platform_apple() && modifier && key==KEY_LEFT)) text_edit_position(edit,0,shift);
    else if(key==KEY_END || (shortcut_platform_apple() && modifier && key==KEY_RIGHT)) text_edit_position(edit,strlen(value),shift);
    else if(key==KEY_LEFT || key==KEY_RIGHT) text_edit_move(edit,value,key==KEY_LEFT ? -1 : 1,word,shift);
    else if(key==KEY_BACKSPACE || key==KEY_DELETE) {
        if(shortcut_platform_apple() && modifier && key==KEY_BACKSPACE && !text_edit_selected(*edit)) edit->anchor=0;
        changed=text_edit_delete(edit,value,capacity,key==KEY_BACKSPACE,word);
    }
    if(changed) client->dialog_error[0]=0;
}
static void dialog_text_characters(WorkspaceClient *client)
{
    int character;
    while((character=GetCharPressed())!=0) {
        if((client->management_busy || client->picker_busy) || !dialog_text_is_field(client) || text_edit_modifier_down() || character<32 || character==127) continue;
        int index=dialog_text_index(client);DialogFieldState *state=&client->edit_fields[index];
        char bytes[4];int count=utf8_encode((uint32_t)character,bytes);
        if(count>0 && text_edit_replace(&state->selection,dialog_text_value(client,index),dialog_text_capacity(client,index),bytes,(size_t)count)) {
            client->dialog_error[0]=0;state->blink=GetTime();
        }
    }
}
static void dialog_field(WorkspaceClient *client,Rectangle field,const char *value,bool active)
{
    DialogFieldState *state=&client->edit_fields[value==client->edit_directory ? 1 : 0];TextEdit edit=state->selection;
    DrawRectangleRounded(field,.2f,8,ui.background);DrawRectangleRoundedLinesEx(field,.2f,8,1,active ? ui.accent : ui.border);
    float x=field.x+8-state->scroll,text_y=field.y+(field.height-UI_TEXT_LEAD)/2;
    BeginScissorMode((int)field.x+8,(int)field.y,(int)field.width-16,(int)field.height);
    bool selected=active && text_edit_selected(edit) && !(client->management_busy || client->picker_busy);
    float low=dialog_text_width(client,value,text_edit_low(edit)),high=dialog_text_width(client,value,text_edit_high(edit));
    if(selected) DrawRectangleRec((Rectangle){x+low,text_y-2,high-low,UI_TEXT_LEAD+4},ui_color(current_theme.selection_background));
    DrawTextEx(client->ui_font,value,(Vector2){x,text_y},UI_TEXT_LEAD,0,ui.text);
    EndScissorMode();
    if(selected) {
        /* Redraw the same shaped line clipped to its selected span. */
        float left=fmaxf(field.x+8,x+low),right=fminf(field.x+field.width-8,x+high);
        if(right>left) { BeginScissorMode((int)left,(int)field.y,(int)ceilf(right-left),(int)field.height);
            DrawTextEx(client->ui_font,value,(Vector2){x,text_y},UI_TEXT_LEAD,0,ui_color(current_theme.selection_foreground));EndScissorMode(); }
    }
    if(active && !(client->management_busy || client->picker_busy) && fmod(GetTime()-state->blink,1)<.5) {
        float caret=x+dialog_text_width(client,value,edit.caret);
        if(caret>=field.x+8 && caret<=field.x+field.width-8) DrawLine((int)caret,(int)field.y+6,(int)caret,(int)(field.y+field.height-6),ui.text);
    }
}
