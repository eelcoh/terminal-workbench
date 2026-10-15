#include <assert.h>
#define WB_KEYBOARD_IMPLEMENTATION
#include "platform_keyboard.h"
int main(void)
{
    WbKeyboardEvent e;
    assert(wb_keyboard_layout_codepoint("z")=='z');
    assert(wb_keyboard_layout_codepoint("\xc3\xa9")==0xe9);
    assert(wb_keyboard_layout_codepoint("\xe4\xb8\xad")==0x4e2d);
    assert(wb_keyboard_layout_codepoint("\xf0\x9f\xa6\x80")==0x1f980);
    assert(!wb_keyboard_layout_codepoint(0) && !wb_keyboard_layout_codepoint(""));
    assert(!wb_keyboard_layout_codepoint("ab") && !wb_keyboard_layout_codepoint("\xc0\xaf"));
    assert(!wb_keyboard_layout_codepoint("\xed\xa0\x80") && !wb_keyboard_layout_codepoint("\xf4\x90\x80\x80"));
    assert(!wb_keyboard_layout_codepoint("\xe4") && !wb_keyboard_layout_codepoint("\x1b"));
    wb_keyboard_push_key(89,1,2,'z');wb_keyboard_push_key(89,0,2,'z');
    assert(wb_keyboard_event_get(0,&e) && e.key==89 && e.codepoint=='z');
    assert(wb_keyboard_event_get(1,&e) && e.action==0 && e.codepoint=='z');
    wb_keyboard_reset();
    wb_keyboard_push_key(65,1,1,0);wb_keyboard_push_text('A');wb_keyboard_push_key(65,0,1,0);
    wb_keyboard_push_key(66,2,2,0);wb_keyboard_push_key(66,0,2,0);
    wb_keyboard_push_text(0x1f980);
    assert(wb_keyboard_event_count()==6 && !wb_keyboard_overflowed());
    assert(wb_keyboard_event_get(0,&e) && e.key==65 && e.action==1 && e.mods==1);
    assert(wb_keyboard_event_get(1,&e) && e.kind==WB_TEXT_EVENT && e.codepoint=='A' && e.mods==1);
    assert(wb_keyboard_event_get(2,&e) && e.key==65 && e.action==0);
    assert(wb_keyboard_event_get(3,&e) && e.key==66 && e.action==2 && e.mods==2);
    assert(wb_keyboard_event_get(5,&e) && e.codepoint==0x1f980 && e.mods==2);
    assert(!wb_keyboard_event_get(-1,&e) && !wb_keyboard_event_get(6,&e) && !wb_keyboard_event_get(0,0));
    wb_keyboard_push_text(0);wb_keyboard_push_text(0xd800);wb_keyboard_push_text(0x110000);
    assert(wb_keyboard_event_count()==6);
    wb_keyboard_reset();assert(!wb_keyboard_event_count());
    for(int i=0;i<WB_KEYBOARD_CAPACITY;i++) wb_keyboard_push_key(i,1,0,0);
    assert(wb_keyboard_event_count()==WB_KEYBOARD_CAPACITY && !wb_keyboard_overflowed());
    wb_keyboard_push_text('x');assert(wb_keyboard_overflowed());
    assert(wb_keyboard_event_get(WB_KEYBOARD_CAPACITY-1,&e) && e.key==WB_KEYBOARD_CAPACITY-1);
    wb_keyboard_reset();assert(!wb_keyboard_overflowed() && !wb_keyboard_event_count());
    /* Char-only commits and repeated same-key presses retain their own order. */
    wb_keyboard_push_text(0x00e9);wb_keyboard_push_key(65,1,0,0);wb_keyboard_push_text('a');
    wb_keyboard_push_key(65,0,0,0);wb_keyboard_push_key(65,1,0,0);wb_keyboard_push_text('a');
    assert(wb_keyboard_event_count()==6);
    assert(wb_keyboard_event_get(0,&e) && e.kind==WB_TEXT_EVENT && e.codepoint==0x00e9);
    return 0;
}
