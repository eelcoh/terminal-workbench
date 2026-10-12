/* Pure client strip geometry. Fractions include one gap, so exact fits share
 * the available estate without accumulating rounded pixel errors. */
#ifndef WORKBENCH_PANE_WIDTH_H
#define WORKBENCH_PANE_WIDTH_H
#include <math.h>
#include <stdbool.h>
typedef struct { int preset; bool full; } PaneWidth;
static float pane_width_fraction(PaneWidth width)
{ return width.full ? 1 : width.preset==1 ? 2.0f/3 : width.preset==2 ? 1.0f/3 : .5f; }
static int pane_width_snap(float fraction)
{
    if (fraction<5.0f/12) return 2;
    if (fraction<7.0f/12) return 0;
    return 1;
}
static float pane_width_start(const PaneWidth *widths, int index, float estate, float gap)
{
    double sum=0;
    for (int p=0;p<index;p++) sum+=pane_width_fraction(widths[p]);
    return (float)round(sum*(estate+gap));
}
static float pane_width_pixels(const PaneWidth *widths, int index, float estate, float gap)
{ return pane_width_start(widths,index+1,estate,gap)-pane_width_start(widths,index,estate,gap)-gap; }
static float pane_width_reveal(const PaneWidth *widths, int count, int focus, float scroll, float estate, float gap)
{
    if (!count || estate<=0) return 0;
    float left=pane_width_start(widths,focus,estate,gap),right=left+pane_width_pixels(widths,focus,estate,gap);
    float maximum=fmaxf(0,pane_width_start(widths,count,estate,gap)-gap-estate);
    if (!isfinite(scroll)) scroll=0;
    if (left<scroll) scroll=left;
    if (right>scroll+estate) scroll=right-estate;
    return fminf(maximum,fmaxf(0,scroll));
}
static bool pane_width_visible(const PaneWidth *widths, int p, float scroll, float estate, float gap)
{
    float left=pane_width_start(widths,p,estate,gap)-scroll;
    return left<estate && left+pane_width_pixels(widths,p,estate,gap)>0;
}
#endif
