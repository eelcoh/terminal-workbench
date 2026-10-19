#include "terminal_scroll.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
static void check(bool ok,const char *text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
int main(){
    TerminalScroll scroll;
    const auto pixel=[&](int y,Qt::ScrollPhase phase=Qt::ScrollUpdate,int x=0){return scroll.consume({x,y},x || y ? QPoint(0,120) : QPoint(),20,3,true,phase);};
    check(pixel(0,Qt::ScrollBegin).rows==0,"zero phase markers never become angle steps");
    scroll.reset();int total=0;for(int i=0;i<40;++i)total+=pixel(1).rows;
    check(total==-2,"small trackpad deltas accumulate into older rows");
    scroll.reset();total=0;for(int i=0;i<40;++i)total+=pixel(-1).rows;
    check(total==2,"opposite delivered pixels move toward live rows");
    scroll.reset();check(pixel(19).rows==0 && pixel(1).rows==-1,"sub-row motion retained");
    check(pixel(-20).rows==1,"direction reversal");
    scroll.reset();pixel(19);pixel(0,Qt::ScrollEnd);check(pixel(1).rows==0,"gesture end clears remainder");
    check(pixel(19,Qt::ScrollBegin).rows==0,"new gesture discards old remainder");
    scroll.reset();check(pixel(20,Qt::ScrollUpdate,1).rows==-1,"vertical gesture survives diagonal jitter");
    check(!pixel(1,Qt::ScrollUpdate,20).handled,"horizontal gesture does not scroll terminal rows");
    scroll.reset();check(scroll.consume({0,-400},{0,120},20,3,false,Qt::NoScrollPhase).rows==-3,"X11 uses angle direction, not unreliable pixels");
    check(scroll.consume({},{0,-120},20,5,false,Qt::NoScrollPhase).rows==5,"configured wheel step and opposite direction");
    scroll.reset();total=0;for(int i=0;i<40;++i)total+=scroll.consume({},{0,1},20,3,false,Qt::NoScrollPhase).rows;
    check(total==-1,"high resolution wheel angles accumulate");
    scroll.reset();pixel(19);check(scroll.consume({},{0,40},20,3,false,Qt::NoScrollPhase).rows==-1,"input unit changes discard stale remainder");
    scroll.reset();check(scroll.consume({0,2},{},0,3,true,Qt::ScrollUpdate).rows==0,"invalid cell geometry is ignored");
    check(scroll.consume({},{0,std::numeric_limits<int>::max()},20,3,false,Qt::NoScrollPhase).rows==-1000,"pathological event is bounded");
    std::puts("PASS: fractional pixel/angle scrolling, direction, diagonal isolation, phases, source changes, X11 policy and bounds");
}
