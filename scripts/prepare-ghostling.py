#!/usr/bin/env python3
"""Prepare a pinned Ghostling adaptation and marked Raylib input fix locally."""
from pathlib import Path
import subprocess
from font_cmap import header as font_codepoint_header, codepoints

ROOT = Path(__file__).resolve().parent.parent
REVISION = "63842bf8e5e481160f81d348da9ff6fd27986798"
GHOSTLING_REPOSITORY = "git@github.com:ghostty-org/ghostling.git"
source = ROOT / ".local/deps/ghostling"
destination = ROOT / ".local/ghostling-viewer"
if not source.exists():
    source.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["git", "clone", GHOSTLING_REPOSITORY, str(source)], check=True)
# Migrate dependency checkouts created by earlier HTTPS-based builds too.
subprocess.run(["git", "-C", str(source), "remote", "set-url", "origin", GHOSTLING_REPOSITORY], check=True)
subprocess.run(["git", "-C", str(source), "cat-file", "-e", REVISION], check=True)
subprocess.run(["python3", str(ROOT / "scripts/prepare-theme-catalog.py"), "--check"], check=True)
destination.mkdir(parents=True, exist_ok=True)
def write_if_changed(path, data):
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)
# Read the pinned tree, regardless of the dependency checkout's current branch.
def upstream(path):
    return subprocess.check_output(["git", "-C", str(source), "show", f"{REVISION}:{path}"])

for path in [ "bin2header.cmake", "LICENSE", "fonts/OFL.txt", "fonts/JetBrainsMono-Regular.ttf"]:
    target = destination / path
    target.parent.mkdir(parents=True, exist_ok=True)
    write_if_changed(target, upstream(path))

for name in ("NotoSansSymbols2-Regular.ttf", "OFL.txt"):
    write_if_changed(destination / "fonts" / ("Noto-" + name if name == "OFL.txt" else name),
        (ROOT / "fonts/noto-symbols" / name).read_bytes())
write_if_changed(destination / "terminal_font_codepoints.h", font_codepoint_header(
    (destination / "fonts/JetBrainsMono-Regular.ttf").read_bytes(),
    (destination / "fonts/NotoSansSymbols2-Regular.ttf").read_bytes()).encode())

for name in ("AdwaitaSans-Regular.ttf", "OFL.txt"):
    write_if_changed(destination / "fonts" / ("Adwaita-"+name if name=="OFL.txt" else name),
        (ROOT / "fonts/adwaita-sans" / name).read_bytes())
ui_points=codepoints((ROOT / "fonts/adwaita-sans/AdwaitaSans-Regular.ttf").read_bytes())
write_if_changed(destination / "ui_font_codepoints.h", ("// Generated from the bundled interface font.\n#pragma once\nstatic int ui_font_codepoints[]={\n"+
    "\n".join("    "+",".join(map(str,ui_points[i:i+16]))+"," for i in range(0,len(ui_points),16))+"\n};\n").encode())

# Adapt only our generated CMake file; the pinned upstream tree stays unchanged.
cmake = upstream("CMakeLists.txt").decode()
archive = "    DOWNLOAD_EXTRACT_TIMESTAMP OFF\n    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz"
assert cmake.count(archive) == 1, "pinned Raylib fetch declaration changed"
cmake = cmake.replace(archive, "    GIT_REPOSITORY git@github.com:raysan5/raylib.git\n    GIT_TAG ${RAYLIB_VERSION}\n    GIT_SHALLOW TRUE")
ghostty_repository = "GIT_REPOSITORY https://github.com/ghostty-org/ghostty.git"
assert cmake.count(ghostty_repository) == 1, "pinned Ghostty fetch declaration changed"
cmake = cmake.replace(ghostty_repository, "GIT_REPOSITORY git@github.com:ghostty-org/ghostty.git")
cmake = cmake.replace("# Use a system-installed raylib if available, otherwise fetch from GitHub.",
    "# Fetch pinned Raylib with the marked Workbench mouse-edge adaptation.")
