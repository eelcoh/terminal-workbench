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
/* GLFW already supplies fractional deltas and OS momentum. Keep the axis and
 * magnitude; vertical/diagonal-dominant events continue to the terminal. */
static inline bool pane_navigation_horizontal(float x,float y)
{ return isfinite(x) && isfinite(y) && x!=0 && fabsf(x)>fabsf(y); }
static inline float pane_navigation_pan(float scroll,float delta,float maximum)
{
    if (!isfinite(delta) || !isfinite(scroll) || maximum<=0) return isfinite(scroll) && maximum>0 ? scroll : 0;
    float pixels=fmaxf(-120,fminf(120,delta*40));
    return fmaxf(0,fminf(maximum,scroll+pixels));
}
#endif
