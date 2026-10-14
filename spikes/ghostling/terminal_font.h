#pragma once
#include "terminal_font_codepoints.h"
#include "font_noto_symbols.h"
#include "font_fira_code.h"
#include "fira_font_codepoints.h"
#include "platform_terminal_font.h"

// Both atlases are loaded per window/backing density, never inside pane rendering. The
// generated Unicode lists are sorted; Raylib preserves their glyph order.
static Font terminal_fallback_font;
static int terminal_glyph_index(Font font,int cp) {
    if (font.glyphCount>=95 && font.glyphs[0].value==32 && cp>=32 && cp<=126)
        return cp-32;
    int left=0,right=font.glyphCount;
    while(left<right) {
        int mid=left+(right-left)/2,value=font.glyphs[mid].value;
        if(value<cp) left=mid+1; else right=mid;
    }
    return left<font.glyphCount && font.glyphs[left].value==cp ? left : -1;
}
static Font terminal_font_load_family(int family,int pixels) {
    const unsigned char *bytes=family ? font_fira_code : font_jetbrains_mono;
    int length=family ? sizeof(font_fira_code) : sizeof(font_jetbrains_mono);
    int *points=family ? fira_primary_codepoints : terminal_primary_codepoints;
    int count=family ? sizeof(fira_primary_codepoints)/sizeof(int) : sizeof(terminal_primary_codepoints)/sizeof(int);
    Font font=platform_terminal_font_load(bytes,length,pixels,points,count);
    points=family ? fira_fallback_codepoints : terminal_fallback_codepoints;
    count=family ? sizeof(fira_fallback_codepoints)/sizeof(int) : sizeof(terminal_fallback_codepoints)/sizeof(int);
    terminal_fallback_font=platform_terminal_font_load(font_noto_symbols,sizeof(font_noto_symbols),pixels,points,count);
    SetTextureFilter(terminal_fallback_font.texture,TEXTURE_FILTER_BILINEAR);
    return font;
}
static Font terminal_font_load(int pixels) { return terminal_font_load_family(0,pixels); }
/* Measure at logical size: backing-density rounding must never change a grid. */
static int terminal_font_cell_width(int family,int size) {
    int cp='M';
    GlyphInfo *glyph=LoadFontData(family ? font_fira_code : font_jetbrains_mono,
        family ? sizeof(font_fira_code) : sizeof(font_jetbrains_mono),size,&cp,1,FONT_DEFAULT);
    if (!glyph) return 0;
    int width=glyph[0].advanceX ? glyph[0].advanceX : glyph[0].image.width+glyph[0].offsetX;
    UnloadFontData(glyph,1);return width;
}
static void terminal_font_unload(Font font) {
    UnloadFont(terminal_fallback_font); UnloadFont(font);
    terminal_fallback_font=(Font){0};
}
static void terminal_draw_grapheme(Font primary,const uint32_t *points,uint32_t count,
    Vector2 position,float size,Color color) {
    for(uint32_t i=0;i<count;i++) {
        int cp=points[i];
        // Variation selectors and joiners affect shaping, not standalone ink.
        // Full complex-script/color-emoji shaping remains a toolkit gate.
        if(cp==0xfe0e || cp==0xfe0f || cp==0x200d) continue;
        Font font=primary;
        int index=terminal_glyph_index(font,cp);
        if(index<0) { font=terminal_fallback_font; index=terminal_glyph_index(font,cp); }
        if(index<0) { font=primary; index=terminal_glyph_index(font,'?'); }
        if(index<0 || !font.texture.id) continue;
        float scale=size/font.baseSize;
        Rectangle rec=font.recs[index]; GlyphInfo glyph=font.glyphs[index];
        float padding=font.glyphPadding;
        Rectangle source={rec.x-padding,rec.y-padding,rec.width+2*padding,rec.height+2*padding};
        Rectangle dest={position.x+(glyph.offsetX-padding)*scale,
            position.y+(glyph.offsetY-padding)*scale,source.width*scale,source.height*scale};
        DrawTexturePro(font.texture,source,dest,(Vector2){0},0,color);
        position.x+=(glyph.advanceX ? glyph.advanceX : rec.width+glyph.offsetX)*scale;
    }
}
