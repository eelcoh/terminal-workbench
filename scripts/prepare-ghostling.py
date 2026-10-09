#!/usr/bin/env python3
"""Prepare an attributed, pinned experimental viewer without editing upstream."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent.parent
REVISION = "63842bf8e5e481160f81d348da9ff6fd27986798"
source = ROOT / ".local/deps/ghostling"
destination = ROOT / ".local/ghostling-viewer"
if not source.exists():
    source.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["git", "clone", "https://github.com/ghostty-org/ghostling.git", str(source)], check=True)
subprocess.run(["git", "-C", str(source), "cat-file", "-e", REVISION], check=True)
destination.mkdir(parents=True, exist_ok=True)
def write_if_changed(path, data):
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)
# Read the pinned tree, regardless of the dependency checkout's current branch.
def upstream(path):
    return subprocess.check_output(["git", "-C", str(source), "show", f"{REVISION}:{path}"])

for path in ["CMakeLists.txt", "bin2header.cmake", "LICENSE", "fonts/OFL.txt", "fonts/JetBrainsMono-Regular.ttf"]:
    target = destination / path
    target.parent.mkdir(parents=True, exist_ok=True)
    write_if_changed(target, upstream(path))

text = upstream("main.c").decode()
start = text.index("// Spawn the user's default shell")
end = text.index("// ---------------------------------------------------------------------------", start)
text = text[:start] + '#include "surface_geometry.h"\n#include "session_bridge.h"\n#include "platform_shortcuts.h"\n#include "window_controls.h"\n#include "selection_ui.h"\n#include "input_lookahead.h"\n#include "clipboard_actions.h"\n\n' + text[end:]
def replace_once(old, new):
    global text
    assert text.count(old) == 1, f"pinned source adaptation no longer applies: {old}"
    text = text.replace(old, new)

replace_once('InitWindow(800, 600, "ghostling");', 'InitWindow(960, 640, "Terminal Workbench — terminal spike");\n    SetExitKey(KEY_NULL);')
replace_once('ioctl(pty_fd, TIOCSWINSZ, &new_ws);', '(void)new_ws;\n                remote_resize(pty_fd, term_cols, term_rows);')
replace_once('bool child_reaped = false;', 'bool child_reaped = true; // The server owns process reaping.')
replace_once('PtyReadResult pty_rc = pty_read(pty_fd, terminal);', """PtyReadResult pty_rc = pty_read(pty_fd, terminal);
            if (remote_cols && remote_rows) {
                effects_ctx.cols = remote_cols;
                effects_ctx.rows = remote_rows;
            }""")
replace_once('"[process exited]"', '"[session connection closed]"')
# Reserve the header in grid, mouse, image, cursor and scrollbar geometry.
replace_once('SetWindowState(FLAG_WINDOW_RESIZABLE);', 'SetWindowState(FLAG_WINDOW_RESIZABLE);\n    SetWindowMinSize(420, 96);')
replace_once('while (!WindowShouldClose()) {', 'while (!WindowShouldClose() && !window_close_requested()) {')
replace_once('(scr_h - 2 * pad) / cell_height', '(scr_h - WORKBENCH_HEADER_HEIGHT - 2 * pad) / cell_height')
replace_once('(h - 2 * pad) / cell_height', '(h - WORKBENCH_HEADER_HEIGHT - 2 * pad) / cell_height')
replace_once('.padding_top   = (uint32_t)pad,', '.padding_top   = (uint32_t)(pad + WORKBENCH_HEADER_HEIGHT),')
replace_once('int y = pad;', 'int y = pad + WORKBENCH_HEADER_HEIGHT;')
replace_once('int dest_y = pad + (int)vp_row', 'int dest_y = pad + WORKBENCH_HEADER_HEIGHT + (int)vp_row')
replace_once('int cur_y = pad + cy', 'int cur_y = pad + WORKBENCH_HEADER_HEIGHT + cy')
replace_once('&& mpos.x >= hit_left && mpos.x <= scr_w)', '&& mpos.y >= WORKBENCH_HEADER_HEIGHT\n        && mpos.x >= hit_left && mpos.x <= scr_w)')
replace_once('double frac = (double)mpos.y / (double)scr_h;', 'double frac = (double)(mpos.y - WORKBENCH_HEADER_HEIGHT) / (double)(scr_h - WORKBENCH_HEADER_HEIGHT);')
replace_once('int thumb_height = (int)(scr_h * visible_frac);', 'int thumb_height = (int)((scr_h - WORKBENCH_HEADER_HEIGHT) * visible_frac);')
replace_once('int thumb_y = (int)(scroll_frac * (scr_h - thumb_height));', 'int thumb_y = WORKBENCH_HEADER_HEIGHT + (int)(scroll_frac * (scr_h - WORKBENCH_HEADER_HEIGHT - thumb_height));')
replace_once("""        // Forward keyboard/mouse input only while the child is alive.
        if (!child_exited) {
            handle_input(pty_fd, key_encoder, key_event, terminal);
            if (!scrollbar_consumed)
                handle_mouse(pty_fd, mouse_encoder, mouse_event, terminal,
                             cell_width, cell_height, pad);
        }""", """        bool selection_consumed = handle_selection(terminal, cell_width,
                                        cell_height, pad, scrollbar_consumed);
        bool can_paste = !child_exited && !remote_replaying;
        bool clipboard_keyboard = handle_clipboard(pty_fd, terminal, can_paste);
        if (can_paste && !clipboard_keyboard) {
            prepare_terminal_keyboard_input(terminal);
            handle_input(pty_fd, key_encoder, key_event, terminal);
        }
        if (!scrollbar_consumed && GetMousePosition().y >= WORKBENCH_HEADER_HEIGHT) {
            if (selection_consumed || child_exited)
                selection_scroll(terminal);
            else if (can_paste)
                handle_mouse(pty_fd, mouse_encoder, mouse_event, terminal,
                             cell_width, cell_height, pad);
        }""")
replace_once('        EndDrawing();', '        draw_window_controls(mono_font, selection_active(terminal), can_paste, clipboard_current_notice());\n        EndDrawing();')


replace_once('    // Aim for about 1000 lines of scrollback.', '    if (!selection_ui_init()) { exit_code = 1; goto cleanup; }\n\n    // Aim for about 1000 lines of scrollback.')
replace_once('    if (terminal)       ghostty_terminal_free(terminal);', '    selection_ui_free(terminal);\n    if (terminal)       ghostty_terminal_free(terminal);')
# Query selection once per row; the cell loop only compares cached bounds.
replace_once('        int x = pad;', """        GhosttyRenderStateRowSelection selection = GHOSTTY_INIT_SIZED(GhosttyRenderStateRowSelection);
        bool selected_row = ghostty_render_state_row_get(row_iter,
            GHOSTTY_RENDER_STATE_ROW_DATA_SELECTION, &selection) == GHOSTTY_SUCCESS;
        int column = 0;
        int x = pad;""")
replace_once('            // How many codepoints make up the grapheme? 0 = empty cell.', """            bool selected = selected_row && column >= selection.start_x
                            && column <= selection.end_x;
            column++;
            // How many codepoints make up the grapheme? 0 = empty cell.""")
