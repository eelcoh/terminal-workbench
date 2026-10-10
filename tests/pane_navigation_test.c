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
    assert(pane_navigation_right_width(3,0,256,770)==256);
    assert(pane_navigation_right_width(.5f,0,256,770)==513);
    assert(pane_navigation_right_width(0,0,256,770)==770);
    assert(pane_navigation_right_width(0,0,770,256)==256);
    int first,last; unsigned seen=0;
    for (int frame=0;frame<=120;frame++) {
        float position=6-frame/20.0f;
        pane_navigation_visible(position,8,523,1036,&first,&last);
        assert(first>=0 && last<=8 && last-first<=3);
        for (int p=first;p<last;p++) seen|=1u<<p;
    }
    assert(seen==255); /* Every pane enters the backwards scrolling viewport. */
    pane_navigation_visible(3.5f,8,266,1036,&first,&last); assert(first==3 && last==8);
    pane_navigation_visible(0,0,523,1036,&first,&last); assert(first==0 && last==0);
    int focused=0; first=0;
    pane_navigation_focus(8,7,&focused,&first); assert(focused==7 && first==6);
    pane_navigation_focus(8,6,&focused,&first); assert(focused==6 && first==6);
    pane_navigation_focus(8,5,&focused,&first); assert(focused==5 && first==5);
    pane_navigation_focus(8,-10,&focused,&first); assert(focused==0 && first==0);
    pane_navigation_focus(8,100,&focused,&first); assert(focused==7 && first==6);
    pane_navigation_focus(1,9,&focused,&first); assert(focused==0 && first==0);
    pane_navigation_focus(0,9,&focused,&first); assert(focused==0 && first==0);
    first=-10; pane_navigation_focus(3,2,&focused,&first); assert(focused==2 && first==1);
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
