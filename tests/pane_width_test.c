#include <assert.h>
#include "pane_width.h"
int main(void) {
    PaneWidth widths[8]={{.preset=2},{.preset=2},{.preset=2}};
    for (int estate=596;estate<1600;estate++) {
        float end=pane_width_start(widths,3,estate,10)-10;
        assert(end==estate);
        assert(pane_width_pixels(widths,0,estate,10)+pane_width_pixels(widths,1,estate,10)+pane_width_pixels(widths,2,estate,10)+20==estate);
    }
    widths[0].preset=0;widths[1].preset=0;
    assert(pane_width_start(widths,2,1036,10)-10==1036);
    widths[0].preset=2;widths[1].preset=1;
    assert(pane_width_start(widths,2,1036,10)-10==1036);
    for (int p=0;p<8;p++) widths[p].preset=p%3;
    float scroll=pane_width_reveal(widths,8,7,0,1036,10);
    assert(pane_width_visible(widths,7,scroll,1036,10));
    assert(pane_width_reveal(widths,8,0,scroll,1036,10)==0);
    unsigned seen=0;
    for (int frame=0;frame<=240;frame++) for (int p=0;p<8;p++)
        if (pane_width_visible(widths,p,scroll*frame/240,1036,10)) seen|=1u<<p;
    assert(seen==255);
    widths[3].full=true;
    assert(pane_width_pixels(widths,3,1036,10)==1036);
    widths[3].full=false;assert(widths[3].preset==0);
    assert(pane_width_snap(.33f)==2 && pane_width_snap(.5f)==0 && pane_width_snap(.67f)==1);
    assert(pane_width_reveal(widths,0,0,0,1036,10)==0);
    assert(pane_width_reveal(widths,8,0,NAN,1036,10)==0);
    return 0;
}
