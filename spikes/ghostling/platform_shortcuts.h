/* Clipboard policy is platform-local; terminal control keys remain untouched. */
#if defined(__APPLE__)
#define WORKBENCH_COPY_HINT "Copy selection (Command+C)"
#define WORKBENCH_PASTE_HINT "Paste (Command+V)"
static bool clipboard_chord_down(void)
{
    return (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER))
        && !IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_RIGHT_CONTROL)
        && !IsKeyDown(KEY_LEFT_ALT) && !IsKeyDown(KEY_RIGHT_ALT);
}
#else
#define WORKBENCH_COPY_HINT "Copy selection (Ctrl+Shift+C)"
#define WORKBENCH_PASTE_HINT "Paste (Ctrl+Shift+V)"
static bool clipboard_chord_down(void)
{
    return (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
        && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
        && !IsKeyDown(KEY_LEFT_ALT) && !IsKeyDown(KEY_RIGHT_ALT)
        && !IsKeyDown(KEY_LEFT_SUPER) && !IsKeyDown(KEY_RIGHT_SUPER);
}
#endif

/* Text fields follow desktop editing conventions, independently of terminal
 * clipboard chords (Linux terminal paste needs Shift; text fields do not). */
static bool text_edit_modifier_down(void)
{
#if defined(__APPLE__)
    return IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
#else
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
#endif
}
