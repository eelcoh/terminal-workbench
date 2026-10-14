/* Clipboard I/O is a client action; raw Ctrl-C continues to reach the shell. */
#define WORKBENCH_CLIPBOARD_LIMIT 65536
static bool workbench_clipboard_keys=true;
static const char *clipboard_message = NULL;
static double clipboard_message_until = 0;

static void clipboard_notice(const char *message)
{
    clipboard_message = message;
    clipboard_message_until = GetTime() + 3;
}

static const char *clipboard_current_notice(void)
{
    return GetTime() < clipboard_message_until ? clipboard_message : NULL;
}

static void clipboard_copy(GhosttyTerminal terminal)
{
    GhosttyTerminalSelectionFormatOptions options =
        GHOSTTY_INIT_SIZED(GhosttyTerminalSelectionFormatOptions);
    options.emit = GHOSTTY_FORMATTER_FORMAT_PLAIN;
    options.unwrap = true;
    options.trim = true;
    size_t length = 0;
    GhosttyResult result = ghostty_terminal_selection_format_buf(terminal, options,
                                                               NULL, 0, &length);
    if (result == GHOSTTY_NO_VALUE) { clipboard_notice("Select text to copy"); return; }
    if (result != GHOSTTY_OUT_OF_SPACE && result != GHOSTTY_SUCCESS) {
        clipboard_notice("Could not copy selection"); return;
    }
    if (length > WORKBENCH_CLIPBOARD_LIMIT) {
        clipboard_notice("Selection too large to copy"); return;
    }
    char *text = malloc(length + 1);
    if (!text) { clipboard_notice("Could not copy selection"); return; }
    result = ghostty_terminal_selection_format_buf(terminal, options,
                                    (uint8_t *)text, length, &length);
    if (result == GHOSTTY_SUCCESS) {
        text[length] = '\0';
        SetClipboardText(text);
        clipboard_notice("Copied selection");
    } else {
        clipboard_notice("Could not copy selection");
    }
    free(text);
}

static void clipboard_paste(int fd, GhosttyTerminal terminal)
{
    const char *clipboard = GetClipboardText();
    if (!clipboard || !clipboard[0]) { clipboard_notice("Clipboard is empty"); return; }
    size_t length = strnlen(clipboard, WORKBENCH_CLIPBOARD_LIMIT + 1);
    if (length > WORKBENCH_CLIPBOARD_LIMIT) {
        clipboard_notice("Clipboard text too large to paste"); return;
    }
    char *input = malloc(length + 1);
    char *encoded = malloc(length + 12);
    if (!input || !encoded) {
        free(input); free(encoded); clipboard_notice("Could not paste clipboard"); return;
    }
    memcpy(input, clipboard, length + 1);
    GhosttyTerminalModeConfig bracketed = { .mode = GHOSTTY_MODE_BRACKETED_PASTE };
    ghostty_terminal_get(terminal, GHOSTTY_TERMINAL_DATA_MODE, &bracketed);
    size_t written = 0;
    GhosttyResult result = ghostty_paste_encode(input, length, bracketed.value,
                                               encoded, length + 12, &written);
    if (result == GHOSTTY_SUCCESS) {
        terminal_return_to_live(terminal);
        pty_write(fd, encoded, written);
    } else {
        clipboard_notice("Could not paste clipboard");
    }
    free(input);
    free(encoded);
}

static bool workbench_window_buttons=true;
static bool handle_clipboard(int fd, GhosttyTerminal terminal, bool can_paste)
{
    bool chord = workbench_clipboard_keys && clipboard_chord_down();
    bool copy_key = chord && (IsKeyPressed(KEY_C) || IsKeyPressedRepeat(KEY_C));
    bool paste_key = chord && (IsKeyPressed(KEY_V) || IsKeyPressedRepeat(KEY_V));
    bool clicked = workbench_window_buttons && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Vector2 pointer = GetMousePosition();
    if (copy_key || (clicked && CheckCollisionPointRec(pointer, window_copy_bounds())))
        clipboard_copy(terminal);
    if (can_paste && (paste_key || (clicked && CheckCollisionPointRec(pointer, window_paste_bounds()))))
        clipboard_paste(fd, terminal);
    if (copy_key || paste_key) {
        /* The shortcut's printable key must never also enter the terminal. */
        while (GetCharPressed()) { }
        while (GetKeyPressed()) { }
        return true;
    }
    return false;
}

/* Native ordered input is consumed by the terminal adapter. Raylib queues remain
 * available to UI routing and are drained here only to reset the viewport. */
#include "platform_keyboard.h"
static void prepare_terminal_keyboard_input(GhosttyTerminal terminal)
{
    while(GetKeyPressed()!=0) { }
    WbKeyboardEvent event;bool input=false;
    for(int i=0;i<wb_keyboard_event_count();i++) if(wb_keyboard_event_get(i,&event)
        && (event.kind==WB_TEXT_EVENT || event.key<KEY_LEFT_SHIFT || event.key>KEY_RIGHT_SUPER)) {input=true;break;}
    if(input) terminal_return_to_live(terminal);
}
