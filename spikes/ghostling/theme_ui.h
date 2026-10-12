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
    Rectangle b=theme_bounds(); return (Rectangle){b.x+24,b.y+164,b.width-48,b.height-238};
}
static int theme_columns(void) { return theme_bounds().width>=820 ? 3 : 2; }
static float theme_card_height(void) { return theme_grid().height<112 ? theme_grid().height : 112; }
static int theme_rows_visible(void) { int rows=(int)((theme_grid().height+12)/(theme_card_height()+12)); return rows>0 ? rows : 1; }
static Rectangle theme_card(int position, int first)
{
    Rectangle grid=theme_grid(); int columns=theme_columns();
    float width=(grid.width-(columns-1)*12)/columns;
    return (Rectangle){grid.x+(position%columns)*(width+12),grid.y+(position/columns-first)*(theme_card_height()+12),width,theme_card_height()};
}
static const Theme *theme_choice(WorkspaceClient *client, int index)
{
    return index<THEME_CATALOG_COUNT ? &client->theme_catalog[index] : index==THEME_CATALOG_COUNT ? &client->theme_custom : &client->theme_imported;
}
static int theme_matches(WorkspaceClient *client, int *indices)
{
    int count=0;
    for (int i=0;i<THEME_CATALOG_COUNT+2;i++) {
        if (i==THEME_CATALOG_COUNT && !client->theme_has_custom) continue;
        if (i==THEME_CATALOG_COUNT+1 && (!client->theme_has_import || (client->theme_has_custom && !memcmp(&client->theme_custom,&client->theme_imported,sizeof(Theme))))) continue;
        if (theme_catalog_matches(theme_catalog_name(i),theme_choice(client,i),client->theme_query,client->theme_filter)) indices[count++]=i;
    }
    return count;
}
static int theme_find(WorkspaceClient *client, const Theme *theme)
{
    for (int i=0;i<THEME_CATALOG_COUNT;i++) if (!memcmp(theme,&client->theme_catalog[i],sizeof(Theme))) return i;
    return THEME_CATALOG_COUNT;
}
#include "appearance_client.h"
static void theme_prepare_gallery(WorkspaceClient *client)
{
    const Theme *theme=theme_target(client);
    client->theme_selected=theme_find(client,theme);
    client->theme_has_custom=client->theme_selected==THEME_CATALOG_COUNT;
    if (client->theme_has_custom) client->theme_custom=*theme;
    client->theme_filter=0; client->theme_first=client->theme_selected/theme_columns()-theme_rows_visible()+1;
    if (client->theme_first<0) client->theme_first=0;
    client->theme_query[0]='\0'; client->theme_error[0]='\0';
}
static void theme_open(WorkspaceClient *client)
{
    theme_sync(client);client->divider_dragging=false;
    client->theme_open=true; client->appearance_before=client->appearance; client->theme_scope=0;
    theme_prepare_gallery(client);
    if (client->overview_open) overview_animation_target(&client->overview_animation,1,GetTime(),0);
}
static void theme_select(WorkspaceClient *client, int selected)
{
    client->theme_selected=selected; client->theme_error[0]='\0';
    theme_apply(client,*theme_choice(client,selected));
}
static void theme_inherit(WorkspaceClient *client)
{
    AppearanceState *state=&client->appearance;
    if (client->theme_scope==1) appearance_set(state->workspaces,&state->workspace_count,APPEARANCE_WORKSPACES,theme_workspace_id(client),NULL);
    if (client->theme_scope==2) { WorkbenchPane *pane=theme_focused(client); if (pane) appearance_set(state->terminals,&state->terminal_count,APPEARANCE_TERMINALS,pane->id,NULL); }
    client->appearance_dirty=true; theme_sync(client); theme_prepare_gallery(client);
}
static Rectangle theme_scope_button(int index)
{
    Rectangle b=theme_bounds(); const int x[]={24,124,238,342}, width[]={92,106,96,114};
    return (Rectangle){b.x+x[index],b.y+82,width[index],28};
}
static Rectangle theme_follow_button(void)
{
    Rectangle b=theme_bounds(); return (Rectangle){b.x+b.width-252,b.y+82,228,28};
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
    for (int i=0;i<4;i++) {
        bool enabled=i!=1 && i!=2 ? true : i==1 ? client->count>0 : theme_focused(client)!=NULL;
        if (click && enabled && CheckCollisionPointRec(pointer,theme_scope_button(i))) { client->theme_scope=i; theme_prepare_gallery(client); changed=true; }
    }
    if (click && CheckCollisionPointRec(pointer,theme_follow_button())) {
        client->appearance.follow=!client->appearance.follow; client->appearance_dirty=true; theme_sync(client);
    }
    Rectangle inherit={bounds.x+24,bounds.y+bounds.height-48,150,30};
    if (click && (client->theme_scope==1 || client->theme_scope==2) && CheckCollisionPointRec(pointer,inherit)) { theme_inherit(client); changed=true; }
    for (int i=0;i<3;i++) {
        Rectangle tab={bounds.x+24+i*72,bounds.y+118,64,28};
        if (click && CheckCollisionPointRec(pointer,tab) && i!=client->theme_filter) { client->theme_filter=i; changed=true; }
    }
    Rectangle clear={bounds.x+bounds.width-52,bounds.y+118,28,28};
    if (click && CheckCollisionPointRec(pointer,clear)) { client->theme_query[0]='\0'; changed=true; }
    int indices[THEME_CATALOG_COUNT+2], count=theme_matches(client,indices), position=-1;
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
        client->appearance=client->appearance_before; client->appearance_dirty=true; theme_sync(client); client->theme_open=false;
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
    char target[160];
    WorkbenchWorkspace *workspace=&client->workspaces[client->active]; WorkbenchPane *focused=theme_focused(client);
    if (client->theme_scope==1) snprintf(target,sizeof(target),"Workspace: %s%s",workspace->name,appearance_binding(client->appearance.workspaces,client->appearance.workspace_count,workspace->id) ? "" : " / inherits defaults");
    else if (client->theme_scope==2) snprintf(target,sizeof(target),"Terminal: %s%s",focused ? focused->name : "none",focused && appearance_binding(client->appearance.terminals,client->appearance.terminal_count,focused->id) ? "" : " / inherits workspace");
    else snprintf(target,sizeof(target),"%s",client->theme_scope==3 ? "Default colors for terminals without a workspace theme" : "Application colors / choosing a palette keeps chrome stable");
    DrawTextEx(client->font,target,(Vector2){bounds.x+24,bounds.y+53},12,0,ui.muted);
    const char *scopes[]={"Application","Workspace","Terminal","Defaults"};
    for (int i=0;i<4;i++) {
        bool enabled=i!=1 && i!=2 ? true : i==1 ? client->count>0 : focused!=NULL;
        Rectangle tab=theme_scope_button(i); draw_header_button(client->font,tab,scopes[i],enabled);
        if (client->theme_scope==i) DrawRectangleRoundedLinesEx(tab,.2f,8,1,ui.accent);
    }
    Rectangle follow=theme_follow_button();
    draw_header_button(client->font,follow,client->appearance.follow ? "[x] Follow active workspace" : "[ ] Follow active workspace",true);
    if (client->appearance.follow) DrawRectangleRoundedLinesEx(follow,.2f,8,1,ui.accent);
    const char *filters[]={"All","Dark","Light"};
    for (int i=0;i<3;i++) {
        Rectangle tab={bounds.x+24+i*72,bounds.y+118,64,28};
        draw_header_button(client->font,tab,filters[i],true);
        if (client->theme_filter==i) DrawRectangleRoundedLinesEx(tab,.2f,8,1,ui.accent);
    }
    Rectangle search={bounds.x+252,bounds.y+118,bounds.width-276,28};
    DrawRectangleRounded(search,.22f,8,ui.background);
    DrawRectangleRoundedLinesEx(search,.22f,8,1,ui.border);
    BeginScissorMode((int)search.x+8,(int)search.y,(int)search.width-42,(int)search.height);
    DrawTextEx(client->font,client->theme_query[0] ? client->theme_query : "Type to search palettes...",(Vector2){search.x+8,search.y+7},12,0,client->theme_query[0] ? ui.text : ui.muted);
    EndScissorMode();
    if (client->theme_query[0]) draw_header_button(client->font,(Rectangle){bounds.x+bounds.width-52,bounds.y+118,28,28},"x",true);
    int indices[THEME_CATALOG_COUNT+2],count=theme_matches(client,indices);
    BeginScissorMode((int)grid.x,(int)grid.y,(int)grid.width,(int)grid.height);
    if (!count) DrawTextEx(client->font,"No palettes match / change the search or filter",(Vector2){grid.x+16,grid.y+24},13,0,ui.muted);
    for (int i=0;i<count;i++) {
        Rectangle card=theme_card(i,client->theme_first);
        if (card.y+card.height<=grid.y || card.y>=grid.y+grid.height) continue;
        int index=indices[i]; const Theme *theme=theme_choice(client,index);
        bool selected=index==client->theme_selected, hovered=CheckCollisionPointRec(GetMousePosition(),card);
        ui_panel(card,selected ? ui.selected : hovered ? ui.hover : ui.raised,false);
        DrawTextEx(client->font,theme_catalog_name(index),(Vector2){card.x+10,card.y+(card.height<80 ? 4 : 10)},13,0,ui.text);
        DrawTextEx(client->font,theme_light(theme) ? "Light" : "Dark",(Vector2){card.x+card.width-46,card.y+(card.height<80 ? 5 : 12)},10,0,ui.muted);
        if (card.height>=80) {
        Rectangle sample={card.x+8,card.y+34,card.width-16,card.height-42};
        DrawRectangleRounded(sample,.1f,8,ui_color(theme->background));
        DrawTextEx(client->font,"$ git status",(Vector2){sample.x+10,sample.y+8},11,0,ui_color(theme->foreground));
        DrawTextEx(client->font,"main",(Vector2){sample.x+sample.width-38,sample.y+8},10,0,ui_color(theme->palette[4]));
        if (sample.height>=60) DrawTextEx(client->font,"ready to build",(Vector2){sample.x+10,sample.y+28},10,0,ui_color(theme->palette[2]));
        float swatch=(sample.width-20)/16;
        if (sample.height>=50) for (int c=0;c<16;c++) DrawRectangleRounded((Rectangle){sample.x+10+c*swatch,sample.y+sample.height-20,swatch-2,10},.2f,4,ui_color(theme->palette[c]));
        }
        if (selected) DrawRectangleRoundedLinesEx(card,.06f,8,2,ui.accent);
    }
    EndScissorMode();
    int rows=(count+theme_columns()-1)/theme_columns();
    if (rows>theme_rows_visible()) {
        float thumb=grid.height*theme_rows_visible()/rows;
        DrawRectangleRounded((Rectangle){grid.x+grid.width+8,grid.y+client->theme_first*(grid.height-thumb)/(rows-theme_rows_visible()),3,thumb},.5f,4,ui.muted);
    }
    int ignored=client->theme_selected<THEME_CATALOG_COUNT ? client->theme_catalog_ignored[client->theme_selected] : client->theme_selected==THEME_CATALOG_COUNT+1 || (client->theme_has_import && !memcmp(theme_choice(client,client->theme_selected),&client->theme_imported,sizeof(Theme))) ? client->theme_ignored : 0;
    const char *notice=client->theme_error[0] ? client->theme_error : ignored ? "Color fields applied / other Ghostty options ignored" : "Arrows preview / Enter applies / Esc restores";
    DrawTextEx(client->font,notice,(Vector2){bounds.x+24,bounds.y+bounds.height-64},11,0,client->theme_error[0] ? ui.danger : ui.muted);
    char label[64]; snprintf(label,sizeof(label),"%d palette%s",count,count==1 ? "" : "s");
    bool inherits=client->theme_scope==1 || client->theme_scope==2;
    if (inherits) draw_header_button(client->font,(Rectangle){bounds.x+24,bounds.y+bounds.height-48,150,30},client->theme_scope==1 ? "Use defaults" : "Use workspace",true);
    DrawTextEx(client->font,label,(Vector2){bounds.x+(inherits ? 186 : 24),bounds.y+bounds.height-35},12,0,ui.muted);
    Rectangle cancel={bounds.x+bounds.width-208,bounds.y+bounds.height-48,80,30};
    Rectangle apply={bounds.x+bounds.width-118,bounds.y+bounds.height-48,94,30};
    draw_header_button(client->font,cancel,"Cancel",true);
    draw_header_button(client->font,apply,"Apply",count>0);
}
