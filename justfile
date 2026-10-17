config-path:
    python3 scripts/config_paths.py

config-init:
    python3 scripts/config_paths.py --init

default:
    @just --list

fmt:
    cargo fmt --all

runtime-build:
    python3 scripts/build-spike.py --runtime-only

check: runtime-build
    cargo fmt --all -- --check
    cargo clippy --workspace --all-targets -- -D warnings
    cargo test --workspace
    cargo build --locked --bin workbench-session
    python3 tests/session_smoke.py target/debug/workbench-session
    python3 tests/context_smoke.py target/debug/workbench-session
    python3 tests/terminal_core_smoke.py target/debug/workbench-session
    python3 tests/snapshot_attach_smoke.py target/debug/workbench-session
    python3 tests/catalog_test.py
    python3 tests/catalog_journal_test.py target/debug/workbench-session
    python3 tests/session_shutdown_test.py target/debug/workbench-session
    python3 tests/session_soak.py target/debug/workbench-session --seconds 3 --terminals 1 --report .local/test-results/session-soak-check.json
    python3 tests/session_soak_interrupt_test.py target/debug/workbench-session
    python3 tests/socket_path_test.py target/debug/workbench-session
    python3 tests/move_test.py
    python3 tests/close_smoke.py target/debug/workbench-session
    python3 tests/agent_smoke.py target/debug/workbench-session
    python3 tests/attention_smoke.py target/debug/workbench-session
    python3 tests/agent_watch_test.py target/debug/workbench-session
    python3 tests/copilot_observer_test.py
    python3 scripts/prepare-theme-catalog.py --check
    python3 tests/overview_animation_test.py
    python3 tests/terminal_search_vt_test.py
    python3 tests/snapshot_stage_test.py
    python3 tests/macos_metadata_test.py
    python3 tests/config_paths_test.py
    python3 tests/build_env_test.py
    python3 tests/wayland_display_test.py
    python3 tests/font_assets_test.py
    python3 tests/recovery_test.py target/debug/workbench-session
    python3 tests/session_service_test.py target/debug/workbench-session
    python3 tests/terminal_status_cli_test.py
    python3 tests/lifecycle_record_test.py
    python3 tests/lifecycle_smoke.py target/debug/workbench-session
    python3 tests/workspace_directories_test.py target/debug/workbench-session
    python3 tests/folder_picker_test.py
    python3 tests/window_slots_test.py
    python3 tests/launch_diagnostics_test.py

# Finds Zig 0.16.0 locally/on PATH, or uses an explicit ZIG executable.
spike-build:
    python3 scripts/build-spike.py

spike-run socket:
    WORKBENCH_SOCKET="{{socket}}" .local/build/viewer/ghostling

spike-test: runtime-build
    cargo build --locked --bin workbench-session
    python3 tests/session_smoke.py target/debug/workbench-session
    python3 tests/context_smoke.py target/debug/workbench-session
    python3 tests/terminal_core_smoke.py target/debug/workbench-session
    python3 tests/snapshot_attach_smoke.py target/debug/workbench-session

launch-gui-test: spike-build
    python3 tests/launch_diagnostics_test.py .local/build/viewer/ghostling

spike-gui-test: spike-build
    python3 tests/gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

workspace-gui-test: spike-build
    python3 tests/mouse_edges_test.py .local/build/viewer/_deps/raylib-src
    python3 tests/workspace_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

management-gui-test: spike-build
    python3 tests/management_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

expose-gui-test: spike-build
    python3 tests/expose_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling
    WORKBENCH_REDUCED_MOTION=1 python3 tests/expose_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

move-gui-test: spike-build
    python3 tests/move_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

theme-gui-test: spike-build
    python3 tests/theme_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling 1
    python3 tests/theme_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling 15

# Start/reuse the workspace sessions and attach a desktop window.
spike: spike-build
    python3 scripts/run-spike.py

# Explicitly recover dead session endpoints after a reboot or runtime crash.
recover-sessions:
    python3 scripts/workspace_catalog.py --state-dir .local/sessions --runtime target/debug/workbench-session --cwd . --command session.recover

# Working-view navigation, populated cardinality and reduced motion.
navigation-gui-test: spike-build
    python3 tests/navigation_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling 1
    python3 tests/navigation_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling 15
    WORKBENCH_REDUCED_MOTION=1 python3 tests/navigation_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling 15

# Runtime VT parsing cardinality, detached, fixed geometry (Linux diagnostic only).
runtime-core-scale: runtime-build
    python3 tests/runtime_core_scale.py target/debug/workbench-session

