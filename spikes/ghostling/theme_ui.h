/* A client-only live appearance preview. Cancel restores the exact prior theme. */
/* Focus order: search, gallery, close, brightness filters, clear, reset, Cancel, Apply. */
static Rectangle theme_button(void) { return (Rectangle){-1000,-1000,0,0}; }
static Rectangle overview_theme_button(WorkspaceClient *client)
{
    Rectangle bounds=overview_bounds(client);
    return (Rectangle){bounds.x+224,bounds.y+14,88,24};
}
static Rectangle theme_bounds(void)
{
    float width=GetScreenWidth()-40, height=GetScreenHeight()-40;
    if (width>836) width=836;
    if (height>640) height=640;
    return (Rectangle){(GetScreenWidth()-width)/2,(GetScreenHeight()-height)/2,width,height};
}
static Rectangle theme_grid(void)
{
    Rectangle b=theme_bounds(); return (Rectangle){b.x+24,b.y+140,b.width-48,b.height-228};
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
    WorkbenchWorkspace *workspace=theme_dialog_workspace(client);
    client->theme_target_live=workspace!=NULL;client->theme_exceptions=0;
    if (workspace && client->theme_scope==1) for (int p=0;p<workspace->count;p++)
        client->theme_exceptions+=appearance_binding(client->appearance.terminals,client->appearance.terminal_count,workspace->panes[p]->id)!=NULL;
    const Theme *theme=theme_target(client);
    client->theme_selected=theme_find(client,theme);
    client->theme_has_custom=client->theme_selected==THEME_CATALOG_COUNT;
    if (client->theme_has_custom) client->theme_custom=*theme;
    client->theme_filter=0; client->theme_first=client->theme_selected/theme_columns()-theme_rows_visible()+1;
    if (client->theme_first<0) client->theme_first=0;
    client->theme_query[0]='\0'; client->theme_error[0]='\0';
}
static void theme_open_target(WorkspaceClient *client,bool terminal)
{
    theme_sync(client);client->divider_dragging=false;
    WorkbenchWorkspace *workspace=&client->workspaces[client->active];WorkbenchPane *pane=theme_focused(client);
    client->theme_focus=(UiFocus){.item=0};client->theme_open=true;
    client->appearance_before=client->appearance;client->theme_scope=terminal ? 2 : 1;client->theme_reset=false;
    snprintf(client->theme_workspace,sizeof(client->theme_workspace),"%s",workspace->id);
    snprintf(client->theme_terminal,sizeof(client->theme_terminal),"%s",terminal && pane ? pane->id : "");
    snprintf(client->theme_name,sizeof(client->theme_name),"%s",terminal && pane ? pane->name : workspace->name);
    if (!terminal && workspace->id[0]) {
        client->appearance.follow=theme_follow_policy(client);client->appearance_dirty=true;theme_sync(client);
    }
    client->theme_surface=ui; /* Dialogs keep the application surface, including fixed chrome. */
    theme_prepare_gallery(client);
    if (client->overview_open) overview_animation_target(&client->overview_animation,1,GetTime(),0);
}
static void theme_open(WorkspaceClient *client) {theme_open_target(client,false);}
static void theme_select(WorkspaceClient *client, int selected)
{
    client->theme_selected=selected;client->theme_reset=false;client->theme_error[0]='\0';
    theme_apply(client,*theme_choice(client,selected));
}
static void theme_inherit(WorkspaceClient *client)
{
    if (!theme_dialog_workspace(client)) return;
    AppearanceState *state=&client->appearance;client->theme_reset=true;
    if (client->theme_scope==1) appearance_set(state->workspaces,&state->workspace_count,APPEARANCE_WORKSPACES,client->theme_workspace,NULL);
    else appearance_set(state->terminals,&state->terminal_count,APPEARANCE_TERMINALS,client->theme_terminal,NULL);
    client->appearance_dirty=true;theme_sync(client);theme_prepare_gallery(client);
}
static Rectangle theme_search(void)
{Rectangle b=theme_bounds();return (Rectangle){b.x+24,b.y+94,b.width-276,32};}
static Rectangle theme_filter_button(int index)
{Rectangle b=theme_bounds();return (Rectangle){b.x+b.width-232+index*70,b.y+94,66,32};}
static Rectangle theme_close_button(void)
{Rectangle b=theme_bounds();return (Rectangle){b.x+b.width-58,b.y+20,34,30};}
static Rectangle theme_apply_button(void)
{Rectangle b=theme_bounds();return (Rectangle){b.x+b.width-194,b.y+b.height-48,170,32};}
static Rectangle theme_cancel_button(void)
{Rectangle b=theme_bounds();return (Rectangle){b.x+b.width-284,b.y+b.height-48,80,32};}
static void theme_input(WorkspaceClient *client)
{
    int indices[THEME_CATALOG_COUNT+2],count=theme_matches(client,indices);
    uint32_t enabled=0x3f87;
    bool live=client->theme_target_live;
    if (!live) enabled&=~(ui_focus_bit(1)|ui_focus_bit(11)|ui_focus_bit(13));
    if (!client->theme_query[0]) enabled&=~ui_focus_bit(10);
    if (!count) enabled&=~(ui_focus_bit(1)|ui_focus_bit(13));
    ui_focus_begin(&client->theme_focus,enabled,enabled&~3u);
    ui_focus_hit(&client->theme_focus,0,theme_search());
    ui_focus_hit(&client->theme_focus,1,theme_grid());
    bool escape=false,enter=false,changed=false; int direction=0,key;
    while ((key=GetKeyPressed())!=0) {
        if (ui_focus_key(&client->theme_focus,key)) continue;
        escape|=key==KEY_ESCAPE; enter|=key==KEY_ENTER;
        if (key==KEY_LEFT) direction--;
        if (key==KEY_RIGHT) direction++;
        if (key==KEY_UP) direction-=theme_columns();
        if (key==KEY_DOWN) direction+=theme_columns();
        if (client->theme_focus.item==0 && key==KEY_BACKSPACE && client->theme_query[0]) { client->theme_query[strlen(client->theme_query)-1]='\0'; changed=true; }
        if (client->theme_focus.item==0 && key==KEY_A && text_edit_modifier_down()) { client->theme_query[0]='\0'; changed=true; }
    }
    int ch;
    while ((ch=GetCharPressed())!=0) {
        size_t length=strlen(client->theme_query);
        if (client->theme_focus.item==0 && !text_edit_modifier_down() && ch>=32 && ch<127 && length+1<sizeof(client->theme_query)) {
            client->theme_query[length]=(char)ch; client->theme_query[length+1]='\0'; changed=true;
        }
    }
    if (client->theme_focus.item>1) direction=0;
    Rectangle bounds=theme_bounds(),grid=theme_grid(); Vector2 pointer=GetMousePosition();
    bool click=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Rectangle inherit={bounds.x+24,bounds.y+bounds.height-48,150,30};
    if ((client->theme_scope==1 || client->theme_scope==2) && ui_focus_hit(&client->theme_focus,11,inherit)) { theme_inherit(client); changed=true; }
    for (int i=0;i<3;i++) {
        Rectangle tab=theme_filter_button(i);
        if (ui_focus_hit(&client->theme_focus,i+7,tab) && i!=client->theme_filter) { client->theme_filter=i; changed=true; }
    }
    Rectangle search=theme_search();Rectangle clear={search.x+search.width-30,search.y+2,28,28};
    if (ui_focus_hit(&client->theme_focus,10,clear)) { client->theme_query[0]='\0'; changed=true; }
    if (changed) count=theme_matches(client,indices);
    int position=-1;
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
    Rectangle cancel=theme_cancel_button();
    Rectangle apply=theme_apply_button();
    if (escape || ui_focus_hit(&client->theme_focus,2,theme_close_button()) || ui_focus_hit(&client->theme_focus,12,cancel)) {
        client->appearance=client->appearance_before; client->appearance_dirty=true; theme_sync(client); client->theme_open=false;
    } else if (live && count && ((enter && client->theme_focus.item<=1) || ui_focus_hit(&client->theme_focus,13,apply))) {
        client->theme_error[0]=0;
        if (!client->theme_reset) theme_apply(client,*theme_choice(client,client->theme_selected));
        if (!client->theme_error[0] && theme_save(client)) { client->theme_open=false; clipboard_notice(client->theme_scope==2 ? "Terminal theme saved" : "Workspace theme saved"); }
    }
}
static void theme_draw_contents(WorkspaceClient *client)
{
    Rectangle bounds=theme_bounds(),grid=theme_grid();
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){0,0,0,75});
    ui_panel(bounds,ui.surface,true);
    bool live=client->theme_target_live;char title[160],description[192];
    const char *name=client->theme_name[0] ? client->theme_name : "workspace";
    snprintf(title,sizeof(title),"Theme for %s",name);
    if (!live) snprintf(description,sizeof(description),"This target was closed. Cancel to return.");
    else if (client->theme_scope==2) snprintf(description,sizeof(description),"Custom colors for this terminal. The workspace keeps its theme.");
    else {
        int exceptions=client->theme_exceptions;
        if (exceptions) snprintf(description,sizeof(description),"Give this workspace its own look. %d terminal%s keep%s custom colors.",exceptions,exceptions==1 ? "" : "s",exceptions==1 ? "s" : "");
        else snprintf(description,sizeof(description),"Give this workspace its own look.%s",theme_follow_policy(client) ? "" : " Chrome stays fixed by configuration.");
    }
    BeginScissorMode((int)bounds.x+24,(int)bounds.y+20,(int)bounds.width-94,64);
    DrawTextEx(client->ui_font,title,(Vector2){bounds.x+24,bounds.y+22},UI_TEXT_DISPLAY,0,ui.text);
    DrawTextEx(client->ui_font,description,(Vector2){bounds.x+24,bounds.y+57},UI_TEXT_CAPTION,0,ui.muted);
    EndScissorMode();
    Rectangle close=theme_close_button();
    ui_focus_button(client->ui_font,client->theme_focus,2,close,"",true);
    Vector2 centre={close.x+close.width/2,close.y+close.height/2};
    DrawLineEx((Vector2){centre.x-4,centre.y-4},(Vector2){centre.x+4,centre.y+4},1.5f,ui.muted);
    DrawLineEx((Vector2){centre.x-4,centre.y+4},(Vector2){centre.x+4,centre.y-4},1.5f,ui.muted);
    const char *filters[]={"All","Dark","Light"};
    for (int i=0;i<3;i++) {
        Rectangle tab=theme_filter_button(i);
        ui_focus_button(client->ui_font,client->theme_focus,i+7,tab,filters[i],true);
        if (client->theme_filter==i) DrawRectangleRoundedLinesEx(tab,.2f,8,1,ui.accent);
    }
    Rectangle search=theme_search();
    DrawRectangleRounded(search,.22f,8,ui.background);
    DrawRectangleRoundedLinesEx(search,.22f,8,1,ui.border);
    ui_focus_ring(client->theme_focus,0,search);
    BeginScissorMode((int)search.x+8,(int)search.y,(int)search.width-42,(int)search.height);
    DrawTextEx(client->ui_font,client->theme_query[0] ? client->theme_query : "Search themes",(Vector2){search.x+8,search.y+7},UI_TEXT_CAPTION,0,client->theme_query[0] ? ui.text : ui.muted);
    EndScissorMode();
    if (client->theme_query[0]) ui_focus_button(client->ui_font,client->theme_focus,10,(Rectangle){search.x+search.width-30,search.y+2,28,28},"x",true);
    int indices[THEME_CATALOG_COUNT+2],count=theme_matches(client,indices);
    BeginScissorMode((int)grid.x,(int)grid.y,(int)grid.width,(int)grid.height);
    if (!count) DrawTextEx(client->ui_font,"No palettes match / change the search or filter",(Vector2){grid.x+16,grid.y+24},UI_TEXT_BODY,0,ui.muted);
    for (int i=0;i<count;i++) {
        Rectangle card=theme_card(i,client->theme_first);
        if (card.y+card.height<=grid.y || card.y>=grid.y+grid.height) continue;
        int index=indices[i]; const Theme *theme=theme_choice(client,index);
        bool selected=index==client->theme_selected, hovered=CheckCollisionPointRec(GetMousePosition(),card);
        ui_panel(card,selected ? ui.selected : hovered ? ui.hover : ui.raised,false);
        DrawTextEx(client->ui_font,theme_catalog_name(index),(Vector2){card.x+10,card.y+(card.height<80 ? 4 : 10)},UI_TEXT_BODY,0,ui.text);
        DrawTextEx(client->ui_font,theme_light(theme) ? "Light" : "Dark",(Vector2){card.x+card.width-46,card.y+(card.height<80 ? 5 : 12)},UI_TEXT_CAPTION,0,ui.muted);
        if (card.height>=80) {
        Rectangle sample={card.x+8,card.y+34,card.width-16,card.height-42};
        DrawRectangleRounded(sample,.1f,8,ui_color(theme->background));
        DrawTextEx(client->font,"$ git status",(Vector2){sample.x+10,sample.y+8},11,0,ui_color(theme->foreground));
        DrawTextEx(client->font,"main",(Vector2){sample.x+sample.width-38,sample.y+8},10,0,ui_color(theme->palette[4]));
        if (sample.height>=60) DrawTextEx(client->font,"ready to build",(Vector2){sample.x+10,sample.y+28},10,0,ui_color(theme->palette[2]));
        float swatch=(sample.width-20)/16;
        if (sample.height>=50) for (int c=0;c<16;c++) DrawRectangleRounded((Rectangle){sample.x+10+c*swatch,sample.y+sample.height-20,swatch-2,10},.2f,4,ui_color(theme->palette[c]));
        }
        if (selected) ui_focus_ring(client->theme_focus,1,card);
        if (selected) {
            DrawRectangleRoundedLinesEx(card,ui_roundness(card,8),8,1.001f,ui.accent);
            DrawCircle((int)(card.x+card.width-13),(int)(card.y+13),3,ui.accent);
        }
    }
    EndScissorMode();
    int rows=(count+theme_columns()-1)/theme_columns();
    if (rows>theme_rows_visible()) {
        float thumb=grid.height*theme_rows_visible()/rows;
        DrawRectangleRounded((Rectangle){grid.x+grid.width+8,grid.y+client->theme_first*(grid.height-thumb)/(rows-theme_rows_visible()),3,thumb},.5f,4,ui.muted);
    }
    char preview[160];
    if (client->theme_reset) snprintf(preview,sizeof(preview),"Using %s theme",client->theme_scope==2 ? "workspace" : "default");
    else snprintf(preview,sizeof(preview),"Previewing %s",theme_catalog_name(client->theme_selected));
    const char *notice=client->theme_error[0] ? client->theme_error : preview;
    DrawTextEx(client->ui_font,notice,(Vector2){bounds.x+24,bounds.y+bounds.height-74},UI_TEXT_CAPTION,0,client->theme_error[0] ? ui.danger : ui.muted);
    char label[64]; snprintf(label,sizeof(label),"%d palette%s",count,count==1 ? "" : "s");
    bool inherits=live;
    if (inherits) ui_focus_button(client->ui_font,client->theme_focus,11,(Rectangle){bounds.x+24,bounds.y+bounds.height-48,150,30},client->theme_scope==1 ? "Use default theme" : "Use workspace",true);
    DrawTextEx(client->ui_font,label,(Vector2){bounds.x+(inherits ? 186 : 24),bounds.y+bounds.height-35},UI_TEXT_CAPTION,0,ui.muted);
    Rectangle cancel=theme_cancel_button();
    Rectangle apply=theme_apply_button();
    ui_focus_button(client->ui_font,client->theme_focus,12,cancel,"Cancel",true);
    ui_focus_primary(client->ui_font,client->theme_focus,13,apply,client->theme_scope==2 ? "Apply to terminal" : "Apply to workspace",count>0 && live);
}

static void theme_draw(WorkspaceClient *client)
{
    /* Freeze the picker tokens; surrounding workspace colors still preview live. */
    UiStyle surrounding=ui;ui=client->theme_surface;theme_draw_contents(client);ui=surrounding;
}
