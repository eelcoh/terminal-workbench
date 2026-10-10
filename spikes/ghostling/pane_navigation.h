/* Pure client navigation policy; no terminal/runtime access. */
#ifndef WORKBENCH_PANE_NAVIGATION_H
#define WORKBENCH_PANE_NAVIGATION_H
#include <stdbool.h>
#include <math.h>
typedef struct { float accumulated; double last, next; int direction; } PaneNavigationWheel;
/* Scroll through every pane position; longer travel gets time proportional to
 * distance. Retarget from the displayed fractional position, not the old target. */
typedef struct { float distance; double duration; } PaneNavigationMotion;
static PaneNavigationMotion pane_navigation_motion(float from, float to, float stride)
{
    float steps=fabsf(to-from);
    if (!steps) return (PaneNavigationMotion){0,0};
    double duration=.27;
    if (steps>1) duration+=(steps-1)*.18;
    if (duration>1.25) duration=1.25;
    return (PaneNavigationMotion){(to-from)*stride,duration};
}
/* The right destination expands/contracts into its divider slot as it arrives,
 * so narrow-left layouts do not expose later tiles through the final right pane. */
static float pane_navigation_right_width(float position, int target, float left, float right)
{
    float weight=1-fabsf(position-target);
    if (weight<0) weight=0;
    if (weight>1) weight=1;
    return left+(right-left)*weight;
}
/* Only the intersecting strip tiles perform presentation work. */
static void pane_navigation_visible(float position, int count, float stride, float viewport, int *first, int *last)
{
    if (count<=0 || stride<=0 || viewport<=0) { *first=*last=0; return; }
    if (position<0) position=0;
    *first=(int)position;
    *last=(int)(position+viewport/stride)+1;
    if (*first>count) *first=count;
    if (*last>count) *last=count;
}
static void pane_navigation_focus(int count, int requested, int *focused, int *first)
{
    if (count <= 0) { *focused = *first = 0; return; }
    if (requested < 0) requested = 0;
    if (requested >= count) requested = count - 1;
    *focused = requested;
    if (*first > requested) *first = requested;
    if (requested >= *first + 2) *first = requested - 1;
    int maximum = count > 1 ? count - 2 : 0;
    if (*first > maximum) *first = maximum;
    if (*first < 0) *first = 0;
}
static int pane_navigation_wheel(PaneNavigationWheel *wheel, float delta, double now)
{
    if (!isfinite(delta) || delta == 0) return 0;
    int direction = delta > 0 ? 1 : -1;
    if (now - wheel->last > .25 || wheel->direction != direction) {
        wheel->accumulated = 0; wheel->next = now;
    }
    wheel->last = now; wheel->direction = direction;
    wheel->accumulated += delta;
    if (wheel->accumulated > 2) wheel->accumulated = 2;
    if (wheel->accumulated < -2) wheel->accumulated = -2;
    if (now < wheel->next || fabsf(wheel->accumulated) < .8f) return 0;
    wheel->accumulated = 0; wheel->next = now + .12;
    return direction;
}
#endif
