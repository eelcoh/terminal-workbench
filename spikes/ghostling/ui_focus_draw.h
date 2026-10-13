/* Raylib adapter: consume traversal/activation before text or terminal routing. */
static bool ui_focus_key(UiFocus *focus, int key)
{
    if (key>=0 && key<512) workbench_key_blocked[key]=true;
    if (key==KEY_TAB && !(shortcut_modifiers() & ~2u)) { ui_focus_step(focus,IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT));return true; }
    if ((key==KEY_ENTER || key==KEY_SPACE) && !shortcut_modifiers()) return ui_focus_activate(focus);
    return false;
}
static void ui_focus_ring(UiFocus focus, int item, Rectangle bounds)
{
    if (focus.visible && focus.item==item && (focus.enabled & ui_focus_bit(item)))
        DrawRectangleRoundedLinesEx((Rectangle){bounds.x-2,bounds.y-2,bounds.width+4,bounds.height+4},.2f,8,2,ui.accent);
}
static bool ui_focus_hit(UiFocus *focus, int item, Rectangle bounds)
{
    if (!(focus->enabled & ui_focus_bit(item))) return false;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(),bounds)) {
        focus->item=item;focus->visible=false;return true;
    }
    return focus->activated==item;
}
static void ui_focus_button(Font font, UiFocus focus, int item, Rectangle bounds, const char *label, bool enabled)
{
    draw_header_button(font,bounds,label,enabled);ui_focus_ring(focus,item,bounds);
}

static void ui_focus_action(Font font, UiFocus focus, int item, Rectangle bounds, const char *label, bool enabled, bool destructive)
{
    draw_header_button(font,bounds,label,enabled);
    if (enabled) {
        Color color=destructive ? ui.danger : ui.accent;
        DrawRectangleRoundedLinesEx(bounds,ui_roundness(bounds,6),8,1,ui_mix(ui.surface,color,.65f));
    }
    ui_focus_ring(focus,item,bounds);
}

/* Filled primary action, with the existing hover/press/focus behavior. */
static void ui_focus_primary(Font font,UiFocus focus,int item,Rectangle bounds,const char *label,bool enabled)
{
    UiStyle original=ui;
    if (enabled) {
        Color ink=ui.accent.r*.2126f+ui.accent.g*.7152f+ui.accent.b*.0722f>140
            ? (Color){20,27,34,255} : (Color){246,248,250,255};
        ui.raised=ui.accent;ui.hover=ui_mix(ui.accent,ink,.10f);ui.selected=ui_mix(ui.accent,ink,.18f);ui.text=ink;
    }
    draw_header_button(font,bounds,label,enabled);ui=original;ui_focus_ring(focus,item,bounds);
}