# Pin/fetch Raylib so the mouse-edge adaptation also applies on machines with a
# system Raylib. The GLFW backend is used by this spike on Linux and macOS.
start_raylib = cmake.index("find_package(raylib")
end_raylib = cmake.index("# --- libghostty", start_raylib)
cmake = cmake[:start_raylib] + 'find_package(Python3 COMPONENTS Interpreter REQUIRED)\nFetchContent_Declare(raylib\n  GIT_REPOSITORY git@github.com:raysan5/raylib.git\n  GIT_TAG ${RAYLIB_VERSION}\n  GIT_SHALLOW TRUE\n)\nFetchContent_MakeAvailable(raylib)\nexecute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/patch-raylib-input.py" "${raylib_SOURCE_DIR}"\n  COMMAND_ERROR_IS_FATAL ANY)\nexecute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/patch-raylib-display.py" "${raylib_SOURCE_DIR}"\n  COMMAND_ERROR_IS_FATAL ANY)\n\n' + cmake[end_raylib:]
write_if_changed(destination / "embed-font.py", (ROOT / "scripts/embed-font.py").read_bytes())
write_if_changed(destination / "patch-raylib-input.py", (ROOT / "scripts/patch-raylib-input.py").read_bytes())
write_if_changed(destination / "patch-raylib-display.py", (ROOT / "scripts/patch-raylib-display.py").read_bytes())
cmake = cmake.replace("# --- Our Project", """set(SYMBOL_FONT_INPUT "${CMAKE_SOURCE_DIR}/fonts/NotoSansSymbols2-Regular.ttf")
set(SYMBOL_FONT_HEADER "${CMAKE_BINARY_DIR}/font_noto_symbols.h")
add_custom_command(OUTPUT "${SYMBOL_FONT_HEADER}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/embed-font.py"
    "${SYMBOL_FONT_INPUT}" "${SYMBOL_FONT_HEADER}" font_noto_symbols
  DEPENDS "${SYMBOL_FONT_INPUT}" "${CMAKE_SOURCE_DIR}/embed-font.py")
set(UI_FONT_INPUT "${CMAKE_SOURCE_DIR}/fonts/AdwaitaSans-Regular.ttf")
set(UI_FONT_HEADER "${CMAKE_BINARY_DIR}/font_adwaita_sans.h")
add_custom_command(OUTPUT "${UI_FONT_HEADER}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/embed-font.py"
    "${UI_FONT_INPUT}" "${UI_FONT_HEADER}" font_adwaita_sans
  DEPENDS "${UI_FONT_INPUT}" "${CMAKE_SOURCE_DIR}/embed-font.py")
# --- Our Project""")
cmake = cmake.replace('main.c "${FONT_HEADER}"', 'main.c "${FONT_HEADER}" "${SYMBOL_FONT_HEADER}" "${UI_FONT_HEADER}"')
write_if_changed(destination / "CMakeLists.txt", cmake.encode())

text = upstream("main.c").decode()
start = text.index("// Spawn the user's default shell")
end = text.index("// ---------------------------------------------------------------------------", start)
text = text[:start] + '#include "ui_style.h"\n#include "surface_geometry.h"\n#include "session_bridge.h"\n#include "platform_shortcuts.h"\n#include "window_controls.h"\n#include "selection_ui.h"\n#include "input_lookahead.h"\n#include "clipboard_actions.h"\n\n' + text[end:]
def replace_once(old, new):
    global text
    assert text.count(old) == 1, f"pinned source adaptation no longer applies: {old}"
    text = text.replace(old, new)

replace_once('#include "font_jetbrains_mono.h"', '#include "font_jetbrains_mono.h"\n#include "terminal_font.h"')
replace_once('Font mono_font = LoadFontFromMemory(".ttf", font_jetbrains_mono,\n                         (int)sizeof(font_jetbrains_mono), font_size_px, NULL, 0);',
    'Font mono_font = terminal_font_load(font_size_px);')
