/* Pure timeline: absolute time, bounded easing and continuous retargeting. */
typedef struct { float from, target; double started, duration; } OverviewAnimation;

static float overview_animation_value(const OverviewAnimation *animation, double now)
{
    if (animation->duration <= 0) return animation->target;
    double elapsed = (now - animation->started) / animation->duration;
    if (elapsed <= 0) return animation->from;
    if (elapsed >= 1) return animation->target;
    float t = (float)elapsed;
    float ease = t * t * (3 - 2 * t);
    return animation->from + (animation->target - animation->from) * ease;
}
static void overview_animation_target(OverviewAnimation *animation, float target, double now, double duration)
{
    animation->from = overview_animation_value(animation, now);
    animation->target = target;
    animation->started = now;
    animation->duration = duration;
}
static bool overview_animation_busy(const OverviewAnimation *animation, double now)
{
    return animation->duration > 0 && animation->from != animation->target
        && now < animation->started + animation->duration;
}