# Real snapshot restoration and legacy negotiation fallback.
snapshot-gui-test: spike-build
    python3 tests/snapshot_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Maximum populated catalog through overview scrolling and navigation.
overview-scale-gui-test: spike-build
    WORKBENCH_TEST_OVERVIEW_WORKSPACES=8 WORKBENCH_TEST_OVERVIEW_PANES=8 python3 tests/expose_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Shared catalog projection and runtime ownership transfer across two windows.
multiwindow-gui-test: spike-build
    python3 tests/multiwindow_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Inspect runtime health without attaching to or restarting terminals.
session-status:
    python3 scripts/session_service.py status --state-dir .local/sessions --runtime target/debug/workbench-session --cwd .

# Read agent identity/status and viewer metadata; never restart or attach terminals.
agent-status:
    python3 scripts/agent_status.py

# Capture delivered lifecycle transitions without attaching or changing sessions.
agent-watch seconds="120":
    python3 scripts/agent_watch.py --seconds "{{seconds}}"

# Restart only an explicitly selected stopped terminal; preserves identity/themes.
recover-terminal id:
    python3 scripts/session_service.py recover --state-dir .local/sessions --runtime target/debug/workbench-session --cwd . --id "{{id}}"

# Stops the organization service only. Live terminals and windows continue.
service-stop:
    python3 scripts/session_service.py stop --state-dir .local/sessions --runtime target/debug/workbench-session --cwd .

# Independent client view restoration across close, crash and catalog changes.
window-state-gui-test: spike-build
    python3 tests/window_state_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Action palette routing and modal isolation with real live shells.
commands-gui-test: spike-build
    python3 tests/command_palette_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Real Unicode glyph rendering, independent of the system's installed fonts.
font-gui-test: spike-build
    python3 tests/font_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Copilot labels and Needs input filtering through the existing GUI workflows.
copilot-gui-test: spike-build
    WORKBENCH_TEST_AGENT=copilot python3 tests/management_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Current shortcut help, slash recording and overlay input ownership.
shortcuts-gui-test: spike-build
    python3 tests/shortcut_help_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling
    WORKBENCH_REDUCED_MOTION=1 python3 tests/shortcut_help_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Fixed window geometry, populated runtime cardinality and equal terminal estate.
shortcuts-scale-gui-test: spike-build
    python3 tests/shortcut_help_scale.py target/debug/workbench-session .local/build/viewer/ghostling 1
    python3 tests/shortcut_help_scale.py target/debug/workbench-session .local/build/viewer/ghostling 15

# Keyboard focus traversal, explicit confirmations and overlay restoration.
focus-gui-test: spike-build
    python3 tests/ui_focus_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Backing-texture/atlas fidelity and logical PTY/input contracts at simulated 2x.
retina-gui-test: spike-build
    python3 tests/retina_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling
    WORKBENCH_RENDER_SCALE=2 python3 tests/navigation_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling 15

# Minimal native chrome, menus, panel geometry and live ownership.
chrome-gui-test: spike-build
    python3 tests/minimal_chrome_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Pinned renderer callbacks and dynamic backing-density changes.
display-gui-test: spike-build
    python3 tests/display_callback_test.py .local/build/viewer/_deps/raylib-src
    python3 tests/display_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling 1
    python3 tests/display_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling 15

# Portable settings and UI overrides with same-process reopen.
font-settings-gui-test: spike-build
    python3 tests/font_settings_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling 1
    python3 tests/font_settings_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling 15

dotfiles-gui-test: spike-build
    python3 tests/dotfiles_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Live Overview context and search with real shells and independent runtime metadata.
context-gui-test: spike-build
    python3 tests/context_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Native cursor/selection editing and clipboard isolation with real live shells.
text-edit-gui-test: spike-build
    python3 tests/text_edit_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Focused-terminal output/scrollback search with live shell and grid continuity.
search-gui-test: spike-build
    python3 tests/terminal_search_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling 1
    python3 tests/terminal_search_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling 15

# Exit reason presentation and explicit replacement-shell confirmation.
lifecycle-gui-test: spike-build
    python3 tests/lifecycle_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Explicit session shutdown, confirmed selection and surviving peer windows.
shutdown-gui-test: spike-build
    python3 tests/shutdown_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Sustained runtime/ownership/service checks in disposable sessions (no GUI).
session-soak seconds="300" terminals="15": runtime-build
    python3 tests/session_soak.py target/debug/workbench-session --seconds "{{seconds}}" --terminals "{{terminals}}"

# Uses the actual Linux desktop folder chooser in an isolated Xvfb display.
folder-picker-gui-test: spike-build
    python3 tests/folder_picker_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Slow snapshot transport and interactive UI at one/fifteen-terminal cardinality.
