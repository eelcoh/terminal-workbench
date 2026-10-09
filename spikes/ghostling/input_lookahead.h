/* Non-destructive lookahead for the modal-to-terminal handoff. Raylib's short
 * press/text queues carry events even when IsKeyPressed no longer reports them.
 * Normal input bypasses the empty buffers; there is no frame-wide collection. */
#define WORKBENCH_INPUT_LOOKAHEAD 128
static int workbench_pending_keys[WORKBENCH_INPUT_LOOKAHEAD], workbench_pending_chars[WORKBENCH_INPUT_LOOKAHEAD];
static int workbench_key_count, workbench_key_first, workbench_char_count, workbench_char_first;

static void workbench_input_peek(void)
{
    int value;
    while (workbench_key_count < WORKBENCH_INPUT_LOOKAHEAD && (value = GetKeyPressed()) != 0)
        workbench_pending_keys[workbench_key_count++] = value;
    while (workbench_char_count < WORKBENCH_INPUT_LOOKAHEAD && (value = GetCharPressed()) != 0)
        workbench_pending_chars[workbench_char_count++] = value;
}
static bool workbench_input_has_key(int key)
{
    for (int i = workbench_key_first; i < workbench_key_count; i++)
        if (workbench_pending_keys[i] == key) return true;
    return false;
}
static bool workbench_input_has_text(void) { return workbench_char_first < workbench_char_count; }
static int workbench_get_key_pressed(void)
{
    if (workbench_key_first < workbench_key_count) {
        int value = workbench_pending_keys[workbench_key_first++];
        if (workbench_key_first == workbench_key_count) workbench_key_first = workbench_key_count = 0;
        return value;
    }
    return GetKeyPressed();
}
static int workbench_get_char_pressed(void)
{
    if (workbench_char_first < workbench_char_count) {
        int value = workbench_pending_chars[workbench_char_first++];
        if (workbench_char_first == workbench_char_count) workbench_char_first = workbench_char_count = 0;
        return value;
    }
    return GetCharPressed();
}
#define GetKeyPressed workbench_get_key_pressed
#define GetCharPressed workbench_get_char_pressed
