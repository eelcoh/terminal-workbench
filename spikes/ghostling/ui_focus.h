/* Pure client focus traversal. Bit masks describe available controls, not runtime facts. */
#ifndef WORKBENCH_UI_FOCUS_H
#define WORKBENCH_UI_FOCUS_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { int item, activated; bool visible; uint32_t enabled, buttons; } UiFocus;
static inline uint32_t ui_focus_bit(int item) { return item>=0 && item<32 ? UINT32_C(1)<<item : 0; }
static inline int ui_focus_next(int item, int direction, uint32_t enabled)
{
    if (!enabled) return -1;
    int next=item<0 ? (direction>0 ? 31 : 0) : item;
    for (int i=0;i<32;i++) { next=(next+(direction>0 ? 1 : 31))%32; if (enabled & ui_focus_bit(next)) return next; }
    return -1;
}
static inline void ui_focus_begin(UiFocus *focus, uint32_t enabled, uint32_t buttons)
{
    focus->activated=-1;focus->enabled=enabled;focus->buttons=buttons;
    if (enabled && focus->item>=0 && !(enabled & ui_focus_bit(focus->item))) focus->item=ui_focus_next(focus->item,1,enabled);
}
static inline void ui_focus_step(UiFocus *focus, bool reverse)
{
    if (!focus->enabled) return;
    focus->item=ui_focus_next(focus->item,reverse ? -1 : 1,focus->enabled);focus->visible=true;
}
static inline bool ui_focus_activate(UiFocus *focus)
{
    if (!(focus->enabled & focus->buttons & ui_focus_bit(focus->item))) return false;
    focus->activated=focus->item;return true;
}
#endif
