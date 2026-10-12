#include <assert.h>
#include "ui_focus.h"
int main(void)
{
    UiFocus focus={.item=-1};
    ui_focus_begin(&focus,12,12);
    assert(!ui_focus_activate(&focus)); /* Opening confirmation cannot stop/recover. */
    ui_focus_step(&focus,false);assert(focus.item==2 && focus.visible);
    assert(ui_focus_activate(&focus) && focus.activated==2); /* Cancel. */
    ui_focus_begin(&focus,12,12);assert(focus.activated==-1);
    ui_focus_step(&focus,false);assert(focus.item==3);
    assert(ui_focus_activate(&focus) && focus.activated==3); /* Explicit confirmation. */
    ui_focus_begin(&focus,0,12);assert(focus.item==3 && !ui_focus_activate(&focus)); /* Busy retains focus, disables action. */
    ui_focus_step(&focus,true);assert(focus.item==3);
    focus=(UiFocus){.item=0};ui_focus_begin(&focus,13,12);
    assert(!ui_focus_activate(&focus)); /* Text Enter belongs to its dialog. */
    ui_focus_step(&focus,false);assert(focus.item==2); /* Absent second field skipped. */
    ui_focus_step(&focus,true);assert(focus.item==0);
    ui_focus_step(&focus,true);assert(focus.item==3);
    ui_focus_step(&focus,false);assert(focus.item==0);
    focus.item=1;ui_focus_begin(&focus,13,12);assert(focus.item==2); /* Control became unavailable. */
    assert(ui_focus_next(-1,-1,12)==3);
    assert(ui_focus_next(31,1,UINT32_C(0x80000001))==0);
    assert(ui_focus_next(0,-1,UINT32_C(0x80000001))==31);
    assert(ui_focus_bit(-1)==0 && ui_focus_bit(32)==0);
    return 0;
}
