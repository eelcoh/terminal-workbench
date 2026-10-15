/* GLFW adapter: callback-order events, separate from Raylib's UI key/text queues. */
#ifndef WORKBENCH_PLATFORM_KEYBOARD_H
#define WORKBENCH_PLATFORM_KEYBOARD_H
#include <stdbool.h>
#include <stdint.h>
#define WB_KEYBOARD_CAPACITY 4096
enum { WB_KEY_EVENT=1, WB_TEXT_EVENT=2 };
/* codepoint is the unmodified layout scalar for keys, committed text for text events. */
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
/* GLFW key names are UTF-8; retain exactly one valid layout scalar. */
static uint32_t wb_keyboard_layout_codepoint(const char *name)
{
    if (!name || !name[0]) return 0;
    const unsigned char *s=(const unsigned char *)name;
    uint32_t cp=0;int length=0;
    if(s[0]<0x80) {cp=s[0];length=1;}
    else if(s[0]>=0xc2 && s[0]<=0xdf) {cp=s[0]&0x1f;length=2;}
    else if(s[0]>=0xe0 && s[0]<=0xef) {cp=s[0]&0x0f;length=3;}
    else if(s[0]>=0xf0 && s[0]<=0xf4) {cp=s[0]&7;length=4;}
    else return 0;
    for(int i=1;i<length;i++) {
        if((s[i]&0xc0)!=0x80) return 0;
        cp=(cp<<6)|(s[i]&0x3f);
    }
    if(s[length] || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)
        || (length==2 && cp<0x80) || (length==3 && cp<0x800)
        || (length==4 && cp<0x10000) || cp<0x20 || cp==0x7f) return 0;
    return cp;
}
static void wb_keyboard_push_key(int key, int action, int mods, uint32_t unshifted)
{
    wb_keyboard_mods=mods;
    wb_keyboard_push((WbKeyboardEvent){WB_KEY_EVENT,key,action,mods,unshifted});
}
static void wb_keyboard_push_text(uint32_t codepoint)
{
    if (!codepoint || codepoint>0x10ffff || (codepoint>=0xd800 && codepoint<=0xdfff)) return;
    wb_keyboard_push((WbKeyboardEvent){WB_TEXT_EVENT,0,1,wb_keyboard_mods,codepoint});
}
#endif
#endif
