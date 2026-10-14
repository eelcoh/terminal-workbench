/* API stand-ins exercise our adapter; this is not Apple SDK/runtime validation. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#define __APPLE__ 1
typedef struct {void *data;int width,height,mipmaps,format;} Image;
typedef struct {int value,offsetX,offsetY,advanceX;Image image;} GlyphInfo;
typedef struct {unsigned id;} Texture2D;
typedef struct {int baseSize,glyphCount,glyphPadding;Texture2D texture;void *recs;GlyphInfo *glyphs;} Font;
enum {PIXELFORMAT_UNCOMPRESSED_GRAYSCALE=1,FONT_DEFAULT=0,kCTFontOrientationHorizontal=0,kCGImageAlphaOnly=1};
typedef uint16_t UniChar,CGGlyph;
typedef void *CGDataProviderRef;
typedef void *CGFontRef;
typedef void *CTFontRef;
typedef void *CGColorSpaceRef;
typedef void *CGContextRef;
typedef struct {double x,y;} CGPoint;
typedef struct {CGPoint origin;struct {double width,height;} size;} CGRect;
static const char *kCGColorSpaceLinearGray="linear";
static int resources,bitmaps,fail_context,fail_font,fail_texture,draws;
static unsigned char *coverage;
static size_t bitmap_width,bitmap_height;
static void *resource(void) {resources++;return malloc(1);}
static void release(void *p) {assert(p);resources--;free(p);}
static void *MemAlloc(unsigned n) {void *p=malloc(n);if(p)bitmaps++;return p;}
static void MemFree(void *p) {if(p){bitmaps--;free(p);}}
static void UnloadImage(Image image) {MemFree(image.data);}
static CGDataProviderRef CGDataProviderCreateWithData(void *info,const void *data,size_t size,void *callback)
{assert(!info && data && size && !callback);return resource();}
static CGFontRef CGFontCreateWithDataProvider(CGDataProviderRef p) {assert(p);return resource();}
static void CGDataProviderRelease(CGDataProviderRef p) {release(p);}
static CTFontRef CTFontCreateWithGraphicsFont(CGFontRef p,double size,void *matrix,void *attributes)
{assert(p && size==20 && !matrix && !attributes);return fail_font?NULL:resource();}
static void CGFontRelease(CGFontRef p) {release(p);}
static CGColorSpaceRef CGColorSpaceCreateWithName(const char *name) {assert(name==kCGColorSpaceLinearGray);return resource();}
static void CFRelease(CTFontRef p) {release(p);}
static void CGColorSpaceRelease(CGColorSpaceRef p) {release(p);}
static bool CTFontGetGlyphsForCharacters(CTFontRef p,const UniChar *chars,CGGlyph *ids,int count)
{assert(p);if(chars[0]=='?')return false;ids[0]=chars[0];assert(count==1 || (count==2 && chars[0]==0xd83d && chars[1]==0xde00));return true;}
static CGRect CTFontGetBoundingRectsForGlyphs(CTFontRef p,int orientation,const CGGlyph *ids,CGRect *rects,int count)
{assert(p && orientation==0 && ids && !rects && count==1);return (CGRect){{-1.2,-3.1},{8.4,ids[0]==0xd83d?25.2:12.2}};}
static CGContextRef CGBitmapContextCreate(void *data,size_t width,size_t height,size_t bits,size_t stride,CGColorSpaceRef gray,unsigned flags)
{assert(bits==8 && stride==width && gray && flags==1);if(fail_context)return NULL;coverage=data;bitmap_width=width;bitmap_height=height;return resource();}
static void CGContextRelease(CGContextRef p) {release(p);}
#define SETTER(name,expected) static void name(CGContextRef p,bool value) {assert(p && value==(expected));}
SETTER(CGContextSetAllowsAntialiasing,true)
SETTER(CGContextSetShouldAntialias,true)
SETTER(CGContextSetAllowsFontSmoothing,true)
SETTER(CGContextSetShouldSmoothFonts,false)
SETTER(CGContextSetAllowsFontSubpixelPositioning,true)
SETTER(CGContextSetShouldSubpixelPositionFonts,true)
SETTER(CGContextSetAllowsFontSubpixelQuantization,false)
SETTER(CGContextSetShouldSubpixelQuantizeFonts,false)
static void CGContextSetGrayFillColor(CGContextRef p,double gray,double alpha) {assert(p && gray==1 && alpha==1);}
static CGPoint CGPointMake(double x,double y) {return (CGPoint){x,y};}
static void CTFontDrawGlyphs(CTFontRef p,const CGGlyph *ids,const CGPoint *positions,size_t count,CGContextRef context)
{assert(p && ids && count==1 && context && positions[0].x==3 && positions[0].y==5);coverage[0]=77;coverage[bitmap_width*bitmap_height-1]=91;draws++;}
static Font GetFontDefault(void) {return (Font){.texture={999}};}
static GlyphInfo *LoadFontData(const unsigned char *data,int size,int pixels,int *points,int count,int type)
{(void)data;(void)size;assert(pixels==20 && type==FONT_DEFAULT);GlyphInfo *g=calloc((size_t)count,sizeof *g);for(int i=0;i<count;i++){g[i]=(GlyphInfo){.value=points[i],.advanceX=9,.image={MemAlloc(1),1,1,1,1}};}return g;}
static void UnloadFontData(GlyphInfo *glyphs,int count) {for(int i=0;i<count;i++)UnloadImage(glyphs[i].image);free(glyphs);}
static Image GenImageFontAtlas(GlyphInfo *g,void **recs,int count,int pixels,int padding,int method)
{assert(g && count && pixels>=20 && padding==4 && method==1);for(int i=0;i<count;i++)assert(pixels>=g[i].image.height);*recs=MemAlloc(8);return (Image){MemAlloc(8),8,1,1,1};}
static Texture2D LoadTextureFromImage(Image atlas) {assert(atlas.data);return (Texture2D){fail_texture?0:123};}
#include "platform_terminal_font.h"
static void put16(unsigned char *p,unsigned n) {p[0]=(unsigned char)(n>>8);p[1]=(unsigned char)n;}
static void put32(unsigned char *p,unsigned n) {p[0]=(unsigned char)(n>>24);p[1]=(unsigned char)(n>>16);p[2]=(unsigned char)(n>>8);p[3]=(unsigned char)n;}
int main(void)
{
    unsigned char bytes[72]={0};put16(bytes+4,2);memcpy(bytes+12,"head",4);put32(bytes+20,44);put32(bytes+24,20);
    memcpy(bytes+28,"hhea",4);put32(bytes+36,64);put32(bytes+40,8);put16(bytes+62,1000);put16(bytes+68,800);put16(bytes+70,(uint16_t)-200);
    int points[]={'M','?',0x1f600};
    Font f=platform_terminal_font_load(bytes,sizeof bytes,20,points,3);
    assert(f.texture.id==123 && draws==2 && resources==0 && bitmaps==4);
    assert(f.glyphs[0].advanceX==9 && f.glyphs[0].offsetX==-3 && f.glyphs[0].offsetY==5);
    assert(f.glyphs[0].image.width==12 && f.glyphs[0].image.height==16);
    assert(f.baseSize==20 && f.glyphs[2].image.height==29); /* Tall ink reserves atlas space, never changes cells. */
    assert(((unsigned char *)f.glyphs[0].image.data)[0]==77);
    assert(f.glyphs[1].image.width==1 && f.glyphs[1].advanceX==9); /* Missing native glyph retained. */
    MemFree(f.recs);UnloadFontData(f.glyphs,3);assert(bitmaps==0);
    fail_context=1;f=platform_terminal_font_load(bytes,sizeof bytes,20,points,3);
    assert(f.glyphs[0].image.width==1 && resources==0);MemFree(f.recs);UnloadFontData(f.glyphs,3);assert(bitmaps==0);fail_context=0;
    fail_font=1;f=platform_terminal_font_load(bytes,sizeof bytes,20,points,3);
    assert(f.glyphs[0].image.width==1 && resources==0);MemFree(f.recs);UnloadFontData(f.glyphs,3);assert(bitmaps==0);fail_font=0;
    fail_texture=1;f=platform_terminal_font_load(bytes,sizeof bytes,20,points,3);assert(f.texture.id==999 && bitmaps==0 && resources==0);
    assert(platform_terminal_font_load(bytes,sizeof bytes,0,points,3).texture.id==999);
    return 0;
}