attachment-gui-test: spike-build
    python3 tests/attachment_progress_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling
    WORKBENCH_TEST_PANES=15 python3 tests/attachment_progress_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Ordered native keyboard input with actual Ghostty legacy/Kitty encoders.
keyboard-gui-test: spike-build
    python3 tests/keyboard_input_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling 12
    python3 tests/keyboard_input_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling 1

# Opt-in Linux native Wayland viewer; default spike remains X11.
wayland-build:
    python3 scripts/build-spike.py --wayland

wayland-spike: wayland-build
    python3 scripts/run-spike.py --wayland

# Requires niri, Xvfb, wl-clipboard and Wayland/xkbcommon development tools; uses no existing desktop/session.
wayland-gui-test: wayland-build
    python3 tests/wayland_gui_smoke.py target/debug/workbench-session .local/build/viewer-wayland/ghostling

# Non-US text and modified-key routing in a disposable native Wayland seat.
keyboard-layout-gui-test: wayland-build
    WORKBENCH_TEST_LAYOUTS=1 python3 tests/wayland_gui_smoke.py target/debug/workbench-session .local/build/viewer-wayland/ghostling

# Actual compositor scale transitions in the private nested Wayland session.
wayland-display-gui-test: wayland-build
    WORKBENCH_TEST_DISPLAY=1 python3 tests/wayland_gui_smoke.py target/debug/workbench-session .local/build/viewer-wayland/ghostling

# Native selection/copy at 1x, fractional scale and startup at fractional scale.
wayland-pointer-gui-test: wayland-build
    python3 tests/wayland_pointer_callback_test.py .local/build/viewer-wayland/_deps/raylib-src
    WORKBENCH_TEST_POINTER=1 python3 tests/wayland_gui_smoke.py target/debug/workbench-session .local/build/viewer-wayland/ghostling

# Isolated Qt Quick text/semantic API probe; does not launch Workbench sessions.
toolkit-probe-build:
    python3 scripts/toolkit-probe.py build

toolkit-probe:
    python3 scripts/toolkit-probe.py run

toolkit-probe-test:
    python3 scripts/toolkit-probe.py test

# Disposable single-terminal Qt host; closes its temporary shell on exit.
toolkit-terminal: runtime-build
    python3 scripts/toolkit-probe.py terminal

# Real PTY, snapshot, input, selection/clipboard, resize and same-PID reopen.
toolkit-terminal-test: runtime-build
    python3 scripts/toolkit-probe.py terminal-test

# Native Qt Wayland in private nested Niri: fractional scale, input, clipboard.
toolkit-terminal-wayland-test: runtime-build
    python3 scripts/toolkit-probe.py terminal-wayland-test

# Qt terminal text/focus/selection semantics with a real disposable PTY.
toolkit-terminal-accessibility-test: runtime-build
    python3 tests/qt_accessibility_gui_smoke.py

# Linux only: real IBus/Anthy key -> composition -> commit on private desktop/bus.
toolkit-terminal-ime-test: runtime-build
    python3 scripts/toolkit-probe.py terminal-ime-test

# Linux: real IME transfers between two independently owned editable terminals.
toolkit-terminal-ime-transfer-test: runtime-build
    python3 scripts/toolkit-probe.py terminal-ime-transfer-test

# Mac and Linux: offline terminal-cell/full-text shaping and fallback comparison.
toolkit-text-test: runtime-build
    python3 scripts/toolkit-probe.py text-test

# Mac/Linux: Release CPU row-raster/cache benchmark, 1/15 panes at 1x/2x.
toolkit-perf: runtime-build
    python3 scripts/toolkit-probe.py perf

# Mac/Linux: native Qt Quick scene/image path; keep the windows visible.
toolkit-scene-perf: runtime-build
    python3 scripts/toolkit-probe.py scene-perf

# Linux: private Xvfb + software OpenGL, explicit 1x/2x scene operation checks.
toolkit-scene-perf-test: runtime-build
    python3 scripts/toolkit-probe.py scene-perf-test

# Mac/Linux: Qt key events through the real terminal host to a raw disposable PTY.
toolkit-terminal-key-test: runtime-build
    python3 scripts/toolkit-probe.py terminal-key-test

# Mac/Linux: two disposable editable terminals in one Qt window.
toolkit-terminal-pair: runtime-build
    python3 scripts/toolkit-probe.py terminal-pair

# Linux: real IME moves between two terminals sharing one Qt window/input context.
toolkit-terminal-ime-pane-test: runtime-build
    python3 scripts/toolkit-probe.py terminal-ime-pane-test
