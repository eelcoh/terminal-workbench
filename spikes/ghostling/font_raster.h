/* Font raster geometry only. Logical cell sizing keeps its existing contract. */
#ifndef WORKBENCH_FONT_RASTER_H
#define WORKBENCH_FONT_RASTER_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
typedef struct { int units, ascent, descent; } FontRasterMetrics;
typedef struct { int left, bottom, width, height, offset_y; } FontRasterBox;
static inline uint16_t font_raster_u16(const unsigned char *p)
{ return (uint16_t)(((unsigned)p[0]<<8)|p[1]); }
static inline uint32_t font_raster_u32(const unsigned char *p)
{ return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static inline const unsigned char *font_raster_table(const unsigned char *data,size_t size,const char *tag,size_t minimum)
{
    if(size<12) return NULL;
    size_t count=font_raster_u16(data+4);
    if(count>(size-12)/16) return NULL;
    for(size_t i=0;i<count;i++) {
        const unsigned char *record=data+12+16*i;
        if(memcmp(record,tag,4)) continue;
        size_t offset=font_raster_u32(record+8),length=font_raster_u32(record+12);
        if(offset>size || length>size-offset || length<minimum) return NULL;
        return data+offset;
    }
    return NULL;
}
static inline bool font_raster_metrics(const unsigned char *data,size_t size,FontRasterMetrics *metrics)
{
    const unsigned char *head=font_raster_table(data,size,"head",20),*hhea=font_raster_table(data,size,"hhea",8);
    if(!head || !hhea) return false;
    int ascent=font_raster_u16(hhea+4),descent=font_raster_u16(hhea+6);
    if(ascent>=32768) ascent-=65536;
    if(descent>=32768) descent-=65536;
    FontRasterMetrics next={font_raster_u16(head+18),ascent,descent};
    if(next.units<16 || next.units>16384 || ascent<=0 || descent>0 || ascent<=descent) return false;
    *metrics=next;return true;
}
static inline double font_raster_point_size(FontRasterMetrics metrics,int pixels)
{ return (double)pixels*metrics.units/(metrics.ascent-metrics.descent); }
static inline int font_raster_baseline(FontRasterMetrics metrics,int pixels)
{ return (int)((double)pixels*metrics.ascent/(metrics.ascent-metrics.descent)); }
static inline bool font_raster_box(double x,double y,double width,double height,int pixels,int baseline,FontRasterBox *box)
{
    double maximum=(double)pixels*8;
    if(pixels<1 || pixels>512 || !isfinite(x) || !isfinite(y) || !isfinite(width) || !isfinite(height)
        || width<=0 || height<=0 || fabs(x)>maximum || fabs(y)>maximum || width>maximum || height>maximum) return false;
    int left=(int)floor(x)-1,bottom=(int)floor(y)-1,top=(int)ceil(y+height)+1,right=(int)ceil(x+width)+1;
    if(right-left>4096 || top-bottom>4096) return false;
    *box=(FontRasterBox){left,bottom,right-left,top-bottom,baseline-top};return true;
}
static inline int font_raster_utf16(uint32_t cp,uint16_t chars[2])
{
    if(cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return 0;
    if(cp<=0xffff) {chars[0]=(uint16_t)cp;return 1;}
    cp-=0x10000;chars[0]=(uint16_t)(0xd800+(cp>>10));chars[1]=(uint16_t)(0xdc00+(cp&1023));return 2;
}
#endif
