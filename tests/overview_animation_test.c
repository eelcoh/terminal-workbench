#include <stdbool.h>
#include <assert.h>
#include "overview_animation.h"

int main(void)
{
    OverviewAnimation animation = {0};
    assert(overview_animation_value(&animation, 100) == 0);
    overview_animation_target(&animation, 1, 100, 2);
    assert(overview_animation_value(&animation, 99) == 0);
    assert(overview_animation_busy(&animation, 100));
    float previous = 0;
    for (int step = 0; step <= 20; step++) {
        float value = overview_animation_value(&animation, 100 + step / 10.0);
        assert(value >= previous && value <= 1);
        previous = value;
    }
    assert(overview_animation_value(&animation, 101) == 0.5f);
    assert(!overview_animation_busy(&animation, 102));
    assert(overview_animation_value(&animation, 500) == 1);
    // Reverse in flight, preserving position and converging to the new endpoint.
    overview_animation_target(&animation, 0, 101, 2);
    assert(overview_animation_value(&animation, 101) == 0.5f);
    assert(overview_animation_value(&animation, 102) == 0.25f);
    overview_animation_target(&animation, 1, 102, 1);
    assert(overview_animation_value(&animation, 102) == 0.25f);
    assert(overview_animation_value(&animation, 103) == 1);
    // Reduced motion and stalled frames settle without overshoot or extra work.
    overview_animation_target(&animation, 0, 104, 0);
    assert(overview_animation_value(&animation, 104) == 0);
    assert(!overview_animation_busy(&animation, 104));
    overview_animation_target(&animation, 0, 105, 5);
    assert(!overview_animation_busy(&animation, 105));
    return 0;
}
