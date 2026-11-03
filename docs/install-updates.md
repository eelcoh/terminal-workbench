# Updating an installed Band

Design note (2026-11-03). The owner chose the proposal below; it is
implemented in `crates/runtime/src/payload.rs` and covered by the package
test on Linux. A native Mac run from the copy is still pending.

Band's shells must survive an update. Today they do, but what keeps serving
them afterwards depends on where the new copy lands. This note records what
happens now and proposes one change.

## What happens today

Opening a window runs `band-session open` from the install. It hands the
session service the install's `bin/band-session` as the runtime, and every
terminal's runtime is that binary. Shells get the install's `bin/` on `PATH` as
`BAND_AGENT_BIN`; agent launchers and Copilot's plugin hooks run from there.
`just agent-status` reports both paths per terminal (`origin`).

Running processes keep their code when the files are replaced or deleted: the
installer removes the old copy and renames the new one into place, so the
kernel keeps the old inode. (Overwriting a signed macOS binary in place would
instead get the process killed; neither `just package-install` nor Finder does
that.)

| Update | Terminals and shells | Session service | Agent launchers in old shells |
| --- | --- | --- | --- |
| Same path (macOS `Band.app`, same Linux version) | Keep running, old code | **Old service keeps running** and serves the new window, because the service only hands over when the runtime path or folder changes | New launchers, talking to old runtimes |
| New path (Linux `band-0.2.0` beside `band-0.1.0`) | Keep running, old code | Handed over to the new install if no other window is open | Old launchers, as long as the old folder stays |
| Old path deleted while its shells run | Keep running | — | **`band-copilot` and the others are gone**; `PATH` points at a missing folder |
| Copy briefly absent during the swap | — | — | A launcher started in that moment fails |

So there are three gaps:

1. **The service is not updated** after a same-path update. The new window
   talks to the old service until it exits (logout, reboot, `just
   service-stop`). That works only while the protocol is unchanged, and a fix in
   the service does not take effect.
2. **Old shells lose their launchers** when their install is deleted. The docs
   currently say to keep the old install until its shells exit, which nobody can
   tell from the outside.
3. **Mixed versions are untested.** A new `band-agent-hook` reporting to an old
   runtime, or a new viewer on an old service, has no test. Both only work
   because nothing has changed yet.

## Proposal: run from a copy Band owns

When a window opens, copy the install's `bin/` and `integrations/` (about 4 MB;
the Qt client stays in the install) into
`~/.local/state/band/payloads/<build-id>/`, where `<build-id>` is the source
revision and version from `build-info.json`. Copy into a temporary folder and
rename it, so a half-written copy is never used. Then start the service,
runtimes and `BAND_AGENT_BIN` from that copy instead of from the install.

- Each shell keeps working launchers for as long as it runs, whatever happens
  to the app (gap 2). Deleting or moving `Band.app` is safe.
- A different build gets a different path, so the existing handover replaces
  the old service whenever no other window is open (gap 1). This needs no new
  logic.
- On open, delete payload copies that no running runtime or service uses.
  `origin.runtime_binary` already reports which copy each runtime uses.
- The swap window disappears, because nothing runs from the install after the
  launch step.

Costs and risks:

- About 4 MB per build still in use, usually one or two.
- On macOS the copied `band-session` keeps its ad-hoc signature. A copy made by
  Band is not quarantined. This needs a native check before it ships.
- Restarting a stopped terminal (**Start all stopped shells**) uses the current
  service's copy, so it gets the new runtime. That is the same as today.
- Shells that were already running before this change still point into the
  install. They move over on their next restart.

Gap 3 stays open with either approach. The proposal reduces mixing to one case:
an old runtime receiving reports from its own launchers stays consistent. A
new viewer on an old service only happens while another window keeps the old
service alive. The viewer should refuse an older protocol version clearly
rather than misbehave.

## Alternatives considered

- **Keep as is and document.** No code, but gap 1 means a service fix silently
  does not apply, and gap 2 stays a trap.
- **Symlinks in a stable folder** (`~/.local/state/band/bin` → current install).
  This fixes `PATH` for launchers, but an old runtime would then get a new hook
  binary, and deleting the install still breaks the links. It does not touch
  gap 1.
- **Compare build identity in the service** and hand over on a same-path update
  without copying. This fixes gap 1 only; old shells still depend on the
  install's files.

## Recommendation

Run from an owned copy. It is one change to `band-session open` plus cleanup,
and it reuses the existing handover. Tests:

- Packaging: replace the install, delete the old one, and check that an old
  shell's `band-copilot` still runs and the service is handed over.
- Unit: cleanup keeps every copy still in use.
- Native: Mac signature and launch check from the copy.
