/* Backing pixels affect rasterization only; layout, input and PTYs remain logical. */
#ifndef WORKBENCH_PIXEL_GEOMETRY_H
#define WORKBENCH_PIXEL_GEOMETRY_H
#include <math.h>
static inline float pixel_scale(int logical, int backing)
{ return logical>0 && backing>=logical ? (float)backing/logical : 1; }
static inline int pixel_extent(float logical, float scale)
{ return (int)ceilf(fmaxf(1,logical)*scale); }
static inline float pixel_snap(float logical, float scale)
{ return roundf(logical*scale)/scale; }
#endif
