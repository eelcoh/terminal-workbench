/* Native Mac glyph rasterization occurs only when a window's atlas is loaded.
 * This changes ink/bitmap bearings, never the existing logical cell metrics. */
#pragma once
#include "font_raster.h"
#ifdef __APPLE__
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
static int platform_terminal_rasterize(const unsigned char *bytes,size_t size,int pixels,GlyphInfo *glyphs,int count)
{
    FontRasterMetrics metrics;
    if(!font_raster_metrics(bytes,size,&metrics)) return 0;
    CGDataProviderRef provider=CGDataProviderCreateWithData(NULL,bytes,size,NULL);
    if(!provider) return 0;
    CGFontRef graphics=CGFontCreateWithDataProvider(provider);
    CGDataProviderRelease(provider);
    if(!graphics) return 0;
    CTFontRef font=CTFontCreateWithGraphicsFont(graphics,font_raster_point_size(metrics,pixels),NULL,NULL);
    CGFontRelease(graphics);
    if(!font) return 0;
    CGColorSpaceRef gray=CGColorSpaceCreateWithName(kCGColorSpaceLinearGray);
    if(!gray) {CFRelease(font);return 0;}
    int rendered=0,baseline=font_raster_baseline(metrics,pixels);
    for(int i=0;i<count;i++) {
        UniChar chars[2];int length=font_raster_utf16((uint32_t)glyphs[i].value,chars);
        CGGlyph ids[2]={0};
        if(!length || !CTFontGetGlyphsForCharacters(font,chars,ids,length) || !ids[0]) continue;
        CGRect bounds=CTFontGetBoundingRectsForGlyphs(font,kCTFontOrientationHorizontal,ids,NULL,1);
        FontRasterBox box;
        if(!font_raster_box(bounds.origin.x,bounds.origin.y,bounds.size.width,bounds.size.height,pixels,baseline,&box)) continue;
        unsigned char *coverage=MemAlloc((unsigned)box.width*(unsigned)box.height);
        if(!coverage) continue;
        memset(coverage,0,(size_t)box.width*box.height);
        CGContextRef context=CGBitmapContextCreate(coverage,box.width,box.height,8,box.width,gray,kCGImageAlphaOnly);
        if(!context) {MemFree(coverage);continue;}
        /* Grayscale coverage, no LCD panel assumptions or synthetic thickening. */
        CGContextSetAllowsAntialiasing(context,true);CGContextSetShouldAntialias(context,true);
        CGContextSetAllowsFontSmoothing(context,true);CGContextSetShouldSmoothFonts(context,false);
        CGContextSetAllowsFontSubpixelPositioning(context,true);CGContextSetShouldSubpixelPositionFonts(context,true);
        CGContextSetAllowsFontSubpixelQuantization(context,false);CGContextSetShouldSubpixelQuantizeFonts(context,false);
        CGContextSetGrayFillColor(context,1,1);
        CGPoint origin=CGPointMake(-box.left,-box.bottom);
        CTFontDrawGlyphs(font,ids,&origin,1,context);
        CGContextRelease(context);
        UnloadImage(glyphs[i].image);
        glyphs[i].image=(Image){coverage,box.width,box.height,1,PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};
        glyphs[i].offsetX=box.left;glyphs[i].offsetY=box.offset_y;
        rendered++;
    }
    CGColorSpaceRelease(gray);CFRelease(font);return rendered;
}
static Font platform_terminal_font_load(const unsigned char *bytes,int size,int pixels,int *points,int count)
{
    if(pixels<1 || pixels>512 || count<=0 || size<=0) return GetFontDefault();
    GlyphInfo *glyphs=LoadFontData(bytes,size,pixels,points,count,FONT_DEFAULT);
    if(!glyphs) return GetFontDefault();
    int native=platform_terminal_rasterize(bytes,(size_t)size,pixels,glyphs,count);
    Font font={.baseSize=pixels,.glyphCount=count,.glyphPadding=4,.glyphs=glyphs};
    /* Native ink can extend beyond the nominal em height. Reserve its full
     * height during packing so padded accents/symbols cannot overlap rows. */
    int atlas_height=pixels;
    for(int i=0;i<count;i++) if(glyphs[i].image.height>atlas_height) atlas_height=glyphs[i].image.height;
    Image atlas=GenImageFontAtlas(glyphs,&font.recs,count,atlas_height,font.glyphPadding,1);
    if(atlas.data && font.recs) font.texture=LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    if(!font.texture.id) { if(font.recs) MemFree(font.recs);UnloadFontData(glyphs,count);return GetFontDefault(); }
    fprintf(stderr,"Terminal glyph rasterizer: CoreText %d/%d (remaining glyphs use stb), %d backing px; logical metrics retained\n",native,count,pixels);
    return font;
}
#else
static Font platform_terminal_font_load(const unsigned char *bytes,int size,int pixels,int *points,int count)
{ return LoadFontFromMemory(".ttf",bytes,size,pixels,points,count); }
#endif
