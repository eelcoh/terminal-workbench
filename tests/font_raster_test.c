#include <assert.h>
#include "font_raster.h"
static void put16(unsigned char *p,unsigned n) {p[0]=(unsigned char)(n>>8);p[1]=(unsigned char)n;}
static void put32(unsigned char *p,unsigned n) {p[0]=(unsigned char)(n>>24);p[1]=(unsigned char)(n>>16);p[2]=(unsigned char)(n>>8);p[3]=(unsigned char)n;}
int main(void)
{
    unsigned char data[72]={0};
    put16(data+4,2);memcpy(data+12,"head",4);put32(data+20,44);put32(data+24,20);
    memcpy(data+28,"hhea",4);put32(data+36,64);put32(data+40,8);
    put16(data+62,1000);put16(data+68,800);put16(data+70,(unsigned)(uint16_t)-200);
    FontRasterMetrics m={0};assert(font_raster_metrics(data,sizeof data,&m));
    assert(m.units==1000 && m.ascent==800 && m.descent==-200);
    assert(font_raster_point_size(m,20)==20 && font_raster_baseline(m,20)==16);
    for(size_t n=0;n<sizeof data;n++) {FontRasterMetrics old=m;assert(!font_raster_metrics(data,n,&m));assert(!memcmp(&m,&old,sizeof m));}
    put32(data+36,0xfffffff0);assert(!font_raster_metrics(data,sizeof data,&m));put32(data+36,64);
    put16(data+62,0);assert(!font_raster_metrics(data,sizeof data,&m));put16(data+62,1000);
    put16(data+70,50);assert(!font_raster_metrics(data,sizeof data,&m));
    FontRasterBox box;
    assert(font_raster_box(-1.2,-3.1,8.4,12.2,20,16,&box));
    assert(box.left==-3 && box.bottom==-5 && box.width==12 && box.height==16 && box.offset_y==5);
    FontRasterBox old=box;
    assert(!font_raster_box(0,0,0,10,20,16,&box));assert(!memcmp(&box,&old,sizeof box));
    assert(!font_raster_box(NAN,0,10,10,20,16,&box));
    assert(!font_raster_box(0,0,10,INFINITY,20,16,&box));
    assert(!font_raster_box(0,0,10,10,513,16,&box));
    assert(!font_raster_box(1000,0,10,10,20,16,&box));
    uint16_t chars[2]={0};assert(font_raster_utf16('M',chars)==1 && chars[0]=='M');
    assert(font_raster_utf16(0x1f600,chars)==2 && chars[0]==0xd83d && chars[1]==0xde00);
    assert(font_raster_utf16(0x10ffff,chars)==2 && chars[0]==0xdbff && chars[1]==0xdfff);
    assert(!font_raster_utf16(0xd800,chars) && !font_raster_utf16(0x110000,chars));
    return 0;
}
