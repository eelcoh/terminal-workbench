/* Grouped, read-only help from the current binding map. No runtime mutations. */
#include "shortcut_help.h"
static Rectangle shortcut_help_bounds(void)
{
    float width=fminf(960,GetScreenWidth()-48),height=fminf(720,GetScreenHeight()-40);
    return (Rectangle){(GetScreenWidth()-width)/2,(GetScreenHeight()-height)/2,width,height};
}
static Rectangle shortcut_help_body(void)
{
    Rectangle b=shortcut_help_bounds();
    return (Rectangle){b.x+20,b.y+86,b.width-40,b.height-142};
}
static Rectangle shortcut_help_settings(void)
{
    Rectangle b=shortcut_help_bounds();
    return (Rectangle){b.x+20,b.y+b.height-40,148,26};
}
static void shortcut_help_open(WorkspaceClient *client)
{
    navigation_finish(client);shortcut_load(client);
    client->command_open=false;client->help_open=true;client->help_scroll=0;
    client->help_dirty=true;client->help_cache_failed=false;
    client->forwarding=false;client->divider_dragging=false;
    if (client->overview_open) overview_animation_target(&client->overview_animation,1,GetTime(),0);
    for (int w=0;w<client->count;w++) for (int p=0;p<client->workspaces[w].count;p++) {
        client->workspaces[w].panes[p]->selection.dragging=false;
        client->workspaces[w].panes[p]->scrollbar_dragging=false;
    }
    for (int i=0;i<COMMAND_COUNT;i++) {
        shortcut_label(client->shortcuts.bindings[i],shortcut_platform_apple(),client->help_bindings[i],sizeof(client->help_bindings[i]));
        if (!client->shortcuts.bindings[i].key) strcpy(client->help_bindings[i],"Unbound");
    }
}
static void shortcut_help_input(WorkspaceClient *client)
{
    Rectangle b=shortcut_help_bounds(),body=shortcut_help_body();
    float maximum=fmaxf(0,shortcut_help_content_height(b.width>=900)-body.height);
    float previous=client->help_scroll;
    int key;float delta=0;bool settings=false,escape=false;
    while ((key=GetKeyPressed())!=0) {
        escape|=key==KEY_ESCAPE;settings|=key==KEY_ENTER;
        if (key==KEY_UP) delta-=40;if (key==KEY_DOWN) delta+=40;
        if (key==KEY_PAGE_UP) delta-=body.height;if (key==KEY_PAGE_DOWN) delta+=body.height;
        if (key==KEY_HOME) client->help_scroll=0;if (key==KEY_END) client->help_scroll=maximum;
        if (key>=0 && key<512) workbench_key_blocked[key]=true;
    }
    while (GetCharPressed()!=0) { }
    if (IsKeyPressedRepeat(KEY_UP)) delta-=40;if (IsKeyPressedRepeat(KEY_DOWN)) delta+=40;
    Vector2 pointer=GetMousePosition();
    if (CheckCollisionPointRec(pointer,body)) delta-=GetMouseWheelMoveV().y*40;
    client->help_scroll=shortcut_help_scroll(client->help_scroll,delta,maximum);
    if (client->help_scroll!=previous) client->help_dirty=true;
    bool click=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (escape || (click && CheckCollisionPointRec(pointer,(Rectangle){b.x+b.width-84,b.y+18,64,26}))) client->help_open=false;
    else if (settings || (click && CheckCollisionPointRec(pointer,shortcut_help_settings()))) {
        command_open(client);shortcut_load(client);client->command_settings=true;
    }
}
/* Retain only the static scroll body; modal buttons still draw live hover states. */
static void shortcut_help_content(WorkspaceClient *client, Rectangle body, bool wide)
{
    BeginScissorMode((int)body.x,(int)body.y,(int)body.width,(int)body.height);
    for (int g=0;g<SHORTCUT_HELP_GROUPS;g++) {
        float width=wide ? (body.width-12)/2 : body.width;
        Rectangle card={body.x+(wide ? g%2*(width+12) : 0),body.y+shortcut_help_group_top(g,wide)-client->help_scroll,width,shortcut_help_group_height(g)};
        if (card.y+card.height<=body.y || card.y>=body.y+body.height) continue;
        DrawRectangleRounded(card,.08f,8,ui.background);
        DrawTextEx(client->font,shortcut_help_groups[g].title,(Vector2){card.x+14,card.y+12},14,0,ui.accent);
        for (int row=0;row<shortcut_help_groups[g].count;row++) {
            int action=shortcut_help_groups[g].actions[row];float y=card.y+38+row*32;
            float binding_width=fminf(card.width*.58f,MeasureTextEx(client->font,client->help_bindings[action],12,0).x+16);
            Rectangle pill={card.x+card.width-binding_width-12,y,binding_width,24};
            DrawRectangleRounded(pill,.22f,8,ui.raised);
            /* Intersect row clips with the scroll body; Raylib scissors do not nest. */
            int top=(int)fmaxf(body.y,y),bottom=(int)fminf(body.y+body.height,y+26);
            if (bottom<=top) continue;
            BeginScissorMode((int)card.x+14,top,(int)fmaxf(1,card.width-binding_width-38),bottom-top);
            DrawTextEx(client->font,command_definitions[action].title,(Vector2){card.x+14,y+5},12,0,ui.text);
            BeginScissorMode((int)pill.x+7,top,(int)pill.width-14,bottom-top);
            DrawTextEx(client->font,client->help_bindings[action],(Vector2){pill.x+8,y+5},12,0,client->shortcuts.bindings[action].key ? ui.text : ui.muted);
            BeginScissorMode((int)body.x,(int)body.y,(int)body.width,(int)body.height);
        }
    }
    EndScissorMode();
}
static void shortcut_help_prepare(WorkspaceClient *client)
{
    Rectangle body=shortcut_help_body();
    int width=(int)body.width,height=(int)body.height;
    if (client->help_canvas.id && (client->help_canvas.texture.width!=width || client->help_canvas.texture.height!=height)) {
        UnloadRenderTexture(client->help_canvas);client->help_canvas=(RenderTexture2D){0};client->help_dirty=true;
    }
    if (!client->help_canvas.id && !client->help_cache_failed) {
        client->help_canvas=LoadRenderTexture(width,height);client->help_dirty=true;
        if (!client->help_canvas.id) {
            client->help_cache_failed=true;
            fprintf(stderr,"Could not allocate shortcut help cache; using direct drawing\n");
        }
    }
    if (!client->help_canvas.id || !client->help_dirty) return;
    BeginTextureMode(client->help_canvas);ClearBackground(BLANK);
    shortcut_help_content(client,(Rectangle){0,0,width,height},shortcut_help_bounds().width>=900);
    EndTextureMode();client->help_dirty=false;
}
static void shortcut_help_draw(WorkspaceClient *client)
{
    Rectangle b=shortcut_help_bounds(),body=shortcut_help_body();bool wide=b.width>=900;
    float maximum=fmaxf(0,shortcut_help_content_height(wide)-body.height);
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){0,0,0,160});ui_panel(b,ui.surface,true);
    DrawTextEx(client->font,"Keyboard shortcuts",(Vector2){b.x+22,b.y+20},20,0,ui.text);
    DrawTextEx(client->font,"Your current bindings. Terminal actions apply in the working view.",(Vector2){b.x+22,b.y+52},13,0,ui.muted);
    draw_header_button(client->font,(Rectangle){b.x+b.width-84,b.y+18,64,26},"Done",true);
    if (client->help_canvas.id)
        DrawTextureRec(client->help_canvas.texture,(Rectangle){0,0,body.width,-body.height},(Vector2){body.x,body.y},WHITE);
    else shortcut_help_content(client,body,wide);
    if (maximum>0) {
        float thumb=fmaxf(20,body.height*body.height/(body.height+maximum));
        DrawRectangleRounded((Rectangle){body.x+body.width-3,body.y+(body.height-thumb)*client->help_scroll/maximum,3,thumb},.5f,6,ui.muted);
    }
    draw_header_button(client->font,shortcut_help_settings(),"Edit shortcuts",true);
    char hint[160];snprintf(hint,sizeof(hint),"%s%sEsc closes   Enter edits   Arrows / wheel scroll",
        client->shortcuts.bindings[COMMAND_HELP].key ? client->help_bindings[COMMAND_HELP] : "",
        client->shortcuts.bindings[COMMAND_HELP].key ? " toggles   " : "");
    BeginScissorMode((int)b.x+180,(int)b.y+(int)b.height-42,(int)b.width-200,30);
    DrawTextEx(client->font,hint,(Vector2){b.x+180,b.y+b.height-31},11,0,ui.muted);EndScissorMode();
}
