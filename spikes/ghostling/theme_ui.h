/* A client-only live appearance preview. Cancel restores the exact prior theme. */
static Rectangle theme_button(void) { return (Rectangle){8,GetScreenHeight()-52,168,22}; }
static Rectangle overview_theme_button(WorkspaceClient *client)
{
    Rectangle bounds=overview_bounds(client);
    return (Rectangle){bounds.x+224,bounds.y+14,88,24};
}
static Rectangle theme_bounds(void)
{
    float width=GetScreenWidth()-40, height=GetScreenHeight()-40;
    if (width>880) width=880;
    if (height>640) height=640;
    return (Rectangle){(GetScreenWidth()-width)/2,(GetScreenHeight()-height)/2,width,height};
}
static Rectangle theme_grid(void)
{
    Rectangle b=theme_bounds(); return (Rectangle){b.x+24,b.y+128,b.width-48,b.height-202};
}
static int theme_columns(void) { return theme_bounds().width>=820 ? 3 : 2; }
static int theme_rows_visible(void) { int rows=(int)((theme_grid().height+12)/124); return rows>0 ? rows : 1; }
static Rectangle theme_card(int position, int first)
{
    Rectangle grid=theme_grid(); int columns=theme_columns();
    float width=(grid.width-(columns-1)*12)/columns;
    return (Rectangle){grid.x+(position%columns)*(width+12),grid.y+(position/columns-first)*124,width,112};
}
static const Theme *theme_choice(WorkspaceClient *client, int index)
{
    return index<THEME_CATALOG_COUNT ? &client->theme_catalog[index] : &client->theme_custom;
}
static int theme_matches(WorkspaceClient *client, int *indices)
{
    int count=0, choices=THEME_CATALOG_COUNT+(client->theme_has_custom ? 1 : 0);
    for (int i=0;i<choices;i++)
        if (theme_catalog_matches(theme_catalog_name(i),theme_choice(client,i),client->theme_query,client->theme_filter)) indices[count++]=i;
    return count;
}
static int theme_find(WorkspaceClient *client, const Theme *theme)
{
    for (int i=0;i<THEME_CATALOG_COUNT;i++) if (!memcmp(theme,&client->theme_catalog[i],sizeof(Theme))) return i;
    return THEME_CATALOG_COUNT;
}
static void theme_apply(WorkspaceClient *client, Theme theme)
{
    ui_theme_set(theme);
    for (int w=0; w<client->count; w++)
        for (int p=0; p<client->workspaces[w].count; p++) ui_apply_terminal(client->workspaces[w].panes[p]->terminal);
    client->overview_epoch++;
}
static bool theme_load_path(const char *path, Theme *theme, int *ignored, char *error, size_t capacity)
{
    FILE *file=fopen(path,"r");
    if (!file) { snprintf(error,capacity,"Could not open theme: %s",strerror(errno)); return false; }
    Theme base=theme_preset(false);
    bool success=theme_read(file,&base,theme,ignored,error,capacity);
    fclose(file); return success;
}
static bool theme_initialize(WorkspaceClient *client)
{
    if (!theme_catalog_load(client->theme_catalog,client->theme_catalog_ignored,client->theme_error,sizeof(client->theme_error))) {
        fprintf(stderr,"Theme catalog: %s\n",client->theme_error); return false;
    }
    Theme theme=client->theme_catalog[0];
    const char *state=getenv("WORKBENCH_STATE_DIR");
    if (state && snprintf(client->theme_path,sizeof(client->theme_path),"%s/appearance.conf",state)>=(int)sizeof(client->theme_path)) return false;
    const char *path=getenv("WORKBENCH_THEME");
    if (!path && client->theme_path[0] && access(client->theme_path,F_OK)==0) path=client->theme_path;
    if (path) {
        if (!theme_load_path(path,&theme,&client->theme_ignored,client->theme_error,sizeof(client->theme_error))) {
            fprintf(stderr,"Theme: %s\n",client->theme_error); return false;
        }
        client->theme_has_custom=theme_find(client,&theme)==THEME_CATALOG_COUNT; client->theme_custom=theme;
    }
    ui_theme_set(theme); return true;
}
static bool theme_save(WorkspaceClient *client)
{
    if (!client->theme_path[0]) return true; /* Diagnostic clients preview in memory. */
    char temporary[1100];
    snprintf(temporary,sizeof(temporary),"%s.XXXXXX",client->theme_path);
    int fd=mkstemp(temporary);
    if (fd<0) { snprintf(client->theme_error,sizeof(client->theme_error),"Could not save appearance: %s",strerror(errno)); return false; }
    FILE *file=fdopen(fd,"w");
    bool valid=file!=NULL;
    if (file) {
        Theme t=current_theme;
        valid=fprintf(file,"# Workbench appearance; Ghostty color syntax\nbackground = #%02x%02x%02x\nforeground = #%02x%02x%02x\ncursor-color = #%02x%02x%02x\nselection-background = #%02x%02x%02x\nselection-foreground = #%02x%02x%02x\n",t.background.r,t.background.g,t.background.b,t.foreground.r,t.foreground.g,t.foreground.b,t.cursor.r,t.cursor.g,t.cursor.b,t.selection_background.r,t.selection_background.g,t.selection_background.b,t.selection_foreground.r,t.selection_foreground.g,t.selection_foreground.b)>0;
        for (int i=0;i<256 && valid;i++) valid=fprintf(file,"palette = %d=#%02x%02x%02x\n",i,t.palette[i].r,t.palette[i].g,t.palette[i].b)>0;
        if (fflush(file) || fsync(fd)) valid=false;
        if (fclose(file)) valid=false;
    } else close(fd);
    if (valid && rename(temporary,client->theme_path)) valid=false;
    if (!valid) { unlink(temporary); snprintf(client->theme_error,sizeof(client->theme_error),"Could not save appearance; retry or cancel"); }
    return valid;
}
static void theme_open(WorkspaceClient *client)
{
    client->theme_open=true; client->theme_before=current_theme;
    client->theme_selected=theme_find(client,&current_theme);
    client->theme_filter=0; client->theme_first=client->theme_selected/theme_columns()-theme_rows_visible()+1;
    if (client->theme_first<0) client->theme_first=0;
    client->theme_query[0]='\0';
    client->theme_error[0]='\0';
    if (client->overview_open) overview_animation_target(&client->overview_animation,1,GetTime(),0);
}
static void theme_select(WorkspaceClient *client, int selected)
{
    if (selected==client->theme_selected) return;
    client->theme_selected=selected; theme_apply(client,*theme_choice(client,selected));
    client->theme_error[0]='\0';
}
static void theme_input(WorkspaceClient *client)
{
    bool escape=false,enter=false,changed=false; int direction=0,key;
    while ((key=GetKeyPressed())!=0) {
        escape|=key==KEY_ESCAPE; enter|=key==KEY_ENTER;
        if (key==KEY_LEFT) direction--;
        if (key==KEY_RIGHT) direction++;
        if (key==KEY_UP) direction-=theme_columns();
        if (key==KEY_DOWN) direction+=theme_columns();
        if (key==KEY_BACKSPACE && client->theme_query[0]) { client->theme_query[strlen(client->theme_query)-1]='\0'; changed=true; }
        if (key==KEY_A && text_edit_modifier_down()) { client->theme_query[0]='\0'; changed=true; }
    }
    int ch;
    while ((ch=GetCharPressed())!=0) {
        size_t length=strlen(client->theme_query);
        if (!text_edit_modifier_down() && ch>=32 && ch<127 && length+1<sizeof(client->theme_query)) {
            client->theme_query[length]=(char)ch; client->theme_query[length+1]='\0'; changed=true;
        }
    }
    Rectangle bounds=theme_bounds(),grid=theme_grid(); Vector2 pointer=GetMousePosition();
    bool click=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    for (int i=0;i<3;i++) {
        Rectangle tab={bounds.x+24+i*72,bounds.y+82,64,28};
        if (click && CheckCollisionPointRec(pointer,tab) && i!=client->theme_filter) { client->theme_filter=i; changed=true; }
    }
    Rectangle clear={bounds.x+bounds.width-52,bounds.y+82,28,28};
    if (click && CheckCollisionPointRec(pointer,clear)) { client->theme_query[0]='\0'; changed=true; }
    int indices[THEME_CATALOG_COUNT+1], count=theme_matches(client,indices), position=-1;
    for (int i=0;i<count;i++) if (indices[i]==client->theme_selected) position=i;
    if (changed) { client->theme_first=0; if (count && position<0) { theme_select(client,indices[0]); position=0; } }
    if (direction && count) {
        position=position<0 ? 0 : position+direction;
        if (position<0) position=0;
        if (position>=count) position=count-1;
        theme_select(client,indices[position]);
    }
    int rows=(count+theme_columns()-1)/theme_columns(), limit=rows-theme_rows_visible();
    if (limit<0) limit=0;
    if ((direction || changed) && position>=0) {
        int row=position/theme_columns();
        if (row<client->theme_first) client->theme_first=row;
        if (row>=client->theme_first+theme_rows_visible()) client->theme_first=row-theme_rows_visible()+1;
    }
    float wheel=GetMouseWheelMove();
    if (CheckCollisionPointRec(pointer,grid)) {
        if (wheel>0) client->theme_first--;
        if (wheel<0) client->theme_first++;
    }
    if (client->theme_first<0) client->theme_first=0;
    if (client->theme_first>limit) client->theme_first=limit;
    for (int i=0;i<count;i++) {
        if (click && CheckCollisionPointRec(pointer,grid) && CheckCollisionPointRec(pointer,theme_card(i,client->theme_first))) theme_select(client,indices[i]);
    }
    Rectangle cancel={bounds.x+bounds.width-208,bounds.y+bounds.height-48,80,30};
    Rectangle apply={bounds.x+bounds.width-118,bounds.y+bounds.height-48,94,30};
    if (escape || (click && CheckCollisionPointRec(pointer,cancel))) {
        theme_apply(client,client->theme_before); client->theme_open=false;
    } else if (count && (enter || (click && CheckCollisionPointRec(pointer,apply)))) {
        if (theme_save(client)) { client->theme_open=false; clipboard_notice("Appearance saved"); }
    }
}
static void theme_draw(WorkspaceClient *client)
{
    Rectangle bounds=theme_bounds(),grid=theme_grid();
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){0,0,0,75});
    ui_panel(bounds,ui.surface,true);
    DrawTextEx(client->font,"Appearance",(Vector2){bounds.x+24,bounds.y+22},20,0,ui.text);
    DrawTextEx(client->font,"Find a palette / preview across your running workspaces",(Vector2){bounds.x+24,bounds.y+53},12,0,ui.muted);
    const char *filters[]={"All","Dark","Light"};
    for (int i=0;i<3;i++) {
        Rectangle tab={bounds.x+24+i*72,bounds.y+82,64,28};
        draw_header_button(client->font,tab,filters[i],true);
        if (client->theme_filter==i) DrawRectangleRoundedLinesEx(tab,.2f,8,1,ui.accent);
    }
    Rectangle search={bounds.x+252,bounds.y+82,bounds.width-276,28};
    DrawRectangleRounded(search,.22f,8,ui.background);
    DrawRectangleRoundedLinesEx(search,.22f,8,1,ui.border);
    BeginScissorMode((int)search.x+8,(int)search.y,(int)search.width-42,(int)search.height);
    DrawTextEx(client->font,client->theme_query[0] ? client->theme_query : "Type to search palettes...",(Vector2){search.x+8,search.y+7},12,0,client->theme_query[0] ? ui.text : ui.muted);
    EndScissorMode();
    if (client->theme_query[0]) draw_header_button(client->font,(Rectangle){bounds.x+bounds.width-52,bounds.y+82,28,28},"x",true);
    int indices[THEME_CATALOG_COUNT+1],count=theme_matches(client,indices);
    BeginScissorMode((int)grid.x,(int)grid.y,(int)grid.width,(int)grid.height);
    if (!count) DrawTextEx(client->font,"No palettes match / change the search or filter",(Vector2){grid.x+16,grid.y+24},13,0,ui.muted);
    for (int i=0;i<count;i++) {
        Rectangle card=theme_card(i,client->theme_first);
        if (card.y+card.height<=grid.y || card.y>=grid.y+grid.height) continue;
        int index=indices[i]; const Theme *theme=theme_choice(client,index);
        bool selected=index==client->theme_selected, hovered=CheckCollisionPointRec(GetMousePosition(),card);
        ui_panel(card,selected ? ui.selected : hovered ? ui.hover : ui.raised,false);
        DrawTextEx(client->font,theme_catalog_name(index),(Vector2){card.x+10,card.y+10},13,0,ui.text);
        DrawTextEx(client->font,theme_light(theme) ? "Light" : "Dark",(Vector2){card.x+card.width-46,card.y+12},10,0,ui.muted);
        Rectangle sample={card.x+8,card.y+34,card.width-16,70};
        DrawRectangleRounded(sample,.1f,8,ui_color(theme->background));
        DrawTextEx(client->font,"$ git status",(Vector2){sample.x+10,sample.y+8},11,0,ui_color(theme->foreground));
        DrawTextEx(client->font,"main",(Vector2){sample.x+sample.width-38,sample.y+8},10,0,ui_color(theme->palette[4]));
        DrawTextEx(client->font,"ready to build",(Vector2){sample.x+10,sample.y+28},10,0,ui_color(theme->palette[2]));
        float swatch=(sample.width-20)/16;
        for (int c=0;c<16;c++) DrawRectangleRounded((Rectangle){sample.x+10+c*swatch,sample.y+50,swatch-2,10},.2f,4,ui_color(theme->palette[c]));
        if (selected) DrawRectangleRoundedLinesEx(card,.06f,8,2,ui.accent);
    }
    EndScissorMode();
    int rows=(count+theme_columns()-1)/theme_columns();
    if (rows>theme_rows_visible()) {
        float thumb=grid.height*theme_rows_visible()/rows;
        DrawRectangleRounded((Rectangle){grid.x+grid.width+8,grid.y+client->theme_first*(grid.height-thumb)/(rows-theme_rows_visible()),3,thumb},.5f,4,ui.muted);
    }
    int ignored=client->theme_selected<THEME_CATALOG_COUNT ? client->theme_catalog_ignored[client->theme_selected] : client->theme_ignored;
    const char *notice=client->theme_error[0] ? client->theme_error : ignored ? "Color fields applied / other Ghostty options ignored" : "Arrows preview / Enter applies / Esc restores";
    DrawTextEx(client->font,notice,(Vector2){bounds.x+24,bounds.y+bounds.height-64},11,0,client->theme_error[0] ? ui.danger : ui.muted);
    char label[64]; snprintf(label,sizeof(label),"%d palette%s",count,count==1 ? "" : "s");
    DrawTextEx(client->font,label,(Vector2){bounds.x+24,bounds.y+bounds.height-35},12,0,ui.muted);
    Rectangle cancel={bounds.x+bounds.width-208,bounds.y+bounds.height-48,80,30};
    Rectangle apply={bounds.x+bounds.width-118,bounds.y+bounds.height-48,94,30};
    draw_header_button(client->font,cancel,"Cancel",true);
    draw_header_button(client->font,apply,"Apply",count>0);
}