replace_once('UnloadFont(mono_font);', 'terminal_font_unload(mono_font);')
grapheme_start = text.index('            // Build a UTF-8 string from the grapheme codepoints.')
grapheme_end = text.index('            // Resolve foreground and background colors', grapheme_start)
text = text[:grapheme_start] + text[grapheme_end:]
replace_once('DrawTextEx(font, text, (Vector2){x + italic_offset, y}, font_size, 0, ray_fg);',
    'terminal_draw_grapheme(font, codepoints, len, (Vector2){x + italic_offset, y}, font_size, ray_fg);')
replace_once('DrawTextEx(font, text, (Vector2){x + italic_offset + 1, y}, font_size, 0, ray_fg);',
    'terminal_draw_grapheme(font, codepoints, len, (Vector2){x + italic_offset + 1, y}, font_size, ray_fg);')

replace_once('InitWindow(800, 600, "ghostling");', 'InitWindow(960, 640, "Terminal Workbench — terminal spike");\n    SetExitKey(KEY_NULL);')
replace_once('ioctl(pty_fd, TIOCSWINSZ, &new_ws);', '(void)new_ws;\n                remote_resize(pty_fd, term_cols, term_rows);')
replace_once('bool child_reaped = false;', 'bool child_reaped = true; // The server owns process reaping.')
replace_once('PtyReadResult pty_rc = pty_read(pty_fd, terminal);', """PtyReadResult pty_rc = pty_read(pty_fd, &terminal, &effects_ctx);
            if (remote_cols && remote_rows) {
                effects_ctx.cols = remote_cols;
                effects_ctx.rows = remote_rows;
            }""")
replace_once('"[process exited]"', 'active_remote->attached_elsewhere ? "[open in another window]" : "[session connection closed]"')
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
replace_once('    if (terminal)       ghostty_terminal_free(terminal);', '    free(single_remote.snapshot);\n    selection_ui_free(terminal);\n    if (terminal)       ghostty_terminal_free(terminal);')
# A tap can deliver both edges in one frame; emit both to mouse-aware terminals.
replace_once('        if (IsMouseButtonPressed(rl_btn)) {', '        bool release_first = IsMouseButtonPressed(rl_btn)\n            && IsMouseButtonReleased(rl_btn) && IsMouseButtonDown(rl_btn);\n        for (int edge = 0; edge < 2; edge++) {\n        if (edge == (release_first ? 1 : 0) && IsMouseButtonPressed(rl_btn)) {')
replace_once('} else if (IsMouseButtonReleased(rl_btn)) {', '} else if (edge == (release_first ? 0 : 1) && IsMouseButtonReleased(rl_btn)) {')
replace_once('    // Mouse motion — send a motion event', '    } /* Both button edges are delivered before motion. */\n\n    // Mouse motion — send a motion event')
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
replace_once('                x += cell_width;\n                continue;', '                if (selected)\n                    DrawRectangle(x, y, cell_width, cell_height, ui_color(terminal_style()->selection_background));\n                x += cell_width;\n                continue;')
replace_once('            Color ray_fg = { fg.r, fg.g, fg.b, 255 };', """            if (selected) {
                fg = (GhosttyColorRgb){terminal_style()->selection_foreground.r, terminal_style()->selection_foreground.g, terminal_style()->selection_foreground.b};
                bg_rgb = (GhosttyColorRgb){terminal_style()->selection_background.r, terminal_style()->selection_background.g, terminal_style()->selection_background.b};
                has_bg = true;
            }
            Color ray_fg = { fg.r, fg.g, fg.b, 255 };""")

