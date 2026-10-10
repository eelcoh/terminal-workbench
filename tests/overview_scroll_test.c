#include <assert.h>
#include <stdbool.h>
#include "overview_scroll.h"
int main(void)
{
    OverviewScroll scroll={0}; int first=0;
    overview_scroll_delta(&scroll,&first,.1f,250,6);
    assert(fabsf(scroll.target-.016f)<.00001f && first==0);
    overview_scroll_step(&scroll,&first,-1);
    assert(scroll.target==0 && first==0); // buttons clear a partial offset
    overview_scroll_delta(&scroll,&first,.1f,250,6);
    overview_scroll_sync(&scroll,&first,6,.016,false);
    assert(scroll.position>0 && scroll.position<scroll.target);
    overview_scroll_delta(&scroll,&first,-.1f,250,6);
    assert(scroll.target==0); // immediate reversal, no extra fling
    overview_scroll_sync(&scroll,&first,6,.032,true);
    assert(scroll.position==0);
    overview_scroll_delta(&scroll,&first,-2,250,6);
    overview_scroll_delta(&scroll,&first,.1f,250,6);
    assert(scroll.target>0); // no accumulated debt at either boundary
    first=3;
    overview_scroll_sync(&scroll,&first,6,.048,false);
    assert(scroll.position>0 && scroll.position<3 && scroll.target==3);
    overview_scroll_sync(&scroll,&first,1,.064,true);
    assert(first==1 && scroll.position==1 && scroll.target==1);
    overview_scroll_delta(&scroll,&first,NAN,250,6);
    assert(scroll.target==1);
    overview_scroll_delta(&scroll,&first,10000,250,6);
    assert(scroll.target<1.5f); // bounded burst, not a full-content jump
    OverviewScroll sixty={.target=2,.command=2}, twice=sixty;
    int a=2,b=2;
    for(int i=1;i<=12;i++) overview_scroll_sync(&sixty,&a,6,i/60.0,false);
    for(int i=1;i<=24;i++) overview_scroll_sync(&twice,&b,6,i/120.0,false);
    assert(fabsf(sixty.position-twice.position)<.00001f);
    assert(sixty.position>1.9f && sixty.position<=2);
    overview_scroll_sync(&sixty,&a,6,2,false);
    assert(sixty.position==2); // stalled-frame recovery
    overview_scroll_sync(&sixty,&a,0,2.016,false);
    assert(sixty.position==0 && a==0);
    return 0;
}
