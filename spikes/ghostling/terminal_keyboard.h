/* Preserve native callback order and associate text only with its own key. */
#include "platform_keyboard.h"
static GhosttyMods terminal_event_mods(int mods)
{
    return ((mods&1) ? GHOSTTY_MODS_SHIFT : 0) | ((mods&2) ? GHOSTTY_MODS_CTRL : 0)
        | ((mods&4) ? GHOSTTY_MODS_ALT : 0) | ((mods&8) ? GHOSTTY_MODS_SUPER : 0);
}
static GhosttyResult terminal_encode_event(GhosttyKeyEncoder encoder,GhosttyKeyEvent event,
                                  WbKeyboardEvent input,const char *text,int length,
                                  char *bytes,size_t capacity,size_t *written)
{
    *written=0;
    if (input.key>=KEY_LEFT_SHIFT && input.key<=KEY_RIGHT_SUPER) return GHOSTTY_SUCCESS;
    if (input.key>=0 && input.key<512 && workbench_key_blocked[input.key]) return GHOSTTY_SUCCESS;
    GhosttyMods mods=terminal_event_mods(input.mods);
    GhosttyKey key=raylib_key_to_ghostty(input.key);
    uint32_t unshifted=raylib_key_unshifted_codepoint(input.key);
    char shifted[4];
    /* GLFW suppresses character callbacks for Ctrl chords. Retain the existing
     * adapter's US-layout Ctrl+Shift fallback for Ghostty's legacy CSI-u path. */
    if (!length && input.action!=0 && unshifted>='a' && unshifted<='z'
        && (mods&GHOSTTY_MODS_CTRL) && (mods&GHOSTTY_MODS_SHIFT)) {
        length=utf8_encode(unshifted-'a'+'A',shifted);text=shifted;
    }
    /* A printable press without text can be a dead/IME key. Its eventual text
     * callback is authoritative. Control/Alt/Super chords still encode normally. */
    if (!length && input.action!=0 && unshifted && !(input.mods&(2|4|8))) return GHOSTTY_SUCCESS;
    ghostty_key_event_set_key(event,key);
    ghostty_key_event_set_action(event,input.action==0 ? GHOSTTY_KEY_ACTION_RELEASE :
        input.action==2 ? GHOSTTY_KEY_ACTION_REPEAT : GHOSTTY_KEY_ACTION_PRESS);
    ghostty_key_event_set_mods(event,mods);
    ghostty_key_event_set_unshifted_codepoint(event,unshifted);
    ghostty_key_event_set_consumed_mods(event,length && (mods&GHOSTTY_MODS_SHIFT) ? GHOSTTY_MODS_SHIFT : 0);
    ghostty_key_event_set_utf8(event,length ? text : NULL,(size_t)length);
    return ghostty_key_encoder_encode(encoder,event,bytes,capacity,written);
}
static void handle_input(int fd,GhosttyKeyEncoder encoder,GhosttyKeyEvent event,GhosttyTerminal terminal)
{
    ghostty_key_encoder_setopt_from_terminal(encoder,terminal);
    /* UI lookahead/queues have separate ownership. Drain any remaining text so
     * it cannot be replayed alongside the ordered native events. */
    while(GetCharPressed()!=0) { }
    if(wb_keyboard_overflowed()) {
        TraceLog(LOG_WARNING,"Terminal keyboard event queue overflow; input batch discarded");return;
    }
    char output[65536];size_t used=0;
    int count=wb_keyboard_event_count();
    for(int i=0;i<count;i++) {
        WbKeyboardEvent input;if(!wb_keyboard_event_get(i,&input)) break;
        char text[WB_KEYBOARD_CAPACITY*4];int length=0;
        if(input.kind==WB_TEXT_EVENT) {
            length=utf8_encode(input.codepoint,text);input.key=0;input.action=1;
        } else if(input.kind==WB_KEY_EVENT && input.action!=0) {
            WbKeyboardEvent following;
            while(i+1<count && wb_keyboard_event_get(i+1,&following) && following.kind==WB_TEXT_EVENT) {
                length+=utf8_encode(following.codepoint,text+length);i++;
            }
        }
        size_t written=0;
        GhosttyResult result=terminal_encode_event(encoder,event,input,text,length,
            output+used,sizeof(output)-used,&written);
        if(result==GHOSTTY_OUT_OF_SPACE && used) {
            pty_write(fd,output,used);used=0;
            result=terminal_encode_event(encoder,event,input,text,length,output,sizeof(output),&written);
        }
        if(result==GHOSTTY_SUCCESS) used+=written;
        else TraceLog(LOG_WARNING,"Terminal keyboard event encoding failed: %d",result);
    }
    if(used) pty_write(fd,output,used);
}
