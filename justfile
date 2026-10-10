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
    python3 tests/terminal_core_smoke.py target/debug/workbench-session
    python3 tests/snapshot_attach_smoke.py target/debug/workbench-session
    python3 tests/catalog_test.py
    python3 tests/socket_path_test.py target/debug/workbench-session
    python3 tests/move_test.py
    python3 tests/close_smoke.py target/debug/workbench-session
    python3 tests/agent_smoke.py target/debug/workbench-session
    python3 tests/attention_smoke.py target/debug/workbench-session
    python3 scripts/prepare-theme-catalog.py --check
    python3 tests/overview_animation_test.py
    python3 tests/build_env_test.py
    python3 tests/recovery_test.py target/debug/workbench-session
    python3 tests/launch_diagnostics_test.py

# Finds Zig 0.16.0 locally/on PATH, or uses an explicit ZIG executable.
spike-build:
    python3 scripts/build-spike.py

spike-run socket:
    WORKBENCH_SOCKET="{{socket}}" .local/build/viewer/ghostling

spike-test: runtime-build
    cargo build --locked --bin workbench-session
    python3 tests/session_smoke.py target/debug/workbench-session
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
