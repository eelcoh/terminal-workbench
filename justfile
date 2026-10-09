default:
    @just --list

fmt:
    cargo fmt --all

check:
    cargo fmt --all -- --check
    cargo clippy --workspace --all-targets -- -D warnings
    cargo test --workspace
    cargo build --locked --bin workbench-session
    python3 tests/session_smoke.py target/debug/workbench-session
    python3 tests/catalog_test.py
    python3 tests/move_test.py
    python3 tests/close_smoke.py target/debug/workbench-session
    python3 tests/agent_smoke.py target/debug/workbench-session
    python3 tests/attention_smoke.py target/debug/workbench-session
    python3 tests/overview_animation_test.py

# Terminal renderer experiment; requires CMake, Ninja and Zig 0.16.x.
spike-build:
    python3 scripts/prepare-agent-launchers.py
    python3 scripts/prepare-ghostling.py
    cmake -S .local/ghostling-viewer -B .local/build/viewer -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build .local/build/viewer --parallel 2
    cargo build --locked --bin workbench-session

spike-run socket:
    WORKBENCH_SOCKET="{{socket}}" .local/build/viewer/ghostling

spike-test:
    cargo build --locked --bin workbench-session
    python3 tests/session_smoke.py target/debug/workbench-session

spike-gui-test: spike-build
    python3 tests/gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

workspace-gui-test: spike-build
    python3 tests/workspace_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

management-gui-test: spike-build
    python3 tests/management_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

expose-gui-test: spike-build
    python3 tests/expose_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling
    WORKBENCH_REDUCED_MOTION=1 python3 tests/expose_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

move-gui-test: spike-build
    python3 tests/move_gui_smoke.py target/debug/workbench-session .local/build/viewer/ghostling

# Start/reuse the workspace sessions and attach a desktop window.
spike: spike-build
    python3 scripts/run-spike.py
