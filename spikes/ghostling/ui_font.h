/* Bundled interface face, loaded per window/backing density independently of terminal cells. */
#include "font_adwaita_sans.h"
#include "ui_font_codepoints.h"
static Font ui_font_load(float dpi, Font fallback)
{
    Font font=LoadFontFromMemory(".ttf",font_adwaita_sans,sizeof(font_adwaita_sans),
        (int)(32*fmaxf(1,dpi)),ui_font_codepoints,sizeof(ui_font_codepoints)/sizeof(int));
    if (!font.texture.id || font.texture.id==GetFontDefault().texture.id) {
        fprintf(stderr,"Could not load interface font; using terminal font for chrome\n");return fallback;
    }
    SetTextureFilter(font.texture,TEXTURE_FILTER_BILINEAR);return font;
}
static void ui_font_unload(Font font, Font terminal)
{
    if (font.texture.id && font.texture.id!=terminal.texture.id && font.texture.id!=GetFontDefault().texture.id) UnloadFont(font);
}
