#include <assert.h>
#include "display_geometry.h"
int main(void)
{
    assert(display_raster_scale(1240,760,1240,760)==1);
    assert(display_raster_scale(1240,760,2480,1520)==2);
    assert(display_raster_scale(1240,760,1550,950)==1.25f);
    assert(display_raster_scale(0,760,0,1520)==0);
    assert(display_raster_scale(1240,760,2480,760)==0);
    DisplayWorkarea laptop={{0,25,1440,850},0,28,0,0,1};
    DisplayRect fit=display_fit_window((DisplayRect){1900,100,1920,1080},laptop);
    assert(fit.width==1440 && fit.height==822 && fit.x==0 && fit.y==53);
    DisplayRect ordinary={50,90,1000,600};fit=display_fit_window(ordinary,laptop);
    assert(fit.x==ordinary.x && fit.y==ordinary.y && fit.width==ordinary.width && fit.height==ordinary.height);
    DisplayWorkarea left={{-1920,0,1920,1080},4,30,4,4,2};
    fit=display_fit_window((DisplayRect){-4000,-100,1240,760},left);
    assert(fit.x==-1916 && fit.y==30 && fit.width==1240 && fit.height==760);
    assert(display_workarea_changed(laptop,left));assert(!display_workarea_changed(laptop,laptop));
    left.count=1;assert(display_workarea_changed((DisplayWorkarea){{-1920,0,1920,1080},4,30,4,4,2},left));
    DisplayWorkarea external={{1440,0,1920,1080},0,28,0,0,2};
    DisplayWorkarea internal=laptop;internal.count=2;
    DisplayRect straddled={1200,90,1240,760},candidate=display_fit_window(straddled,external);
    assert(!display_should_fit(internal,external,straddled,candidate,false));
    assert(display_should_fit(external,laptop,straddled,display_fit_window(straddled,laptop),false));
    assert(display_should_fit((DisplayWorkarea){0},laptop,straddled,display_fit_window(straddled,laptop),true));
    return 0;
}
