/* GLFW adapter: callback-order events, separate from Raylib's UI key/text queues. */
#ifndef WORKBENCH_PLATFORM_KEYBOARD_H
#define WORKBENCH_PLATFORM_KEYBOARD_H
#include <stdbool.h>
#include <stdint.h>
#define WB_KEYBOARD_CAPACITY 4096
enum { WB_KEY_EVENT=1, WB_TEXT_EVENT=2 };
typedef struct { int kind, key, action, mods; uint32_t codepoint; } WbKeyboardEvent;
int wb_keyboard_event_count(void);
bool wb_keyboard_event_get(int index, WbKeyboardEvent *out);
bool wb_keyboard_overflowed(void);
#ifdef WB_KEYBOARD_IMPLEMENTATION
static WbKeyboardEvent wb_keyboard_events[WB_KEYBOARD_CAPACITY];
static int wb_keyboard_count, wb_keyboard_mods;
static bool wb_keyboard_overflow;
int wb_keyboard_event_count(void) { return wb_keyboard_count; }
bool wb_keyboard_event_get(int index, WbKeyboardEvent *out)
{
    if (!out || index<0 || index>=wb_keyboard_count) return false;
    *out=wb_keyboard_events[index];return true;
}
bool wb_keyboard_overflowed(void) { return wb_keyboard_overflow; }
static void wb_keyboard_reset(void)
{ wb_keyboard_count=0;wb_keyboard_overflow=false; }
static void wb_keyboard_push(WbKeyboardEvent event)
{
    if (wb_keyboard_count==WB_KEYBOARD_CAPACITY) {wb_keyboard_overflow=true;return;}
    wb_keyboard_events[wb_keyboard_count++]=event;
}
static void wb_keyboard_push_key(int key, int action, int mods)
{
    wb_keyboard_mods=mods;
    wb_keyboard_push((WbKeyboardEvent){WB_KEY_EVENT,key,action,mods,0});
}
static void wb_keyboard_push_text(uint32_t codepoint)
{
    if (!codepoint || codepoint>0x10ffff || (codepoint>=0xd800 && codepoint<=0xdfff)) return;
    wb_keyboard_push((WbKeyboardEvent){WB_TEXT_EVENT,0,1,wb_keyboard_mods,codepoint});
}
#endif
#endif
