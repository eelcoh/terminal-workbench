/* Pure client navigation policy; no terminal/runtime access. */
#ifndef WORKBENCH_PANE_NAVIGATION_H
#define WORKBENCH_PANE_NAVIGATION_H
#include <stdbool.h>
#include <math.h>
typedef struct { float accumulated; double last, next; int direction; } PaneNavigationWheel;
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
