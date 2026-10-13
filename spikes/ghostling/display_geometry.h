/* Logical desktop rectangles and backing pixels are independent contracts. */
#ifndef WORKBENCH_DISPLAY_GEOMETRY_H
#define WORKBENCH_DISPLAY_GEOMETRY_H
#include <stdbool.h>
#include <math.h>
typedef struct { int x,y,width,height; } DisplayRect;
typedef struct { DisplayRect area; int left,top,right,bottom,count; } DisplayWorkarea;
static inline float display_raster_scale(int width,int height,int framebuffer_width,int framebuffer_height)
{
    if (width<=0 || height<=0 || framebuffer_width<=0 || framebuffer_height<=0) return 0;
    float x=(float)framebuffer_width/width,y=(float)framebuffer_height/height;
    /* Defer inconsistent dimensions during a resize/minimize event sequence. */
    if (fabsf(x-y)>.02f || x<.5f || x>4) return 0;
    return x;
}
static inline bool display_workarea_changed(DisplayWorkarea a,DisplayWorkarea b)
{
    return a.count!=b.count || a.area.x!=b.area.x || a.area.y!=b.area.y
        || a.area.width!=b.area.width || a.area.height!=b.area.height;
}
static inline DisplayRect display_fit_window(DisplayRect window,DisplayWorkarea work)
{
    int width=work.area.width-work.left-work.right,height=work.area.height-work.top-work.bottom;
    if (width<800 || height<300) return window;
    if (window.width>width) window.width=width;
    if (window.height>height) window.height=height;
    int left=work.area.x+work.left,top=work.area.y+work.top;
    if (window.x<left) window.x=left;
    if (window.y<top) window.y=top;
    if (window.x+window.width>left+width) window.x=left+width-window.width;
    if (window.y+window.height>top+height) window.y=top+height-window.height;
    return window;
}
static inline bool display_should_fit(DisplayWorkarea before,DisplayWorkarea after,DisplayRect original,DisplayRect fitted,bool startup)
{
    if (startup) return true;
    if (!before.count) return false;
    if (before.count!=after.count) return true;
    if (before.area.width==after.area.width && before.area.height==after.area.height) return false;
    /* Do not pull an ordinary, straddling window onto a monitor while dragging. */
    if (before.area.x!=after.area.x || before.area.y!=after.area.y)
        return original.width!=fitted.width || original.height!=fitted.height;
    return true;
}
#endif
