/* Presentation tokens shared by terminal chrome, overview and dialogs. */
#include "theme_core.h"
static Theme current_theme;
static const Theme *active_terminal_theme;
static const Theme *terminal_style(void) { return active_terminal_theme ? active_terminal_theme : &current_theme; }
typedef struct { Color background, surface, raised, hover, border, text, muted, accent, selected, danger, waiting, ready; } UiStyle;
static UiStyle ui;
static Color ui_color(ThemeRgb rgb) { return (Color){rgb.r,rgb.g,rgb.b,255}; }
static Color ui_mix(Color a, Color b, float amount)
{
    return (Color){(unsigned char)(a.r+(b.r-a.r)*amount), (unsigned char)(a.g+(b.g-a.g)*amount), (unsigned char)(a.b+(b.b-a.b)*amount),255};
}
static void ui_theme_set(Theme theme)
{
    current_theme = theme;
    bool light = theme_light(&theme);
    ui.background = ui_color(theme.background); ui.text = ui_color(theme.foreground);
    ui.surface = ui_mix(ui.background,ui.text,light ? .035f : .045f);
    ui.raised = ui_mix(ui.background,ui.text,light ? .06f : .085f);
    ui.hover = ui_mix(ui.background,ui.text,light ? .11f : .14f);
    ui.border = ui_mix(ui.background,ui.text,light ? .18f : .19f);
    ui.muted = ui_mix(ui.background,ui.text,.65f);
    ui.accent = ui_color(theme.cursor);
    ui.selected = ui_mix(ui.background,ui.accent,light ? .13f : .2f);
    /* Status colors are semantic, not arbitrary ANSI palette assignments. */
    ui.danger = light ? (Color){160,43,54,255} : (Color){238,158,166,255};
    ui.waiting = light ? (Color){131,86,18,255} : (Color){230,198,137,255};
    ui.ready = light ? (Color){43,112,77,255} : (Color){165,210,177,255};
}
static void ui_panel(Rectangle bounds, Color fill, bool elevated)
{
    if (elevated) {
        Rectangle shadow = {bounds.x,bounds.y+7,bounds.width,bounds.height};
        DrawRectangleRounded(shadow,.06f,10,(Color){0,0,0,25});
    }
    DrawRectangleRounded(bounds,.06f,10,fill);
    DrawRectangleRoundedLinesEx(bounds,.06f,10,1,ui.border);
}
static void ui_apply_terminal(GhosttyTerminal terminal, const Theme *theme)
{
    GhosttyColorRgb foreground = {theme->foreground.r,theme->foreground.g,theme->foreground.b};
    GhosttyColorRgb background = {theme->background.r,theme->background.g,theme->background.b};
    GhosttyColorRgb cursor = {theme->cursor.r,theme->cursor.g,theme->cursor.b};
    GhosttyColorRgb palette[256];
    for (int i=0; i<256; i++) palette[i]=(GhosttyColorRgb){theme->palette[i].r,theme->palette[i].g,theme->palette[i].b};
    ghostty_terminal_set(terminal,GHOSTTY_TERMINAL_OPT_COLOR_FOREGROUND,&foreground);
    ghostty_terminal_set(terminal,GHOSTTY_TERMINAL_OPT_COLOR_BACKGROUND,&background);
    ghostty_terminal_set(terminal,GHOSTTY_TERMINAL_OPT_COLOR_CURSOR,&cursor);
    ghostty_terminal_set(terminal,GHOSTTY_TERMINAL_OPT_COLOR_PALETTE,palette);
}
