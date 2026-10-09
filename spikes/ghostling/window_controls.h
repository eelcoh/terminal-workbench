/* Viewer-owned window controls, independent of terminal input and sessions. */
#define WORKBENCH_HEADER_HEIGHT 32

static Rectangle window_close_bounds(void)
{
    return (Rectangle){ GetScreenWidth() - 80.0f, 4.0f, 72.0f, 24.0f };
}

static Rectangle window_copy_bounds(void)
{
    return (Rectangle){ GetScreenWidth() - 224.0f, 4.0f, 64.0f, 24.0f };
}

static Rectangle window_paste_bounds(void)
{
    return (Rectangle){ GetScreenWidth() - 152.0f, 4.0f, 64.0f, 24.0f };
}

static bool window_close_requested(void)
{
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
        && CheckCollisionPointRec(GetMousePosition(), window_close_bounds());
}

static void draw_header_button(Font font, Rectangle bounds, const char *label,
                               bool enabled)
{
    bool hovered = enabled && CheckCollisionPointRec(GetMousePosition(), bounds);
    DrawRectangleRec(bounds, hovered ? (Color){ 54, 59, 67, 255 }
                                    : (Color){ 38, 41, 46, 255 });
    Vector2 text = MeasureTextEx(font, label, 14, 0);
    DrawTextEx(font, label,
               (Vector2){ bounds.x + (bounds.width - text.x) / 2, bounds.y + 5 },
               14, 0, enabled ? (Color){ 225, 228, 233, 255 }
                              : (Color){ 100, 105, 113, 255 });
}

static void draw_window_controls(Font font, bool can_copy, bool can_paste,
                                 const char *notice)
{
    Rectangle close = window_close_bounds();
    Rectangle copy = window_copy_bounds();
    Rectangle paste = window_paste_bounds();
    Vector2 pointer = GetMousePosition();
    DrawRectangle(0, 0, GetScreenWidth(), WORKBENCH_HEADER_HEIGHT,
                  (Color){ 22, 24, 27, 255 });
    BeginScissorMode(8, 4, (int)copy.x - 16, 24);
    DrawTextEx(font, notice ? notice : "Terminal Workbench", (Vector2){ 8, 8 },
               14, 0, (Color){ 166, 171, 180, 255 });
    EndScissorMode();
    draw_header_button(font, copy, "Copy", can_copy);
    draw_header_button(font, paste, "Paste", can_paste);
    draw_header_button(font, close, "Close", true);
    const char *hint = NULL;
    if (CheckCollisionPointRec(pointer, close))
        hint = "Close window; shell keeps running";
    else if (CheckCollisionPointRec(pointer, copy))
        hint = can_copy ? WORKBENCH_COPY_HINT : "Select text to copy";
    else if (CheckCollisionPointRec(pointer, paste))
        hint = can_paste ? WORKBENCH_PASTE_HINT : "Session is not ready for input";
    if (hint) {
        Vector2 size = MeasureTextEx(font, hint, 14, 0);
        float left = GetScreenWidth() - size.x - 16;
        if (left < 4) left = 4;
        DrawRectangle((int)left - 4, WORKBENCH_HEADER_HEIGHT + 4,
                      (int)size.x + 8, 22, (Color){ 38, 41, 46, 255 });
        DrawTextEx(font, hint, (Vector2){ left, WORKBENCH_HEADER_HEIGHT + 8 },
                   14, 0, (Color){ 225, 228, 233, 255 });
    }
}
