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

/* Releases/repeats of an intercepted chord must not leak into Kitty input. */
static bool workbench_key_blocked[512];
static bool shortcut_platform_apple(void)
{
#if defined(__APPLE__)
    return true;
#else
    return false;
#endif
}
static unsigned shortcut_modifiers(void)
{
    return ((IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)) ? 1u : 0u)
        | ((IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT)) ? 2u : 0u)
        | ((IsKeyDown(KEY_LEFT_ALT)||IsKeyDown(KEY_RIGHT_ALT)) ? 4u : 0u)
        | ((IsKeyDown(KEY_LEFT_SUPER)||IsKeyDown(KEY_RIGHT_SUPER)) ? 8u : 0u);
}
