/* Fractional overview positions, independent of terminals and rendering. */
#ifndef WORKBENCH_OVERVIEW_SCROLL_H
#define WORKBENCH_OVERVIEW_SCROLL_H
#include <math.h>
#include <stdbool.h>
typedef struct { float position, target; int command; double updated; } OverviewScroll;
static float overview_scroll_bound(float value, float maximum)
{
    if (value < 0) return 0;
    return value > maximum ? maximum : value;
}
static void overview_scroll_sync(OverviewScroll *scroll, int *command, float maximum, double now, bool immediate)
{
    if (maximum < 0) maximum = 0;
    if (*command != scroll->command) scroll->target = (float)*command;
    scroll->target = overview_scroll_bound(scroll->target,maximum);
    scroll->position = overview_scroll_bound(scroll->position,maximum);
    double elapsed = now-scroll->updated;
    if (immediate || elapsed >= .3) scroll->position = scroll->target;
    else if (elapsed > 0) {
        float blend = 1-expf(-(float)elapsed/.055f);
        scroll->position += (scroll->target-scroll->position)*blend;
        if (fabsf(scroll->target-scroll->position) < .0001f) scroll->position = scroll->target;
    }
    scroll->updated = now;
    *command = scroll->command = (int)floorf(scroll->target+.5f);
}
static void overview_scroll_step(OverviewScroll *scroll, int *command, int direction)
{
    float next = direction < 0 ? floorf(scroll->target) : ceilf(scroll->target);
    if (next == scroll->target) next += direction;
    scroll->target = next;
    *command = scroll->command = (int)next;
}
static void overview_scroll_delta(OverviewScroll *scroll, int *command, float delta, float stride, float maximum)
{
    if (!isfinite(delta) || !isfinite(stride) || stride <= 0) return;
    /* GLFW supplies fractional trackpad values and OS momentum already.
     * Preserve magnitude; do not add another fling or convert signs to rows. */
    float pixels = delta*40;
    if (pixels > 120) pixels = 120;
    if (pixels < -120) pixels = -120;
    scroll->target = overview_scroll_bound(scroll->target+pixels/stride,maximum > 0 ? maximum : 0);
    *command = scroll->command = (int)floorf(scroll->target+.5f);
}
#endif