# Preserve queued short key presses and do not attach a preceding character
# to Enter or another non-printable key delivered in the same frame.
replace_once('bool pressed  = IsKeyPressed(rl_key);', 'bool pressed  = terminal_key_pressed(rl_key);')
replace_once('bool released = IsKeyReleased(rl_key);', 'bool released = IsKeyReleased(rl_key) && !pressed;')
replace_once('        ghostty_key_event_set_unshifted_codepoint(event, ucp);', """        ghostty_key_event_set_unshifted_codepoint(event, ucp);
        // Raylib suppresses text for Ctrl+Shift+letters. Supply the same
        // US-layout letter used by this adapter so Ghostty can distinguish
        // the shifted chord and encode CSI-u instead of dropping it.
        if (!released && char_utf8_len == 0 && ucp >= 'a' && ucp <= 'z'
            && (mods & GHOSTTY_MODS_CTRL) && (mods & GHOSTTY_MODS_SHIFT)) {
            char_utf8[0] = (char)(ucp - 'a' + 'A');
            char_utf8_len = 1;
        }
        if (ucp == 0 && !released && char_utf8_len > 0) {
            pty_write(pty_fd, char_utf8, (size_t)char_utf8_len);
            char_utf8_len = 0;
        }""")
replace_once('    uint16_t rows;\n} EffectsContext;', '    uint16_t rows;\n    const Theme *theme;\n} EffectsContext;')
replace_once('    (void)out_scheme;\n    return false;', '    const EffectsContext *context = userdata;\n    const Theme *theme = context && context->theme ? context->theme : &current_theme;\n    *out_scheme = theme_light(theme) ? GHOSTTY_COLOR_SCHEME_LIGHT : GHOSTTY_COLOR_SCHEME_DARK;\n    return true;')
replace_once('        int rl_key = keys_to_check[i];', '        int rl_key = keys_to_check[i];\n        if (rl_key >= 0 && rl_key < 512 && workbench_key_blocked[rl_key]) continue;')
replace_once('int main(void)', '#include "snapshot_restore.h"\n\nstatic int single_terminal_main(void)')
text += '\n#include "workspace_viewer.h"\n\nint main(void) {\n    ui_theme_set(theme_preset(false));\n    const char *catalog = getenv("WORKBENCH_WORKSPACES");\n    return catalog ? workspace_main(catalog) : single_terminal_main();\n}\n'
write_if_changed(destination / "main.c", text.encode())
write_if_changed(destination / "session_bridge.h", (ROOT / "spikes/ghostling/session_bridge.h").read_bytes())
for name in ["portable_config.h", "terminal_font.h", "window_state.h", "window_state_client.h", "overview_scroll.h", "snapshot_restore.h", "window_controls.h", "platform_shortcuts.h", "selection_ui.h", "clipboard_actions.h", "surface_geometry.h", "workspace_viewer.h", "catalog_view.h", "management_ui.h", "command_palette.h", "command_actions.h", "shortcut_state.h", "shortcut_client.h", "shortcut_help.h", "shortcut_help_ui.h", "ui_focus.h", "ui_focus_draw.h", "ui_font.h", "chrome_ui.h", "minimal_chrome.h", "pixel_geometry.h", "display_geometry.h", "platform_display.h", "display_client.h", "overview_ui.h", "overview_spatial.h", "terminal_preview.h", "overview_animation.h", "overview_motion.h", "overview_drag.h", "input_lookahead.h", "theme_core.h", "ui_style.h", "theme_ui.h", "theme_catalog.h", "theme_catalog_data.h", "appearance_state.h", "appearance_client.h", "pane_navigation.h", "pane_width.h", "navigation_ui.h"]:
    write_if_changed(destination / name, (ROOT / "spikes/ghostling" / name).read_bytes())
for collection in ["catppuccin", "dracula"]:
    target = destination / "theme-licenses" / collection / "LICENSE"
    target.parent.mkdir(parents=True, exist_ok=True)
    write_if_changed(target, (ROOT / "themes" / collection / "LICENSE").read_bytes())
print(f"Prepared Ghostling {REVISION} with Workbench session transport at {destination}")