replace_once('                x += cell_width;\n                continue;', '                if (selected)\n                    DrawRectangle(x, y, cell_width, cell_height, (Color){48, 72, 98, 255});\n                x += cell_width;\n                continue;')
replace_once('            Color ray_fg = { fg.r, fg.g, fg.b, 255 };', """            if (selected) {
                fg = (GhosttyColorRgb){235, 240, 246};
                bg_rgb = (GhosttyColorRgb){48, 72, 98};
                has_bg = true;
            }
            Color ray_fg = { fg.r, fg.g, fg.b, 255 };""")

# Preserve queued short key presses and do not attach a preceding character
# to Enter or another non-printable key delivered in the same frame.
replace_once('bool pressed  = IsKeyPressed(rl_key);', 'bool pressed  = terminal_key_pressed(rl_key);')
replace_once('bool released = IsKeyReleased(rl_key);', 'bool released = IsKeyReleased(rl_key) && !pressed;')
replace_once('        ghostty_key_event_set_unshifted_codepoint(event, ucp);', """        ghostty_key_event_set_unshifted_codepoint(event, ucp);
        if (ucp == 0 && !released && char_utf8_len > 0) {
            pty_write(pty_fd, char_utf8, (size_t)char_utf8_len);
            char_utf8_len = 0;
        }""")
replace_once('int main(void)', 'static int single_terminal_main(void)')
text += '\n#include "workspace_viewer.h"\n\nint main(void) {\n    const char *catalog = getenv("WORKBENCH_WORKSPACES");\n    return catalog ? workspace_main(catalog) : single_terminal_main();\n}\n'
write_if_changed(destination / "main.c", text.encode())
write_if_changed(destination / "session_bridge.h", (ROOT / "spikes/ghostling/session_bridge.h").read_bytes())
for name in ["window_controls.h", "platform_shortcuts.h", "selection_ui.h", "clipboard_actions.h", "surface_geometry.h", "workspace_viewer.h", "catalog_view.h", "management_ui.h", "overview_ui.h", "overview_spatial.h", "terminal_preview.h", "overview_animation.h", "overview_motion.h", "overview_drag.h", "input_lookahead.h"]:
    write_if_changed(destination / name, (ROOT / "spikes/ghostling" / name).read_bytes())
print(f"Prepared Ghostling {REVISION} with Workbench session transport at {destination}")
