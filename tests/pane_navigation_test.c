#include <assert.h>
#include "pane_navigation.h"
int main(void)
{
    PaneNavigationMotion adjacent=pane_navigation_motion(4,3,523);
    PaneNavigationMotion jump=pane_navigation_motion(6,0,523);
    assert(adjacent.distance==-523 && adjacent.duration==.27);
    assert(jump.distance==-3138 && jump.duration>1 && jump.duration<=1.25);
    assert(pane_navigation_motion(0,4,523).distance==2092);
    assert(pane_navigation_motion(2,2,523).duration==0);
    assert(pane_navigation_motion(3.5f,2,523).distance==-784.5f);
    PaneNavigationWheel wheel={0};
    assert(pane_navigation_wheel(&wheel,.3f,10)==0);
    assert(pane_navigation_wheel(&wheel,.3f,10.01)==0);
    assert(pane_navigation_wheel(&wheel,.3f,10.02)==1);
    assert(pane_navigation_wheel(&wheel,100,10.03)==0); // Momentum is bounded and throttled.
    assert(pane_navigation_wheel(&wheel,100,10.15)==1);
    assert(pane_navigation_wheel(&wheel,-1,10.16)==-1); // Direction reversal is immediate.
    assert(pane_navigation_wheel(&wheel,-.2f,11)==0); // New gesture resets residuals.
    assert(pane_navigation_wheel(&wheel,-.2f,11.01)==0);
    assert(pane_navigation_wheel(&wheel,0,12)==0);
    assert(pane_navigation_wheel(&wheel,NAN,12)==0);
    assert(pane_navigation_wheel(&wheel,INFINITY,12)==0);
    return 0;
}
