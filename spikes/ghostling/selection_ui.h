/* Pointer selection delegates anchors, word/line boundaries and tracking to VT. */
typedef struct {
    GhosttySelectionGesture gesture;
    GhosttySelectionGestureEvent press, drag, release;
    bool dragging;
} SelectionUI;
static SelectionUI single_selection;
static SelectionUI *active_selection = &single_selection;
#define selection_ui (*active_selection)

static void selection_ui_free(GhosttyTerminal terminal)
{
    ghostty_selection_gesture_event_free(selection_ui.press);
    ghostty_selection_gesture_event_free(selection_ui.drag);
    ghostty_selection_gesture_event_free(selection_ui.release);
    ghostty_selection_gesture_free(selection_ui.gesture, terminal);
    memset(&selection_ui, 0, sizeof(selection_ui));
}

static bool selection_ui_init(void)
{
    GhosttyResult result = ghostty_selection_gesture_new(NULL, &selection_ui.gesture);
    if (result == GHOSTTY_SUCCESS)
        result = ghostty_selection_gesture_event_new(NULL, &selection_ui.press,
                    GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_PRESS);
    if (result == GHOSTTY_SUCCESS)
        result = ghostty_selection_gesture_event_new(NULL, &selection_ui.drag,
                    GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_DRAG);
    if (result == GHOSTTY_SUCCESS)
        result = ghostty_selection_gesture_event_new(NULL, &selection_ui.release,
                    GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_RELEASE);
    if (result != GHOSTTY_SUCCESS)
        fprintf(stderr, "selection setup failed (%d)\n", result);
    return result == GHOSTTY_SUCCESS;
}

static bool selection_active(GhosttyTerminal terminal)
{
    GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
    return ghostty_terminal_get(terminal, GHOSTTY_TERMINAL_DATA_SELECTION,
                                &selection) == GHOSTTY_SUCCESS;
}

static void terminal_return_to_live(GhosttyTerminal terminal)
{
    GhosttyTerminalScrollViewport scroll = { .tag = GHOSTTY_SCROLL_VIEWPORT_BOTTOM };
    ghostty_terminal_scroll_viewport(terminal, scroll);
    ghostty_terminal_set(terminal, GHOSTTY_TERMINAL_OPT_SELECTION, NULL);
    ghostty_selection_gesture_reset(selection_ui.gesture, terminal);
    selection_ui.dragging = false;
}

static bool selection_option(GhosttySelectionGestureEvent event,
                             GhosttySelectionGestureEventOption option,
                             const void *value)
{
    GhosttyResult result = ghostty_selection_gesture_event_set(event, option, value);
    if (result != GHOSTTY_SUCCESS)
        fprintf(stderr, "selection input failed (%d)\n", result);
    return result == GHOSTTY_SUCCESS;
}

static bool handle_selection(GhosttyTerminal terminal, int cell_width,
                             int cell_height, int pad, bool scrollbar_consumed)
{
    Vector2 pointer = GetMousePosition();
    bool tracking = false;
    ghostty_terminal_get(terminal, GHOSTTY_TERMINAL_DATA_MOUSE_TRACKING, &tracking);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool inside = pointer.x >= pad && pointer.x < GetScreenWidth() - 16
               && pointer.y >= WORKBENCH_HEADER_HEIGHT + pad
               && pointer.y < GetScreenHeight() - pad;
    if (!selection_ui.dragging
        && (!inside || scrollbar_consumed || (tracking && !shift)))
        return false;

    uint16_t columns = 0, rows = 0;
    ghostty_terminal_get(terminal, GHOSTTY_TERMINAL_DATA_COLS, &columns);
    ghostty_terminal_get(terminal, GHOSTTY_TERMINAL_DATA_ROWS, &rows);
    if (!columns || !rows) return false;
    int column = (int)((pointer.x - pad) / cell_width);
    int row = (int)((pointer.y - WORKBENCH_HEADER_HEIGHT - pad) / cell_height);
    if (column < 0) column = 0;
    if (row < 0) row = 0;
    if (column >= columns) column = columns - 1;
    if (row >= rows) row = rows - 1;
    GhosttyPoint point = {
        .tag = GHOSTTY_POINT_TAG_VIEWPORT,
        .value = { .coordinate = { .x = (uint16_t)column, .y = (uint32_t)row } },
    };
    GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
    if (ghostty_terminal_grid_ref(terminal, point, &ref) != GHOSTTY_SUCCESS)
        return false;

    GhosttySelectionGestureEvent event = NULL;
    bool press = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool release = selection_ui.dragging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    if (press) {
        selection_ui.dragging = true;
        event = selection_ui.press;
        ghostty_terminal_set(terminal, GHOSTTY_TERMINAL_OPT_SELECTION, NULL);
        uint64_t time_ns = (uint64_t)(GetTime() * 1000000000.0);
        uint64_t interval_ns = 500000000;
        double repeat_distance = 4;
        selection_option(event, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_TIME_NS, &time_ns);
        selection_option(event, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REPEAT_INTERVAL_NS, &interval_ns);
        selection_option(event, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REPEAT_DISTANCE, &repeat_distance);
    } else if (release) {
        event = selection_ui.release;
    } else if (selection_ui.dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        event = selection_ui.drag;
        GhosttySelectionGestureGeometry geometry = {
            .columns = columns, .cell_width = (uint32_t)cell_width,
            .padding_left = (uint32_t)pad,
            .screen_height = (uint32_t)(GetScreenHeight() - WORKBENCH_HEADER_HEIGHT),
        };
        selection_option(event, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_GEOMETRY, &geometry);
    }
    if (event) {
        GhosttySurfacePosition position = { .x = pointer.x,
                                            .y = pointer.y - WORKBENCH_HEADER_HEIGHT };
        if (!release)
            selection_option(event, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_POSITION, &position);
        if (selection_option(event, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF, &ref)) {
            GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
            GhosttyResult result = ghostty_selection_gesture_event(selection_ui.gesture,
                                        terminal, event, &selection);
            if (result == GHOSTTY_SUCCESS)
                ghostty_terminal_set(terminal, GHOSTTY_TERMINAL_OPT_SELECTION, &selection);
            else if (result != GHOSTTY_NO_VALUE)
                fprintf(stderr, "selection gesture failed (%d)\n", result);
        }
    }
    bool consumed = selection_ui.dragging || release || (tracking && shift);
    if (release) selection_ui.dragging = false;
    return consumed;
}

static void selection_scroll(GhosttyTerminal terminal)
{
    float wheel = GetMouseWheelMove();
    if (!wheel) return;
    GhosttyTerminalScrollViewport scroll = {
        .tag = GHOSTTY_SCROLL_VIEWPORT_DELTA,
        .value = { .delta = wheel > 0 ? -3 : 3 },
    };
    ghostty_terminal_scroll_viewport(terminal, scroll);
}
