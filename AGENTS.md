# Terminal Workbench

This is an independent desktop application for macOS and Linux. The adjacent
Herdr repository is reference material, not the governing project.

- Keep both platforms in scope. Platform-specific implementations belong in
  platform adapters or native app directories.
- Keep runtime/session facts separate from client presentation and input state.
- Design shortcuts afresh. Do not import Herdr bindings or shortcut assumptions.
- Use existing terminal technology; do not build a new emulator as part of UI work.
- Before adopting a renderer, prove keyboard input, PTY ownership, resize,
  selection, scrolling, and clipboard integration on both targets.
- Preserve minimal visual chrome and direct access to terminals.
- Do not copy upstream code without checking its license and preserving required
  notices and attribution. Choose the project's license with the owner before
  publishing or distributing code.
- Use SSH for GitHub Git operations and dependency fetches. Do not use GitHub
  HTTPS clones or archive downloads. Keep this preference in build scripts too.
- Add dependencies only for a concrete implementation need.
- Use `just check` for Rust validation and report platform checks separately.
- On this workstation, run builds and install build dependencies inside the
  `build-machine` Distrobox. Rust is intentionally absent from the host. Use
  `distrobox enter build-machine -- just --working-directory /var/home/eelco/Code/ideas/terminal-workbench --justfile /var/home/eelco/Code/ideas/terminal-workbench/justfile check`.
- Describe scaffolding and unverified integrations accurately.
- Get alignment on the commit message before committing. Do not publish a remote
  repository or push without authorization.
