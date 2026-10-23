config-path: runtime-build
    target/debug/band-session config-path

config-init: runtime-build
    target/debug/band-session config-path --init

default:
    @just --list

fmt:
    cargo fmt --all

# Finds Zig 0.16.0 locally/on PATH, or uses an explicit ZIG executable.
runtime-build:
    python3 scripts/build-runtime.py

check: runtime-build
    cargo fmt --all -- --check
    cargo clippy --workspace --all-targets -- -D warnings
    cargo test --workspace
    cargo build --locked --bin band-session
    python3 tests/session_smoke.py target/debug/band-session
    python3 tests/login_shell_test.py target/debug/band-session
    python3 tests/input_burst_test.py target/debug/band-session
    python3 tests/context_smoke.py target/debug/band-session
    python3 tests/terminal_core_smoke.py target/debug/band-session
    python3 tests/snapshot_attach_smoke.py target/debug/band-session
    python3 tests/catalog_journal_test.py target/debug/band-session
    python3 tests/session_shutdown_test.py target/debug/band-session
    python3 tests/session_soak.py target/debug/band-session --seconds 3 --terminals 1 --report .local/test-results/session-soak-check.json
    python3 tests/session_soak_interrupt_test.py target/debug/band-session
    python3 tests/socket_path_test.py target/debug/band-session
    python3 tests/close_smoke.py target/debug/band-session
    python3 tests/agent_smoke.py target/debug/band-session
    python3 tests/attention_smoke.py target/debug/band-session
    python3 tests/pi_runtime_test.py target/debug/band-session
    python3 tests/agent_watch_test.py target/debug/band-session
    python3 tests/copilot_observer_test.py target/debug/band-session
    python3 scripts/prepare-theme-catalog.py --check
    python3 tests/core_headers_test.py
    python3 tests/terminal_search_vt_test.py
    python3 tests/config_path_test.py target/debug/band-session
    python3 tests/build_env_test.py
    python3 tests/recovery_test.py target/debug/band-session
    python3 tests/session_service_test.py target/debug/band-session
    python3 tests/service_startup_cleanup_test.py
    python3 tests/service_startup_cleanup_test.py
    python3 tests/lifecycle_smoke.py target/debug/band-session
    python3 tests/terminal_dismiss_test.py target/debug/band-session
    python3 tests/workspace_directories_test.py target/debug/band-session
    python3 tests/window_slots_test.py

# Runtime PTY, context, VT core and snapshot smoke tests without a display.
runtime-test: runtime-build
    cargo build --locked --bin band-session
    python3 tests/session_smoke.py target/debug/band-session
    python3 tests/context_smoke.py target/debug/band-session
    python3 tests/terminal_core_smoke.py target/debug/band-session
    python3 tests/snapshot_attach_smoke.py target/debug/band-session

# Build the Qt client in the checkout (no window).
toolkit-client: runtime-build
    python3 scripts/toolkit-probe.py client

# Portable fixtures for the Mac deployment tool wrapper; no native Mac result is implied.
package-tools-test:
    python3 tests/macos_packaging_tools_test.py

# Release artifact for this platform in dist/ (Band.app zip or Linux tar.gz).
package:
    python3 scripts/package.py

# Relocation, user state, agent launchers and Qt start of the Linux archive.
package-test: package
    python3 tests/package_test.py

# Explicitly recover dead session endpoints after a reboot or runtime crash.
recover-sessions:
    python3 scripts/session_service.py recover-all --state-dir .local/sessions --runtime target/debug/band-session --cwd .

# Runtime VT parsing cardinality, detached, fixed geometry (Linux diagnostic only).
runtime-core-scale: runtime-build
    python3 tests/runtime_core_scale.py target/debug/band-session

# Inspect runtime health without attaching to or restarting terminals.
session-status:
    python3 scripts/session_service.py status --state-dir .local/sessions --runtime target/debug/band-session --cwd .

# Read agent identity/status and viewer metadata; never restart or attach terminals.
agent-status:
    python3 scripts/agent_status.py

# Capture delivered lifecycle transitions without attaching or changing sessions.
agent-watch seconds="120":
    python3 scripts/agent_watch.py --seconds "{{seconds}}"

# Restart only an explicitly selected stopped terminal; preserves identity/themes.
recover-terminal id:
    python3 scripts/session_service.py recover --state-dir .local/sessions --runtime target/debug/band-session --cwd . --id "{{id}}"

# Stops the organization service only. Live terminals and windows continue.
service-stop:
    python3 scripts/session_service.py stop --state-dir .local/sessions --runtime target/debug/band-session --cwd .

# Sustained runtime/ownership/service checks in disposable sessions (no GUI).
session-soak seconds="300" terminals="15": runtime-build
    python3 tests/session_soak.py target/debug/band-session --seconds "{{seconds}}" --terminals "{{terminals}}"

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

# Mac/Linux: bounded legacy replay and snapshot failure/ownership isolation.
toolkit-terminal-legacy-test: runtime-build
    python3 scripts/toolkit-probe.py terminal-legacy-test

# Mac/Linux: two disposable editable terminals in one Qt window.
toolkit-terminal-pair: runtime-build
    python3 scripts/toolkit-probe.py terminal-pair

# Linux: real IME moves between two terminals sharing one Qt window/input context.
toolkit-terminal-ime-pane-test: runtime-build
    python3 scripts/toolkit-probe.py terminal-ime-pane-test

# Mac/Linux: real runtime/PTY + Qt client CPU/RSS with 1/15 terminals.
toolkit-live-perf: runtime-build
    python3 scripts/toolkit-probe.py live-perf

# Linux: same real-runtime workload on private Xvfb, software GL at 1x/2x.
toolkit-live-perf-test: runtime-build
    python3 scripts/toolkit-probe.py live-perf-test

# Mac/Linux: approved chrome direction around six disposable live shells.
toolkit-ui: runtime-build
    python3 scripts/toolkit-probe.py ui

# Mac/Linux: Qt candidate on the existing saved catalog; closing detaches shells.
toolkit-workbench: runtime-build
    python3 scripts/toolkit-probe.py workbench

# Opt-in font/display metadata for a controlled native screen comparison.
toolkit-display-report: runtime-build
    BAND_DISPLAY_REPORT="{{justfile_directory()}}/.local/artifacts/qt-display/report.json" python3 scripts/toolkit-probe.py workbench

# Linux: confirmed stop-all and peer status with real disposable terminals.
toolkit-shutdown-gui-test: runtime-build
    python3 scripts/toolkit-probe.py ui-test --shutdown-only

# Linux: focused native terminal search on X11 and Wayland.
toolkit-search-gui-test: runtime-build
    python3 scripts/toolkit-probe.py ui-test --search-only

# Mac/Linux: retained terminal search and viewport lifecycle without a display.
toolkit-search-test: runtime-build
    python3 scripts/toolkit-probe.py search-test

# Mac/Linux: fractional vertical scrolling and system-direction policy.
toolkit-scroll-test: runtime-build
    python3 scripts/toolkit-probe.py scroll-test

# Mac/Linux: read-only Qt catalog watcher and failure isolation.
toolkit-catalog-test: runtime-build
    python3 scripts/toolkit-probe.py catalog-test

# Mac/Linux: typed client view persistence without a display.
toolkit-view-state-test: runtime-build
    python3 scripts/toolkit-probe.py view-state-test

# Mac/Linux: saved launcher lifecycle/reattachment without a display.
toolkit-saved-session-test: runtime-build
    python3 tests/qt_saved_launcher_test.py target/debug/band-session

# Linux: native input/focus and stable PTY geometry across UI transitions.
toolkit-ui-test: runtime-build
    python3 scripts/toolkit-probe.py ui-test

# Mac/Linux: offline theme store and deterministic TerminalColors download behavior.
toolkit-theme-import-test: runtime-build
    python3 scripts/toolkit-probe.py theme-import-test

# Read-only runtime metadata adapter contracts (no display required).
toolkit-runtime-facts-test: runtime-build
    python3 scripts/toolkit-probe.py runtime-facts-test

# Read-only lifecycle status contracts, without a GUI.
toolkit-lifecycle-test: runtime-build
    python3 scripts/toolkit-probe.py lifecycle-test

# Read-only dotfiles and window-local Qt font transactions, without a display.
toolkit-fonts-test: runtime-build
    python3 scripts/toolkit-probe.py fonts-test

# Managed Qt application chords, platform mapping and conflict contracts.
toolkit-shortcuts-test: runtime-build
    python3 scripts/toolkit-probe.py shortcuts-test

# Mac/Linux: real zsh login startup under a minimal desktop environment.
login-shell-test: runtime-build
    python3 tests/login_shell_test.py target/debug/band-session

# Exercise Qt's real import-deployment API; no native Mac result is implied.
toolkit-qml-deployment-test: toolkit-client
    cmake --build .local/build/qt-input --target workbench-qml-deployment-probe --parallel
    python3 tests/qt_qml_deployment_test.py

# Client-only About identity, diagnostic allowlist and clipboard contract.
toolkit-about-test: toolkit-client
    cmake --build .local/build/qt-input --target workbench-build-info-test --parallel
    QT_QPA_PLATFORM=offscreen .local/build/qt-input/workbench-build-info-test

# Transactional settings reload, peer-window propagation and file-replacement watching.
toolkit-configuration-test: toolkit-client
    cmake --build .local/build/qt-input --target workbench-configuration-test --parallel
    QT_QPA_PLATFORM=offscreen .local/build/qt-input/workbench-configuration-test

# Linux: real PTY/input/grid invariants during live configuration updates.
toolkit-configuration-ui-test: runtime-build
    python3 scripts/toolkit-probe.py ui-test --configuration-only

# Shared client workspace palettes, concurrent edits and live peer notifications.
toolkit-workspace-appearance-test: toolkit-client
    cmake --build .local/build/qt-input --target workbench-workspace-appearance-test --parallel
    QT_QPA_PLATFORM=offscreen .local/build/qt-input/workbench-workspace-appearance-test

# Read-only agent explanations and allowlisted launcher clipboard actions.
toolkit-agent-info-test: toolkit-client
    cmake --build .local/build/qt-input --target workbench-agent-info-test --parallel
    QT_QPA_PLATFORM=offscreen .local/build/qt-input/workbench-agent-info-test

# Runtime contracts for Pi and older metadata clients.
pi-runtime-test: runtime-build
    python3 tests/pi_runtime_test.py target/debug/band-session

# Optional integration check: Node 22+ TypeScript loader, no Pi/account needed.
pi-observer-test:
    node --experimental-strip-types tests/pi_observer_test.mjs

# Metadata-only exited-pane removal and live/recovery race protection.
terminal-dismiss-test: runtime-build
    python3 tests/terminal_dismiss_test.py target/debug/band-session

# Read-only per-terminal runtime diagnostics, parser bounds and cancellation.
toolkit-runtime-info-test: toolkit-client
    cmake --build .local/build/qt-input --target workbench-runtime-info-test --parallel
    QT_QPA_PLATFORM=offscreen .local/build/qt-input/workbench-runtime-info-test
